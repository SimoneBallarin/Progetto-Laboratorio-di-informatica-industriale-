/**
 * @file test_statistiche.c
 * @brief Test Unity per lib/Statistiche (statistiche.c/.h).
 *
 * Copre soprattutto la scomposizione tempo SISTEMA (coda+processo) vs
 * tempo PROCESSO (solo pipeline, esclusa la coda) appena introdotta per
 * eliminare l'ambiguità sulla verifica della scadenza (vedi
 * statistiche_riepilogo_t in statistiche.h): senza questi test, un
 * refactor futuro potrebbe silenziosamente tornare a confondere le due
 * definizioni di tempo come succedeva prima.
 *
 * statistiche_getRiepilogo richiede un controllore_t non-NULL (usato
 * solo per leggere pending/anomalie qualità): qui viene creato un
 * controllore "minimo" su una cella vuota, dato che i test in questo
 * file non hanno bisogno di nessuna entità reale della cella - vedi
 * setUp/tearDown.
 */

/* Necessaria PRIMA di ogni altro #include: con -std=c11 stretto (vedi
 * build.sh/CMakeLists.txt), fileno/dup/dup2 (usate sotto per catturare
 * temporaneamente stdout) sono estensioni POSIX nascoste di default. */
#define _POSIX_C_SOURCE 200809L

#include "unity.h"
#include "statistiche.h"
#include "object.h"
#include "cell.h"
#include "Controllore.h"
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static cell_t *g_cell;
static controllore_t *g_ctrl;

void setUp( void )
{
    short int err;
    g_cell = cell_create();
    g_ctrl = controllore_create( g_cell, 0.8, &err );
}

void tearDown( void )
{
    controllore_destroy( g_ctrl );
    cell_destroy( g_cell );
    g_cell = NULL;
    g_ctrl = NULL;
}

/* Crea un oggetto "completato": entrato a stepCreation, iniziato il
 * processo a stepPartial, uscito a stepOut. Comodo per non ripetere le
 * tre chiamate in ogni test. */
static object_t *crea_completato( const char *id, short int priorita, int stepCreation, int stepPartial, int stepOut )
{
    short int err;
    object_t *obj = object_create( id, priorita, 'A', stepCreation, 10.0, 5.0, &err );
    object_setStepPartial( obj, stepPartial );
    object_setStepOut( obj, stepOut );
    return obj;
}

/* ------------------------------------------------------------------ */
/*  registraCompletamento: oggetto non ancora uscito viene ignorato    */
/* ------------------------------------------------------------------ */

void test_registraCompletamento_ignora_oggetto_non_uscito( void )
{
    short int err;
    statistiche_t *s = statistiche_create( &err );
    object_t *obj = object_create( "P1", 5, 'A', 0, 10.0, 5.0, &err );  /* stepOut mai impostato */
    statistiche_riepilogo_t r;

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, statistiche_registraCompletamento( s, obj, 40 ) );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, statistiche_getRiepilogo( s, g_ctrl, &r ) );
    TEST_ASSERT_EQUAL_INT( 0, r.totale_completati );

    object_delete( obj );
    statistiche_destroy( s );
}

/* ------------------------------------------------------------------ */
/*  Scomposizione SISTEMA vs PROCESSO - il cuore del modulo             */
/* ------------------------------------------------------------------ */

void test_riepilogo_distingue_tempo_sistema_da_tempo_processo( void )
{
    /* Oggetto con MOLTA attesa in coda ma pipeline breve:
     *   stepCreation=0, stepPartial=30 (30 passi in coda), stepOut=40 (10 di pipeline)
     *   tempo SISTEMA  = 40 - 0  = 40
     *   tempo PROCESSO = 40 - 30 = 10
     * Con scadenza=15: FUORI scadenza sul SISTEMA, ENTRO scadenza sul PROCESSO -
     * esattamente il caso che l'ambiguità del vecchio codice nascondeva. */
    short int err;
    statistiche_t *s = statistiche_create( &err );
    object_t *obj = crea_completato( "P1", 5, 0, 30, 40 );
    statistiche_riepilogo_t r;

    statistiche_registraCompletamento( s, obj, 15 );
    statistiche_getRiepilogo( s, g_ctrl, &r );

    TEST_ASSERT_EQUAL_INT( 1, r.totale_completati );
    TEST_ASSERT_EQUAL_DOUBLE( 40.0, r.tempo_medio_sistema );
    TEST_ASSERT_EQUAL_DOUBLE( 10.0, r.tempo_medio_processo );
    TEST_ASSERT_EQUAL_DOUBLE( 0.0, r.perc_entro_scadenza_tempo_sistema );    /* 40 > 15: fuori */
    TEST_ASSERT_EQUAL_DOUBLE( 100.0, r.perc_entro_scadenza_tempo_processo ); /* 10 <= 15: entro */

    object_delete( obj );
    statistiche_destroy( s );
}

