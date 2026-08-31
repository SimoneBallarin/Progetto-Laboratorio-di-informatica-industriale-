/**
 * @file test_machine.c
 * @brief Test Unity per lib/Macchina (machine.c/.h).
 *
 * In particolare copre machine_tryRelease e il rumore casuale di
 * lavorazione (vedi fattore_rumore, statica in machine.c, esercitata
 * solo indirettamente da qui): prima di questo file il modulo non aveva
 * NESSUNA copertura di test, ed e' esattamente per questo che un bug
 * nel generatore di rumore (uno scarto di +/- 5 volte la tolleranza nel
 * 20% dei casi, invece di restare entro la tolleranza dichiarata) era
 * passato inosservato. test_tryRelease_rumore_resta_entro_la_tolleranza
 * e' un test di regressione mirato proprio su quel bug.
 */

#include "unity.h"
#include "machine.h"
#include "object.h"
#include "errors.h"

void setUp( void ) {}
void tearDown( void ) {}

/* ------------------------------------------------------------------ */
/*  Creazione                                                          */
/* ------------------------------------------------------------------ */

void test_machine_create_valida( void )
{
    short int err;
    machine_t *m = machine_create( "M1", 3, &err );

    TEST_ASSERT_NOT_NULL( m );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, err );
    TEST_ASSERT_EQUAL_STRING( "M1", machine_getID( m ) );
    TEST_ASSERT_EQUAL_INT( 3, machine_getTempoLavorazione( m ) );
    TEST_ASSERT_FALSE( machine_isBusy( m ) );
    /* Default documentato in machine.h/machine.c (TOLLERANZA_LAVORAZIONE_DEFAULT). */
    TEST_ASSERT_EQUAL_DOUBLE( 0.02, machine_getTolleranzaLavorazione( m ) );

    machine_delete( m );
}

void test_machine_create_tempo_lavorazione_non_valido( void )
{
    short int err;
    machine_t *m = machine_create( "M1", 0, &err );

    TEST_ASSERT_NULL( m );
    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, err );
}

void test_machine_create_id_vuoto( void )
{
    short int err;
    machine_t *m = machine_create( "", 3, &err );

    TEST_ASSERT_NULL( m );
    TEST_ASSERT_EQUAL_INT( ERR_ID_INVALID, err );
}

/* ------------------------------------------------------------------ */
/*  Ciclo occupata/libera: admit, isReady, tryRelease                  */
/* ------------------------------------------------------------------ */

void test_machine_admit_su_macchina_libera_la_occupa( void )
{
    short int err;
    machine_t *m = machine_create( "M1", 3, &err );
    object_t *obj = object_create( "P1", 5, 'A', 0, 100.0, 10.0, &err );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, machine_admit( m, obj, 0 ) );
    TEST_ASSERT_TRUE( machine_isBusy( m ) );

    object_delete( machine_tryRelease( m, 3 ) );
    machine_delete( m );
}

void test_machine_admit_su_macchina_occupata_fallisce( void )
{
    short int err;
    machine_t *m = machine_create( "M1", 3, &err );
    object_t *primo = object_create( "P1", 5, 'A', 0, 100.0, 10.0, &err );
    object_t *secondo = object_create( "P2", 5, 'A', 0, 100.0, 10.0, &err );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, machine_admit( m, primo, 0 ) );
    TEST_ASSERT_EQUAL_INT( ERR_FULL, machine_admit( m, secondo, 0 ) );

    object_delete( machine_tryRelease( m, 3 ) );
    object_delete( secondo );
    machine_delete( m );
}

void test_machine_isReady_falso_prima_del_tempo_di_lavorazione( void )
{
    short int err;
    machine_t *m = machine_create( "M1", 3, &err );
    object_t *obj = object_create( "P1", 5, 'A', 0, 100.0, 10.0, &err );

    machine_admit( m, obj, 10 );
    TEST_ASSERT_FALSE( machine_isReady( m, 10 ) );
    TEST_ASSERT_FALSE( machine_isReady( m, 12 ) );
    TEST_ASSERT_TRUE( machine_isReady( m, 13 ) );  /* 10 + tempo_lavorazione(3) */
    TEST_ASSERT_TRUE( machine_isReady( m, 20 ) );  /* resta pronta anche dopo */

    object_delete( machine_tryRelease( m, 13 ) );
    machine_delete( m );
}

