/**
 * @file test_buffer.c
 * @brief Test Unity per lib/Buffer (buffer.c/.h).
 *
 * Copre in particolare i due criteri di inserimento/prelievo
 * (priority=true/false) che dalla Strategia 2 (FCFS, vedi
 * Controllore.h) dipendono direttamente: se questi si rompessero,
 * romperebbero silenziosamente ENTRAMBE le strategie di controllo.
 */

#include "unity.h"
#include "buffer.h"
#include "object.h"
#include "errors.h"

/* setUp/tearDown sono chiamate da Unity prima/dopo OGNI singolo test
 * (RUN_TEST): usate qui solo per documentare che questo file non ha
 * stato condiviso tra test (ogni test crea ed elimina i propri
 * buffer/oggetti), cosi' un test che fallisce non lascia residui che
 * facciano fallire in cascata quelli dopo. */
void setUp( void ) {}
void tearDown( void ) {}

/* ------------------------------------------------------------------ */
/*  Creazione                                                          */
/* ------------------------------------------------------------------ */

void test_buffer_create_valido( void )
{
    short int err;
    buffer_t *b = buffer_create( "B1", 5, &err );

    TEST_ASSERT_NOT_NULL( b );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, err );
    TEST_ASSERT_EQUAL_STRING( "B1", buffer_getID( b ) );
    TEST_ASSERT_EQUAL_INT( 5, buffer_getCapacity( b ) );
    TEST_ASSERT_TRUE( buffer_isEmpty( b ) );
    TEST_ASSERT_FALSE( buffer_isFull( b ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( b ) );

    buffer_delete( b );
}

void test_buffer_create_capacita_non_valida( void )
{
    short int err;
    buffer_t *b = buffer_create( "B1", 0, &err );

    TEST_ASSERT_NULL( b );
    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, err );
}

void test_buffer_create_capacita_negativa( void )
{
    short int err;
    buffer_t *b = buffer_create( "B1", -3, &err );

    TEST_ASSERT_NULL( b );
    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, err );
}

/* ------------------------------------------------------------------ */
/*  Inserimento/prelievo FIFO (priority = false) - Strategia 2         */
/* ------------------------------------------------------------------ */

void test_buffer_fifo_rispetta_ordine_di_arrivo( void )
{
    short int err;
    buffer_t *b = buffer_create( "B1", 5, &err );
    object_t *p1 = object_create( "P1", 2, 'A', 0, 10.0, 5.0, &err );
    object_t *p2 = object_create( "P2", 9, 'A', 0, 10.0, 5.0, &err );  /* priorita' alta, arrivato dopo */
    object_t *p3 = object_create( "P3", 5, 'A', 0, 10.0, 5.0, &err );

    buffer_insertObject( b, p1, false );
    buffer_insertObject( b, p2, false );
    buffer_insertObject( b, p3, false );

    /* FIFO: deve uscire nell'ordine di inserimento, IGNORANDO la
     * priorita' (P2 ha priorita' 9 ma e' arrivato secondo: in FCFS non
     * deve "saltare la fila"). */
    object_t *out1 = buffer_removeObject( b, false );
    object_t *out2 = buffer_removeObject( b, false );
    object_t *out3 = buffer_removeObject( b, false );

    TEST_ASSERT_EQUAL_STRING( "P1", object_getID( out1 ) );
    TEST_ASSERT_EQUAL_STRING( "P2", object_getID( out2 ) );
    TEST_ASSERT_EQUAL_STRING( "P3", object_getID( out3 ) );

    object_delete( out1 );
    object_delete( out2 );
    object_delete( out3 );
    buffer_delete( b );
}

/* ------------------------------------------------------------------ */
/*  Inserimento/prelievo per priorita' (priority = true) - Strategia 1 */
/* ------------------------------------------------------------------ */

void test_buffer_priorita_preleva_sempre_il_piu_alto( void )
{
    short int err;
    buffer_t *b = buffer_create( "B1", 5, &err );
    object_t *basso = object_create( "P_basso", 1, 'A', 0, 10.0, 5.0, &err );
    object_t *alto  = object_create( "P_alto",  9, 'A', 0, 10.0, 5.0, &err );  /* arrivato DOPO, ma priorita' piu' alta */
    object_t *medio = object_create( "P_medio", 5, 'A', 0, 10.0, 5.0, &err );

    buffer_insertObject( b, basso, true );
    buffer_insertObject( b, alto, true );
    buffer_insertObject( b, medio, true );

    /* Priorita': deve uscire in ordine di priorita' decrescente,
     * IGNORANDO l'ordine di arrivo. */
    object_t *out1 = buffer_removeObject( b, true );
    object_t *out2 = buffer_removeObject( b, true );
    object_t *out3 = buffer_removeObject( b, true );

    TEST_ASSERT_EQUAL_STRING( "P_alto", object_getID( out1 ) );
    TEST_ASSERT_EQUAL_STRING( "P_medio", object_getID( out2 ) );
    TEST_ASSERT_EQUAL_STRING( "P_basso", object_getID( out3 ) );

    object_delete( out1 );
    object_delete( out2 );
    object_delete( out3 );
    buffer_delete( b );
}

