/**
 * @file test_object.c
 * @brief Test Unity per lib/Oggetto (object.c/.h).
 */

#include "unity.h"
#include "object.h"
#include "errors.h"

void setUp( void ) {}
void tearDown( void ) {}

/* ------------------------------------------------------------------ */
/*  Creazione e validazione                                            */
/* ------------------------------------------------------------------ */

void test_object_create_valido( void )
{
    short int err;
    object_t *obj = object_create( "P1", 5, 'A', 10, 12.5, 3.0, &err );

    TEST_ASSERT_NOT_NULL( obj );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, err );
    TEST_ASSERT_EQUAL_STRING( "P1", object_getID( obj ) );
    TEST_ASSERT_EQUAL_INT( 5, object_getPriority( obj ) );
    TEST_ASSERT_EQUAL_CHAR( 'A', object_getType( obj ) );
    TEST_ASSERT_EQUAL_INT( 10, object_getStepCreation( obj ) );
    TEST_ASSERT_EQUAL_INT( STEP_OUT_NONE, object_getStepOut( obj ) );
    TEST_ASSERT_EQUAL_INT( STEP_OUT_NONE, object_getStepPartial( obj ) );
    TEST_ASSERT_EQUAL_DOUBLE( 12.5, object_getDimensionX( obj ) );
    TEST_ASSERT_EQUAL_DOUBLE( 3.0, object_getRaggio( obj ) );

    object_delete( obj );
}

void test_object_create_priorita_ai_limiti_ammessi( void )
{
    /* PRIORITY_MIN (0) e PRIORITY_MAX (10) devono essere ACCETTATI, non
     * solo il range interno - errore comune "off by one" da testare
     * esplicitamente sui bordi. */
    short int err;
    object_t *min = object_create( "Pmin", PRIORITY_MIN, 'A', 0, 1.0, 1.0, &err );
    TEST_ASSERT_NOT_NULL( min );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, err );

    object_t *max = object_create( "Pmax", PRIORITY_MAX, 'A', 0, 1.0, 1.0, &err );
    TEST_ASSERT_NOT_NULL( max );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, err );

    object_delete( min );
    object_delete( max );
}

void test_object_create_priorita_fuori_range_sopra( void )
{
    short int err;
    object_t *obj = object_create( "P1", PRIORITY_MAX + 1, 'A', 0, 1.0, 1.0, &err );

    TEST_ASSERT_NULL( obj );
    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, err );
}

void test_object_create_priorita_fuori_range_sotto( void )
{
    short int err;
    object_t *obj = object_create( "P1", PRIORITY_MIN - 1, 'A', 0, 1.0, 1.0, &err );

    TEST_ASSERT_NULL( obj );
    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, err );
}

void test_object_create_id_vuoto_e_rifiutato( void )
{
    short int err;
    object_t *obj = object_create( "", 5, 'A', 0, 1.0, 1.0, &err );

    TEST_ASSERT_NULL( obj );
    TEST_ASSERT_EQUAL_INT( ERR_ID_INVALID, err );
}

void test_object_create_id_null_e_rifiutato( void )
{
    short int err;
    object_t *obj = object_create( NULL, 5, 'A', 0, 1.0, 1.0, &err );

    TEST_ASSERT_NULL( obj );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, err );
}

/* ------------------------------------------------------------------ */
/*  Step: creazione / partial / out                                    */
/* ------------------------------------------------------------------ */

void test_object_setStepOut_e_setStepPartial( void )
{
    short int err;
    object_t *obj = object_create( "P1", 5, 'A', 0, 1.0, 1.0, &err );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, object_setStepPartial( obj, 3 ) );
    TEST_ASSERT_EQUAL_INT( 3, object_getStepPartial( obj ) );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, object_setStepOut( obj, 20 ) );
    TEST_ASSERT_EQUAL_INT( 20, object_getStepOut( obj ) );

    /* Con questi valori: tempo sistema = 20-0 = 20, tempo attesa = 3-0 = 3,
     * tempo processo = 20-3 = 17 (vedi statistiche_registraCompletamento). */
    TEST_ASSERT_EQUAL_INT( 20, object_getStepOut( obj ) - object_getStepCreation( obj ) );
    TEST_ASSERT_EQUAL_INT( 17, object_getStepOut( obj ) - object_getStepPartial( obj ) );

    object_delete( obj );
}

void test_object_setStepPartial_due_volte_fallisce( void )
{
    /* object.h lo documenta esplicitamente: va chiamata UNA SOLA VOLTA,
     * per non sovrascrivere il vero inizio del processo con uno step
     * successivo (falserebbe la scomposizione attesa/processo). */
    short int err;
    object_t *obj = object_create( "P1", 5, 'A', 0, 1.0, 1.0, &err );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, object_setStepPartial( obj, 3 ) );
    TEST_ASSERT_EQUAL_INT( ERR_DUPLICATE, object_setStepPartial( obj, 7 ) );
    /* Il valore originale non deve essere stato toccato dal secondo tentativo. */
    TEST_ASSERT_EQUAL_INT( 3, object_getStepPartial( obj ) );

    object_delete( obj );
}

/* ------------------------------------------------------------------ */
/*  Location                                                           */
/* ------------------------------------------------------------------ */

void test_object_location_di_default_e_ID_NONE( void )
{
    short int err;
    object_t *obj = object_create( "P1", 5, 'A', 0, 1.0, 1.0, &err );

    TEST_ASSERT_EQUAL_STRING( ID_NONE, object_getLocation( obj ) );

    object_delete( obj );
}

void test_object_setLocation_aggiorna_la_location( void )
{
    short int err;
    object_t *obj = object_create( "P1", 5, 'A', 0, 1.0, 1.0, &err );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, object_setLocation( obj, "B1" ) );
    TEST_ASSERT_EQUAL_STRING( "B1", object_getLocation( obj ) );

    object_delete( obj );
}

/* ------------------------------------------------------------------ */
/*  Puntatori NULL: nessun crash                                       */
/* ------------------------------------------------------------------ */

void test_object_getter_su_null_non_crasha( void )
{
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, object_getPriority( NULL ) );
    TEST_ASSERT_NULL( object_getID( NULL ) );
    TEST_ASSERT_NULL( object_getLocation( NULL ) );
    TEST_ASSERT_EQUAL_CHAR( '\0', object_getType( NULL ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, object_getStepCreation( NULL ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, object_getStepOut( NULL ) );
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_object_create_valido );
    RUN_TEST( test_object_create_priorita_ai_limiti_ammessi );
    RUN_TEST( test_object_create_priorita_fuori_range_sopra );
    RUN_TEST( test_object_create_priorita_fuori_range_sotto );
    RUN_TEST( test_object_create_id_vuoto_e_rifiutato );
    RUN_TEST( test_object_create_id_null_e_rifiutato );

    RUN_TEST( test_object_setStepOut_e_setStepPartial );
    RUN_TEST( test_object_setStepPartial_due_volte_fallisce );

    RUN_TEST( test_object_location_di_default_e_ID_NONE );
    RUN_TEST( test_object_setLocation_aggiorna_la_location );

    RUN_TEST( test_object_getter_su_null_non_crasha );

    return UNITY_END();
}