void test_riepilogo_scadenza_esattamente_al_limite_e_entro( void )
{
    /* tempo <= scadenza_passi (non <): il limite esatto conta come
     * "entro scadenza" (vedi statistiche_registraCompletamento). */
    short int err;
    statistiche_t *s = statistiche_create( &err );
    object_t *obj = crea_completato( "P1", 5, 0, 0, 40 );  /* tempo sistema = 40, niente attesa */
    statistiche_riepilogo_t r;

    statistiche_registraCompletamento( s, obj, 40 );
    statistiche_getRiepilogo( s, g_ctrl, &r );

    TEST_ASSERT_EQUAL_DOUBLE( 100.0, r.perc_entro_scadenza_tempo_sistema );

    object_delete( obj );
    statistiche_destroy( s );
}

void test_riepilogo_scadenza_disattivata_con_valore_non_positivo( void )
{
    /* scadenza_passi <= 0 disattiva la statistica (doc in statistiche.h):
     * tutti gli oggetti risultano "fuori scadenza" in entrambe le
     * definizioni, anche se il tempo effettivo e' bassissimo. */
    short int err;
    statistiche_t *s = statistiche_create( &err );
    object_t *obj = crea_completato( "P1", 5, 0, 1, 2 );  /* tempo sistema = 2, minimo possibile */
    statistiche_riepilogo_t r;

    statistiche_registraCompletamento( s, obj, 0 );
    statistiche_getRiepilogo( s, g_ctrl, &r );

    TEST_ASSERT_EQUAL_DOUBLE( 0.0, r.perc_entro_scadenza_tempo_sistema );
    TEST_ASSERT_EQUAL_DOUBLE( 0.0, r.perc_entro_scadenza_tempo_processo );
    TEST_ASSERT_EQUAL_INT( 1, r.totale_completati );  /* pero' resta conteggiato come completato */

    object_delete( obj );
    statistiche_destroy( s );
}

void test_riepilogo_media_pesata_su_piu_oggetti( void )
{
    short int err;
    statistiche_t *s = statistiche_create( &err );
    /* Due oggetti a priorita' bassa (<=3): tempo sistema 10 e 30 -> media 20. */
    object_t *o1 = crea_completato( "P1", 1, 0, 5, 10 );   /* sistema=10, processo=5 */
    object_t *o2 = crea_completato( "P2", 2, 0, 10, 30 );  /* sistema=30, processo=20 */
    statistiche_riepilogo_t r;

    statistiche_registraCompletamento( s, o1, 100 );
    statistiche_registraCompletamento( s, o2, 100 );
    statistiche_getRiepilogo( s, g_ctrl, &r );

    TEST_ASSERT_EQUAL_INT( 2, r.totale_completati );
    TEST_ASSERT_EQUAL_DOUBLE( 20.0, r.tempo_medio_sistema );    /* (10+30)/2 */
    TEST_ASSERT_EQUAL_DOUBLE( 12.5, r.tempo_medio_processo );   /* (5+20)/2 */
    TEST_ASSERT_EQUAL_INT( 2, r.completati_bassa_priorita );    /* priorita' 1 e 2, entrambe <= 3 */
    TEST_ASSERT_EQUAL_INT( 0, r.completati_alta_priorita );     /* nessuna >= 7 */

    object_delete( o1 );
    object_delete( o2 );
    statistiche_destroy( s );
}

/* ------------------------------------------------------------------ */
/*  Blocchi                                                             */
/* ------------------------------------------------------------------ */

void test_registraBlocco_incrementa_il_totale( void )
{
    short int err;
    statistiche_t *s = statistiche_create( &err );
    statistiche_riepilogo_t r;

    statistiche_registraBlocco( s, "B1" );
    statistiche_registraBlocco( s, "B1" );
    statistiche_registraBlocco( s, "B_non_monitorato" );

    statistiche_getRiepilogo( s, g_ctrl, &r );
    TEST_ASSERT_EQUAL_INT( 3, r.totale_blocchi );  /* contati anche i buffer non monitorati esplicitamente */

    statistiche_destroy( s );
}

/* ------------------------------------------------------------------ */
/*  Puntatori NULL                                                     */
/* ------------------------------------------------------------------ */