void test_buffer_priorita_parita_mantiene_ordine_di_arrivo( void )
{
    /* A parita' di priorita', buffer_insertObject (vedi buffer.c: "cur->next != NULL &&
     * object_getPriority(cur->next->dato) >= newPriority") inserisce DOPO gli elementi
     * con la stessa priorita' gia' presenti: comportamento stabile (FIFO tra pari). */
    short int err;
    buffer_t *b = buffer_create( "B1", 5, &err );
    object_t *primo  = object_create( "Primo", 5, 'A', 0, 10.0, 5.0, &err );
    object_t *secondo = object_create( "Secondo", 5, 'A', 0, 10.0, 5.0, &err );

    buffer_insertObject( b, primo, true );
    buffer_insertObject( b, secondo, true );

    object_t *out1 = buffer_removeObject( b, true );
    object_t *out2 = buffer_removeObject( b, true );

    TEST_ASSERT_EQUAL_STRING( "Primo", object_getID( out1 ) );
    TEST_ASSERT_EQUAL_STRING( "Secondo", object_getID( out2 ) );

    object_delete( out1 );
    object_delete( out2 );
    buffer_delete( b );
}

/* ------------------------------------------------------------------ */
/*  Capacita' e casi limite                                            */
/* ------------------------------------------------------------------ */

void test_buffer_pieno_rifiuta_inserimento( void )
{
    short int err;
    buffer_t *b = buffer_create( "B1", 2, &err );
    object_t *p1 = object_create( "P1", 1, 'A', 0, 10.0, 5.0, &err );
    object_t *p2 = object_create( "P2", 1, 'A', 0, 10.0, 5.0, &err );
    object_t *p3 = object_create( "P3", 1, 'A', 0, 10.0, 5.0, &err );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, buffer_insertObject( b, p1, true ) );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, buffer_insertObject( b, p2, true ) );
    TEST_ASSERT_TRUE( buffer_isFull( b ) );
    TEST_ASSERT_EQUAL_INT( ERR_FULL, buffer_insertObject( b, p3, true ) );
    TEST_ASSERT_EQUAL_INT( 2, buffer_getCount( b ) );

    object_delete( buffer_removeObject( b, true ) );
    object_delete( buffer_removeObject( b, true ) );
    object_delete( p3 );  /* mai entrato nel buffer: liberato a parte */
    buffer_delete( b );
}

void test_buffer_vuoto_removeObject_restituisce_null( void )
{
    short int err;
    buffer_t *b = buffer_create( "B1", 3, &err );

    TEST_ASSERT_NULL( buffer_removeObject( b, true ) );
    TEST_ASSERT_NULL( buffer_removeObject( b, false ) );

    buffer_delete( b );
}

void test_buffer_null_e_gestito_senza_crash( void )
{
    /* buffer_removeObject/isEmpty/isFull su buffer NULL non devono
     * mai fare crash (dereferenziare un puntatore NULL): la doc di
     * buffer.h promette ERR_NULL_PTR/false/NULL a seconda del tipo di
     * ritorno, mai un comportamento indefinito. */
    TEST_ASSERT_NULL( buffer_removeObject( NULL, true ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, buffer_getCount( NULL ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, buffer_getCapacity( NULL ) );
    TEST_ASSERT_NULL( buffer_getID( NULL ) );
}

/* ------------------------------------------------------------------ */
/*  Collegamenti input/output                                          */
/* ------------------------------------------------------------------ */

void test_buffer_addOutput_e_getOutputAt( void )
{
    short int err;
    char outID[IDLENGTH];
    buffer_t *b = buffer_create( "B1", 5, &err );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, buffer_addOutput( b, "ISP1" ) );
    TEST_ASSERT_EQUAL_INT( 1, buffer_getOutputCount( b ) );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, buffer_getOutputAt( b, 0, outID ) );
    TEST_ASSERT_EQUAL_STRING( "ISP1", outID );
    TEST_ASSERT_TRUE( buffer_hasOutput( b, "ISP1" ) );
    TEST_ASSERT_FALSE( buffer_hasOutput( b, "ISP2" ) );

    buffer_delete( b );
}

void test_buffer_addOutput_duplicato_restituisce_err( void )
{
    short int err;
    buffer_t *b = buffer_create( "B1", 5, &err );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, buffer_addOutput( b, "ISP1" ) );
    TEST_ASSERT_EQUAL_INT( ERR_DUPLICATE, buffer_addOutput( b, "ISP1" ) );

    buffer_delete( b );
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_buffer_create_valido );
    RUN_TEST( test_buffer_create_capacita_non_valida );
    RUN_TEST( test_buffer_create_capacita_negativa );

    RUN_TEST( test_buffer_fifo_rispetta_ordine_di_arrivo );

    RUN_TEST( test_buffer_priorita_preleva_sempre_il_piu_alto );
    RUN_TEST( test_buffer_priorita_parita_mantiene_ordine_di_arrivo );

    RUN_TEST( test_buffer_pieno_rifiuta_inserimento );
    RUN_TEST( test_buffer_vuoto_removeObject_restituisce_null );
    RUN_TEST( test_buffer_null_e_gestito_senza_crash );

    RUN_TEST( test_buffer_addOutput_e_getOutputAt );
    RUN_TEST( test_buffer_addOutput_duplicato_restituisce_err );

    return UNITY_END();
}