void test_machine_tryRelease_prima_del_tempo_ritorna_null( void )
{
    short int err;
    machine_t *m = machine_create( "M1", 3, &err );
    object_t *obj = object_create( "P1", 5, 'A', 0, 100.0, 10.0, &err );

    machine_admit( m, obj, 0 );
    TEST_ASSERT_NULL( machine_tryRelease( m, 2 ) );
    TEST_ASSERT_TRUE( machine_isBusy( m ) );  /* resta occupata: non ha rilasciato nulla */

    object_delete( machine_tryRelease( m, 3 ) );
    machine_delete( m );
}

void test_machine_tryRelease_su_macchina_libera_ritorna_null( void )
{
    short int err;
    machine_t *m = machine_create( "M1", 3, &err );

    TEST_ASSERT_NULL( machine_tryRelease( m, 0 ) );

    machine_delete( m );
}

void test_machine_tryRelease_libera_la_macchina( void )
{
    short int err;
    machine_t *m = machine_create( "M1", 3, &err );
    object_t *obj = object_create( "P1", 5, 'A', 0, 100.0, 10.0, &err );

    machine_admit( m, obj, 0 );
    object_t *rilasciato = machine_tryRelease( m, 3 );

    TEST_ASSERT_NOT_NULL( rilasciato );
    TEST_ASSERT_EQUAL_PTR( obj, rilasciato );
    TEST_ASSERT_FALSE( machine_isBusy( m ) );

    object_delete( rilasciato );
    machine_delete( m );
}

/* ------------------------------------------------------------------ */
/*  Riduzione fissa di lavorazione (MACHINE_DLAVORATO/MACHINE_RLAVORATO) */
/* ------------------------------------------------------------------ */

void test_tryRelease_applica_la_riduzione_fissa_a_tolleranza_zero( void )
{
    /* Con tolleranza a zero, fattore_rumore non introduce scarto: il
     * risultato deve coincidere ESATTAMENTE con dimensionX/raggio
     * originali meno la riduzione fissa (MACHINE_DLAVORATO/RLAVORATO,
     * vedi machine.h) - nessun rumore casuale a confondere il confronto. */
    short int err;
    machine_t *m = machine_create( "M1", 1, &err );
    machine_setTolleranzaLavorazione( m, 0.0 );
    object_t *obj = object_create( "P1", 5, 'A', 0, 100.0, 10.0, &err );

    machine_admit( m, obj, 0 );
    object_t *rilasciato = machine_tryRelease( m, 1 );

    TEST_ASSERT_NOT_NULL( rilasciato );
    TEST_ASSERT_EQUAL_DOUBLE( 100.0 - MACHINE_DLAVORATO, object_getDimensionX( rilasciato ) );
    TEST_ASSERT_EQUAL_DOUBLE( 10.0 - MACHINE_RLAVORATO, object_getRaggio( rilasciato ) );

    object_delete( rilasciato );
    machine_delete( m );
}

/* ------------------------------------------------------------------ */
/*  Rumore di lavorazione entro la tolleranza dichiarata                */
/*  (test di regressione sul bug di fattore_rumore, vedi doc file)      */
/* ------------------------------------------------------------------ */

