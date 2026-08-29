/**
 * @file test_isp_guasto.c
 * @brief Test Unity per la regola "guasto sensore qualita' = ISP
 *        indisponibile" (vedi processISP/genericIsAvailable in
 *        Controllore.c, e la sezione dedicata nel README).
 *
 * Mini-cella B1 -> ISP1 -> B_OUT (con sensore di qualita' e guasto
 * agganciati a ISP1), nessuna uscita multipla/Deviatore: basta a
 * isolare il comportamento senza dover costruire l'impianto completo.
 */

#include "unity.h"
#include "cell.h"
#include "Controllore.h"
#include "object.h"
#include "errors.h"

static cell_t *g_cell;
static controllore_t *g_ctrl;

void setUp( void )
{
    short int err;

    g_cell = cell_create();
    cell_addBuffer( g_cell, "B1", 5, &err );
    cell_addISP( g_cell, "ISP1", 2, &err );      /* tempo_controllo = 2 passi */
    cell_addBuffer( g_cell, "B_OUT", 5, &err );  /* uscita di ISP1: senza questa, un pezzo
                                                   * rilasciato non avrebbe dove andare e
                                                   * si perderebbe (leak scoperto scrivendo
                                                   * questo test - vedi tearDown). */
    cell_connect( g_cell, "B1", "ISP1" );
    cell_connect( g_cell, "ISP1", "B_OUT" );

    g_ctrl = controllore_create( g_cell, 0.8, &err );
    controllore_collegaSensoreQualita( g_ctrl, "ISP1", 100, 10 );
}

void tearDown( void )
{
    /* Libera eventuali object_t arrivati in B_OUT (vedi setUp) e un
     * eventuale oggetto ancora in B1 (non entrato in ISP1). Un oggetto
     * ancora "in controllo" dentro ISP1 stessa (trattenuto per guasto)
     * NON è invece recuperabile da qui: isp.h non espone un getter per
     * l'oggetto in controllo, quindi ogni test che lo trattiene per
     * guasto deve farlo uscire esplicitamente (disattivando il guasto e
     * facendo avanzare la simulazione) PRIMA che tearDown giri. */
    buffer_t *b_out = cell_getBuffer( g_cell, "B_OUT" );
    if ( b_out != NULL ) {
        bufferObj_t *cur = b_out->head;
        while ( cur != NULL ) { object_delete( cur->dato ); cur = cur->next; }
    }
    buffer_t *b1 = cell_getBuffer( g_cell, "B1" );
    if ( b1 != NULL ) {
        bufferObj_t *cur = b1->head;
        while ( cur != NULL ) { object_delete( cur->dato ); cur = cur->next; }
    }

    controllore_destroy( g_ctrl );
    cell_destroy( g_cell );
    g_cell = NULL;
    g_ctrl = NULL;
}

static object_t *inserisci_un_oggetto_in_B1( void )
{
    short int err;
    object_t *obj = object_create( "P1", 5, 'A', 0, 100.0, 10.0, &err );  /* dentro target: CONFORME atteso */
    controllore_ammettiArrivo( g_ctrl, "B1", obj, 0 );
    return obj;
}

/* ------------------------------------------------------------------ */
/*  Senza guasto: comportamento invariato (baseline)                   */
/* ------------------------------------------------------------------ */

void test_senza_guasto_isp_rilascia_normalmente( void )
{
    object_t *obj = inserisci_un_oggetto_in_B1();
    int step;

    /* step 0: B1->ISP1 (ammissione), step 1: ISP1 in controllo,
     * step 2: tempo_controllo scaduto (2 passi), rilascia. */
    for ( step = 0; step < 5; step++ ) {
        controllore_step( g_ctrl, step );
    }

    TEST_ASSERT_EQUAL_INT( 0, controllore_getStepBloccoGuasto( g_ctrl, "ISP1" ) );
    TEST_ASSERT_EQUAL( 1, controllore_getLettureQualita( g_ctrl, "ISP1" ) );

    /* Il pezzo e' ormai arrivato in B_OUT: lo libera tearDown. */
    (void) obj;
}

/* ------------------------------------------------------------------ */
/*  Con guasto: la ISP NON accetta nuovi pezzi                         */
/* ------------------------------------------------------------------ */

