/**
 * @file test_arrivi_schedulati.c
 * @brief Test Unity per controllore_schedulaArrivo/retryArriviSchedulati
 *        (vedi Controllore.h): arrivi esterni che entrano nella cella
 *        solo al proprio arrival_step, invece che subito.
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
    g_ctrl = controllore_create( g_cell, 0.8, &err );
}

void tearDown( void )
{
    /* Libera eventuali object_t rimasti in B1 (mai processati oltre
     * l'ingresso, in questi test non c'e' nessuna pipeline a valle). Gli
     * oggetti ancora nella coda arriviSchedulati (mai raggiunto il loro
     * turno) sono invece liberati automaticamente da controllore_destroy
     * (vedi doc di controllore_schedulaArrivo) - nessuna azione manuale
     * necessaria per quelli. */
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

/* ------------------------------------------------------------------ */
/*  Arrivo futuro: non entra prima del proprio step                    */
/* ------------------------------------------------------------------ */

void test_arrivo_futuro_non_entra_prima_del_suo_step( void )
{
    short int err;
    object_t *obj = object_create( "P1", 5, 'A', 10, 100.0, 10.0, &err );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, controllore_schedulaArrivo( g_ctrl, "B1", obj, 10 ) );
    TEST_ASSERT_EQUAL_INT( 1, controllore_getArriviSchedulatiCount( g_ctrl ) );

    /* Step 0..9: non deve ancora essere entrato. */
    int step;
    for ( step = 0; step < 10; step++ ) {
        controllore_step( g_ctrl, step );
        TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "B1" ) ) );
    }

    /* Step 10: deve entrare esattamente ora. */
    controllore_step( g_ctrl, 10 );
    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "B1" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, controllore_getArriviSchedulatiCount( g_ctrl ) );
}

void test_arrivo_a_step_zero_entra_al_primo_controllore_step( void )
{
    short int err;
    object_t *obj = object_create( "P1", 5, 'A', 0, 100.0, 10.0, &err );

    controllore_schedulaArrivo( g_ctrl, "B1", obj, 0 );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "B1" ) ) );  /* non ancora, solo schedulato */

    controllore_step( g_ctrl, 0 );
    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "B1" ) ) );
}

/* ------------------------------------------------------------------ */
/*  Buffer pieno al momento giusto: ritenta, non perde l'oggetto        */
/* ------------------------------------------------------------------ */

void test_buffer_pieno_al_momento_giusto_ritenta_finche_non_entra( void )
{
    short int err;
    int i;

    /* Riempio B1 (capacita' 5) con 5 oggetti gia' presenti. */
    for ( i = 0; i < 5; i++ ) {
        char id[8];
        snprintf( id, sizeof( id ), "F%d", i );
        object_t *filler = object_create( id, 1, 'A', 0, 100.0, 10.0, &err );
        controllore_ammettiArrivo( g_ctrl, "B1", filler, 0 );
    }
    TEST_ASSERT_TRUE( buffer_isFull( cell_getBuffer( g_cell, "B1" ) ) );

    /* Sesto oggetto, schedulato per lo step 2 - B1 e' pieno in quel momento. */
    object_t *obj = object_create( "P1", 9, 'A', 2, 100.0, 10.0, &err );
    controllore_schedulaArrivo( g_ctrl, "B1", obj, 2 );

    controllore_step( g_ctrl, 0 );
    controllore_step( g_ctrl, 1 );
    controllore_step( g_ctrl, 2 );

    /* Ancora in coda: B1 era pieno al passo 2. */
    TEST_ASSERT_EQUAL_INT( 1, controllore_getArriviSchedulatiCount( g_ctrl ) );

    /* Libero un posto rimuovendo un filler (simula smaltimento a valle). */
    object_delete( buffer_removeObject( cell_getBuffer( g_cell, "B1" ), true ) );

    controllore_step( g_ctrl, 3 );

    /* Ora deve essere entrato: ritentato automaticamente, non perso. */
    TEST_ASSERT_EQUAL_INT( 0, controllore_getArriviSchedulatiCount( g_ctrl ) );
    TEST_ASSERT_EQUAL_INT( 5, buffer_getCount( cell_getBuffer( g_cell, "B1" ) ) );  /* di nuovo pieno: 4 filler + P1 */
}

/* ------------------------------------------------------------------ */
/*  Nessun leak per arrivi mai raggiunti (simulazione troncata)         */
/* ------------------------------------------------------------------ */

void test_arrivo_mai_raggiunto_non_fa_leak_su_destroy( void )
{
    /* Non verificabile direttamente con Unity (serve un sanitizer
     * esterno, vedi test/run_tests.sh + ASan nel README), ma il test
     * documenta il caso e verifica almeno che il conteggio sia corretto
     * prima della distruzione - la vera prova "nessun leak" e' la corsa
     * sotto AddressSanitizer, non un assert Unity. */
    short int err;
    object_t *obj = object_create( "MaiArrivato", 5, 'A', 1000, 100.0, 10.0, &err );

    controllore_schedulaArrivo( g_ctrl, "B1", obj, 1000 );  /* mai raggiunto in questo test */
    TEST_ASSERT_EQUAL_INT( 1, controllore_getArriviSchedulatiCount( g_ctrl ) );

    controllore_step( g_ctrl, 0 );  /* molto prima di 1000: resta schedulato */
    TEST_ASSERT_EQUAL_INT( 1, controllore_getArriviSchedulatiCount( g_ctrl ) );

    /* tearDown chiama controllore_destroy qui: se liberasse male la coda
     * arriviSchedulati, un sanitizer lo segnalerebbe girando questo test. */
}

/* ------------------------------------------------------------------ */
/*  Puntatori NULL                                                     */
/* ------------------------------------------------------------------ */

void test_schedulaArrivo_su_null_restituisce_errore( void )
{
    short int err;
    object_t *obj = object_create( "P1", 5, 'A', 0, 100.0, 10.0, &err );

    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, controllore_schedulaArrivo( NULL, "B1", obj, 0 ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, controllore_schedulaArrivo( g_ctrl, NULL, obj, 0 ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, controllore_schedulaArrivo( g_ctrl, "B1", NULL, 0 ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, controllore_getArriviSchedulatiCount( NULL ) );

    object_delete( obj );
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_arrivo_futuro_non_entra_prima_del_suo_step );
    RUN_TEST( test_arrivo_a_step_zero_entra_al_primo_controllore_step );

    RUN_TEST( test_buffer_pieno_al_momento_giusto_ritenta_finche_non_entra );

    RUN_TEST( test_arrivo_mai_raggiunto_non_fa_leak_su_destroy );

    RUN_TEST( test_schedulaArrivo_su_null_restituisce_errore );

    return UNITY_END();
}