void test_tryRelease_rumore_resta_entro_la_tolleranza( void )
{
    /* Con la vecchia fattore_rumore, il 20% delle chiamate restituiva
     * uno scarto di +/- 5 volte la tolleranza: su un campione di 500
     * lavorazioni indipendenti, questo test avrebbe quasi certamente
     * incontrato almeno un caso fuori range e sarebbe fallito. */
    const double tolleranza = 0.10;
    const double baseDimX = 100.0 - MACHINE_DLAVORATO;
    const double baseRaggio = 10.0 - MACHINE_RLAVORATO;
    const double minDimX = baseDimX * ( 1.0 - tolleranza );
    const double maxDimX = baseDimX * ( 1.0 + tolleranza );
    const double minRaggio = baseRaggio * ( 1.0 - tolleranza );
    const double maxRaggio = baseRaggio * ( 1.0 + tolleranza );
    int i;

    for ( i = 0; i < 500; i++ ) {
        short int err;
        char id[16];
        machine_t *m = machine_create( "M1", 1, &err );
        machine_setTolleranzaLavorazione( m, tolleranza );

        snprintf( id, sizeof( id ), "P%d", i );
        object_t *obj = object_create( id, 5, 'A', 0, 100.0, 10.0, &err );

        machine_admit( m, obj, 0 );
        object_t *rilasciato = machine_tryRelease( m, 1 );

        TEST_ASSERT_NOT_NULL( rilasciato );
        TEST_ASSERT_TRUE_MESSAGE( object_getDimensionX( rilasciato ) >= minDimX
                                   && object_getDimensionX( rilasciato ) <= maxDimX,
                                   "dimensionX fuori dall'intervallo di tolleranza dichiarato" );
        TEST_ASSERT_TRUE_MESSAGE( object_getRaggio( rilasciato ) >= minRaggio
                                   && object_getRaggio( rilasciato ) <= maxRaggio,
                                   "raggio fuori dall'intervallo di tolleranza dichiarato" );

        object_delete( rilasciato );
        machine_delete( m );
    }
}

/* ------------------------------------------------------------------ */
/*  Tolleranza di lavorazione: setter/getter                           */
/* ------------------------------------------------------------------ */

void test_setTolleranzaLavorazione_valore_valido( void )
{
    short int err;
    machine_t *m = machine_create( "M1", 3, &err );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, machine_setTolleranzaLavorazione( m, 0.05 ) );
    TEST_ASSERT_EQUAL_DOUBLE( 0.05, machine_getTolleranzaLavorazione( m ) );

    machine_delete( m );
}

void test_setTolleranzaLavorazione_negativa_fallisce( void )
{
    short int err;
    machine_t *m = machine_create( "M1", 3, &err );

    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, machine_setTolleranzaLavorazione( m, -0.01 ) );
    /* Il valore di default non deve essere stato toccato dal tentativo fallito. */
    TEST_ASSERT_EQUAL_DOUBLE( 0.02, machine_getTolleranzaLavorazione( m ) );

    machine_delete( m );
}

/* ------------------------------------------------------------------ */
/*  Puntatori NULL                                                     */
/* ------------------------------------------------------------------ */

void test_funzioni_su_null_non_crashano( void )
{
    TEST_ASSERT_NULL( machine_create( NULL, 3, NULL ) );
    TEST_ASSERT_FALSE( machine_isBusy( NULL ) );
    TEST_ASSERT_FALSE( machine_isReady( NULL, 0 ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, machine_getTempoLavorazione( NULL ) );
    TEST_ASSERT_NULL( machine_getID( NULL ) );
    TEST_ASSERT_EQUAL_DOUBLE( -1.0, machine_getTolleranzaLavorazione( NULL ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, machine_admit( NULL, NULL, 0 ) );
    TEST_ASSERT_NULL( machine_tryRelease( NULL, 0 ) );
    machine_delete( NULL );  /* non deve crashare */
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_machine_create_valida );
    RUN_TEST( test_machine_create_tempo_lavorazione_non_valido );
    RUN_TEST( test_machine_create_id_vuoto );

    RUN_TEST( test_machine_admit_su_macchina_libera_la_occupa );
    RUN_TEST( test_machine_admit_su_macchina_occupata_fallisce );
    RUN_TEST( test_machine_isReady_falso_prima_del_tempo_di_lavorazione );
    RUN_TEST( test_machine_tryRelease_prima_del_tempo_ritorna_null );
    RUN_TEST( test_machine_tryRelease_su_macchina_libera_ritorna_null );
    RUN_TEST( test_machine_tryRelease_libera_la_macchina );

    RUN_TEST( test_tryRelease_applica_la_riduzione_fissa_a_tolleranza_zero );
    RUN_TEST( test_tryRelease_rumore_resta_entro_la_tolleranza );

    RUN_TEST( test_setTolleranzaLavorazione_valore_valido );
    RUN_TEST( test_setTolleranzaLavorazione_negativa_fallisce );

    RUN_TEST( test_funzioni_su_null_non_crashano );

    return UNITY_END();
}
