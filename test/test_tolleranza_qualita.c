/**
 * @file test_tolleranza_qualita.c
 * @brief Test Unity per controllore_impostaToleranzaQualita (vedi
 *        Controllore.h): configurazione delle soglie CONFORME/
 *        RIVALUTAZIONE del sensore di qualita' agganciato a una ISP, a
 *        runtime tramite il controllore (non solo direttamente sul
 *        SensoreQualita, vedi test_sensore_qualita.c per quel livello).
 *
 * Stessa mini-cella di test_isp_guasto.c (B1 -> ISP1 -> B_OUT), dato che
 * serve lo stesso scenario minimo con un sensore di qualita' agganciato.
 */

#include "unity.h"
#include "cell.h"
#include "Controllore.h"
#include "S_Qualita.h"
#include "object.h"
#include "errors.h"

static cell_t *g_cell;
static controllore_t *g_ctrl;

void setUp( void )
{
    short int err;

    g_cell = cell_create();
    cell_addBuffer( g_cell, "B1", 5, &err );
    cell_addISP( g_cell, "ISP1", 1, &err );      /* tempo_controllo = 1 passo */
    cell_addBuffer( g_cell, "B_OUT", 5, &err );
    cell_connect( g_cell, "B1", "ISP1" );
    cell_connect( g_cell, "ISP1", "B_OUT" );

    g_ctrl = controllore_create( g_cell, 0.8, &err );
    controllore_collegaSensoreQualita( g_ctrl, "ISP1", 100, 10 );
}

void tearDown( void )
{
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

/* Inserisce un pezzo scostato del 6% dal target (100/10): CONFORME solo
 * se la tolleranza CONFORME e' stata allargata oltre il default (5%). */
static void inserisci_pezzo_scostato_6pct( void )
{
    short int err;
    object_t *obj = object_create( "P1", 5, 'A', 0, 106.0, 10.6, &err );
    controllore_ammettiArrivo( g_ctrl, "B1", obj, 0 );
}

/* ------------------------------------------------------------------ */
/*  Effetto end-to-end sulla classificazione                           */
/* ------------------------------------------------------------------ */

void test_tolleranza_allargata_rende_conforme_un_pezzo_altrimenti_rivalutazione( void )
{
    long tipi[3];

    controllore_impostaToleranzaQualita( g_ctrl, "ISP1", 7, 10 );
    inserisci_pezzo_scostato_6pct();

    int step;
    for ( step = 0; step < 3; step++ ) {
        controllore_step( g_ctrl, step );
    }

    controllore_getTipoLettureQualita( g_ctrl, "ISP1", tipi );
    TEST_ASSERT_EQUAL_INT( 1, tipi[CONFORME] );
    TEST_ASSERT_EQUAL_INT( 0, tipi[RIVALUTAZIONE] );
    TEST_ASSERT_EQUAL_INT( 0, tipi[SCARTO] );
}

void test_senza_impostare_tolleranza_stesso_pezzo_e_rivalutazione( void )
{
    /* Baseline: con le tolleranze di default (5%/10%), lo stesso pezzo
     * scostato del 6% deve risultare RIVALUTAZIONE, non CONFORME - la
     * differenza col test sopra e' interamente dovuta a
     * controllore_impostaToleranzaQualita. */
    long tipi[3];

    inserisci_pezzo_scostato_6pct();

    int step;
    for ( step = 0; step < 3; step++ ) {
        controllore_step( g_ctrl, step );
    }

    controllore_getTipoLettureQualita( g_ctrl, "ISP1", tipi );
    TEST_ASSERT_EQUAL_INT( 0, tipi[CONFORME] );
    TEST_ASSERT_EQUAL_INT( 1, tipi[RIVALUTAZIONE] );
}

/* ------------------------------------------------------------------ */
/*  Validazione e casi limite                                          */
/* ------------------------------------------------------------------ */

void test_tolleranza_su_isp_senza_sensore_restituisce_not_found( void )
{
    /* ISP1 ha un sensore agganciato (vedi setUp), ma un ID inesistente no. */
    TEST_ASSERT_EQUAL_INT( ERR_NOT_FOUND, controllore_impostaToleranzaQualita( g_ctrl, "ISP_INESISTENTE", 5, 10 ) );
}

void test_tolleranza_non_valida_restituisce_out_of_range( void )
{
    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, controllore_impostaToleranzaQualita( g_ctrl, "ISP1", 10, 5 ) );
}

void test_tolleranza_su_null_restituisce_errore( void )
{
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, controllore_impostaToleranzaQualita( NULL, "ISP1", 5, 10 ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, controllore_impostaToleranzaQualita( g_ctrl, NULL, 5, 10 ) );
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_tolleranza_allargata_rende_conforme_un_pezzo_altrimenti_rivalutazione );
    RUN_TEST( test_senza_impostare_tolleranza_stesso_pezzo_e_rivalutazione );

    RUN_TEST( test_tolleranza_su_isp_senza_sensore_restituisce_not_found );
    RUN_TEST( test_tolleranza_non_valida_restituisce_out_of_range );
    RUN_TEST( test_tolleranza_su_null_restituisce_errore );

    return UNITY_END();
}