void test_con_guasto_attivo_isp_non_accetta_pezzi( void )
{
    /* Guasto sempre attivo fin dall'inizio: time_error=1 (dopo 1 passo
     * di "OK" iniziale entra in guasto), time_ok molto lungo (non
     * rientra mai entro i pochi passi del test). */
    controllore_impostaGuastoQualita( g_ctrl, "ISP1", true, 1, 1000 );

    /* Passo "di riscaldamento" a cella vuota: al passo 0 il guasto non
     * e' ancora scattato (update_status parte da time_since_last_change=0,
     * serve che il tempo corrente raggiunga time_error=1 - vedi
     * S_Qualita.c). Senza questo passo il pezzo entrerebbe comunque al
     * passo 0, PRIMA che il guasto sia rilevabile (scoperto proprio
     * scrivendo questo test: la prima versione falliva per questo). */
    controllore_step( g_ctrl, 0 );

    object_t *obj = inserisci_un_oggetto_in_B1();
    int step;

    for ( step = 1; step < 6; step++ ) {
        controllore_step( g_ctrl, step );
    }

    /* Il pezzo non deve mai essere entrato in ISP1: resta in B1, dove
     * lo libera tearDown. */
    buffer_t *b1 = cell_getBuffer( g_cell, "B1" );
    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( b1 ) );
    TEST_ASSERT_EQUAL_INT( 0, controllore_getLettureQualita( g_ctrl, "ISP1" ) );
    (void) obj;
}

/* ------------------------------------------------------------------ */
/*  Con guasto: la ISP TRATTIENE il pezzo che sta gia' controllando     */
/* ------------------------------------------------------------------ */

void test_con_guasto_attivo_isp_trattiene_pezzo_in_controllo( void )
{
    /* Il pezzo entra SUBITO (guasto non ancora attivo al passo 0),
     * poi il guasto scatta PRIMA che il tempo di controllo (2 passi)
     * sia scaduto: il pezzo deve restare bloccato dentro, non uscire. */
    controllore_impostaGuastoQualita( g_ctrl, "ISP1", true, 1, 1000 );

    object_t *obj = inserisci_un_oggetto_in_B1();
    int step;

    /* step 0: ammissione B1->ISP1 (il sensore e' ancora OK al passo 0,
     * quindi genericIsAvailable lo accetta). Dal passo 1 in poi il
     * guasto e' attivo (time_error=1). */
    for ( step = 0; step < 10; step++ ) {
        controllore_step( g_ctrl, step );
    }

    /* Il tempo di controllo (2 passi) e' scaduto da un pezzo, ma il
     * pezzo NON deve essere mai uscito: nessuna lettura qualita'
     * prodotta, e il contatore di blocco deve essere cresciuto. */
    TEST_ASSERT_EQUAL_INT( 0, controllore_getLettureQualita( g_ctrl, "ISP1" ) );
    TEST_ASSERT_GREATER_THAN_INT( 0, controllore_getStepBloccoGuasto( g_ctrl, "ISP1" ) );

    /* Il pezzo non e' ne' in B1 ne' in un buffer a valle: e' ancora
     * "dentro" ISP1 stessa, irraggiungibile dall'esterno (isp.h non
     * espone l'oggetto in controllo). Disattiviamo il guasto e
     * lasciamo che si liberi da solo (arrivando in B_OUT, dove lo
     * libera tearDown) prima della fine del test. */
    controllore_impostaGuastoQualita( g_ctrl, "ISP1", false, 1, 1000 );
    for ( step = 10; step < 15; step++ ) {
        controllore_step( g_ctrl, step );
    }
    TEST_ASSERT_EQUAL_INT( 1, controllore_getLettureQualita( g_ctrl, "ISP1" ) );  /* ora si e' liberato */
    (void) obj;
}

/* ------------------------------------------------------------------ */
/*  Il guasto rientra: torna a comportarsi normalmente                 */
/* ------------------------------------------------------------------ */

void test_guasto_rientra_isp_torna_disponibile( void )
{
    /* Guasto breve: 1 passo OK, 1 passo di guasto, poi torna OK e resta
     * OK abbastanza a lungo da lasciar completare il test. */
    controllore_impostaGuastoQualita( g_ctrl, "ISP1", true, 1, 1 );

    object_t *obj = inserisci_un_oggetto_in_B1();
    int step;

    for ( step = 0; step < 15; step++ ) {
        controllore_step( g_ctrl, step );
    }

    /* Con guasto intermittente breve, prima o poi il pezzo deve
     * riuscire ad essere letto (anche se magari bloccato per qualche
     * passo nel mezzo). Arriva in B_OUT, dove lo libera tearDown. */
    TEST_ASSERT_EQUAL_INT( 1, controllore_getLettureQualita( g_ctrl, "ISP1" ) );
    (void) obj;
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_senza_guasto_isp_rilascia_normalmente );
    RUN_TEST( test_con_guasto_attivo_isp_non_accetta_pezzi );
    RUN_TEST( test_con_guasto_attivo_isp_trattiene_pezzo_in_controllo );
    RUN_TEST( test_guasto_rientra_isp_torna_disponibile );

    return UNITY_END();
}