void test_getRiepilogo_su_null_restituisce_errore( void )
{
    statistiche_riepilogo_t r;
    short int err;
    statistiche_t *s = statistiche_create( &err );

    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, statistiche_getRiepilogo( NULL, g_ctrl, &r ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, statistiche_getRiepilogo( s, NULL, &r ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, statistiche_getRiepilogo( s, g_ctrl, NULL ) );

    statistiche_destroy( s );
}

/* ------------------------------------------------------------------ */
/*  statistiche_stampa: anche_su_stdout scrive sempre su file, ma su   */
/*  stdout solo se richiesto (test di regressione: vedi bug corretto   */
/*  "riepilogo stampato due volte" nel README, causato da app/main.c   */
/*  che chiamava questa funzione due volte, una per strategia, senza   */
/*  nessun modo di sopprimere la stampa a schermo della seconda run)   */
/* ------------------------------------------------------------------ */

/* Legge l'intero contenuto di un file in un buffer allocato con
 * malloc (il chiamante deve fare free). Ritorna NULL se il file non
 * esiste o e' vuoto. */
static char *leggi_intero_file( const char *path )
{
    FILE *f = fopen( path, "r" );
    if ( f == NULL ) { return NULL; }

    fseek( f, 0, SEEK_END );
    long size = ftell( f );
    fseek( f, 0, SEEK_SET );
    if ( size <= 0 ) { fclose( f ); return NULL; }

    char *buf = malloc( (size_t) size + 1 );
    size_t letti = fread( buf, 1, (size_t) size, f );
    buf[letti] = '\0';
    fclose( f );
    return buf;
}

void test_statistiche_stampa_scrive_sempre_su_file( void )
{
    /* anche_su_stdout=false: il file deve comunque contenere il
     * riepilogo completo (solo la console va soppressa, non il file -
     * altrimenti i dati della run andrebbero persi, non solo non
     * stampati). */
    const char *path = "test_stampa_no_stdout.tmp";
    short int err;
    statistiche_t *s = statistiche_create( &err );

    statistiche_stampa( s, g_ctrl, 100, path, false );

    char *contenuto = leggi_intero_file( path );
    TEST_ASSERT_NOT_NULL( contenuto );
    TEST_ASSERT_NOT_NULL( strstr( contenuto, "=== STATISTICHE ===" ) );

    free( contenuto );
    remove( path );
    statistiche_destroy( s );
}

void test_statistiche_stampa_con_anche_su_stdout_scrive_anche_su_stdout( void )
{
    /* Cattura stdout (via dup/dup2, cosi' la destinazione originale si
     * puo' ripristinare esattamente anche se non c'e' un terminale
     * reale, es. in CI) per verificare che anche_su_stdout=true produca
     * EFFETTIVAMENTE output a schermo (non solo che non crashi): senza
     * questo controllo, un futuro refactor potrebbe rompere
     * silenziosamente il ramo "true". */
    const char *stdout_path = "test_stampa_stdout_catturato.tmp";
    short int err;
    statistiche_t *s = statistiche_create( &err );

    fflush( stdout );
    int fd_originale = dup( fileno( stdout ) );
    TEST_ASSERT_TRUE( fd_originale >= 0 );
    FILE *catturato_stream = freopen( stdout_path, "w", stdout );
    TEST_ASSERT_NOT_NULL( catturato_stream );

    statistiche_stampa( s, g_ctrl, 100, NULL, true );

    fflush( stdout );
    dup2( fd_originale, fileno( stdout ) );
    close( fd_originale );
    clearerr( stdout );

    char *catturato = leggi_intero_file( stdout_path );
    TEST_ASSERT_NOT_NULL( catturato );
    TEST_ASSERT_NOT_NULL( strstr( catturato, "=== STATISTICHE ===" ) );

    free( catturato );
    remove( stdout_path );
    statistiche_destroy( s );
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_registraCompletamento_ignora_oggetto_non_uscito );

    RUN_TEST( test_riepilogo_distingue_tempo_sistema_da_tempo_processo );
    RUN_TEST( test_riepilogo_scadenza_esattamente_al_limite_e_entro );
    RUN_TEST( test_riepilogo_scadenza_disattivata_con_valore_non_positivo );
    RUN_TEST( test_riepilogo_media_pesata_su_piu_oggetti );

    RUN_TEST( test_registraBlocco_incrementa_il_totale );

    RUN_TEST( test_getRiepilogo_su_null_restituisce_errore );

    RUN_TEST( test_statistiche_stampa_scrive_sempre_su_file );
    RUN_TEST( test_statistiche_stampa_con_anche_su_stdout_scrive_anche_su_stdout );

    return UNITY_END();
}
