/**
 * @file test_isp_routing_materiale.c
 * @brief Test Unity per l'instradamento a 4 uscite su un'ISP con
 *        sensore di qualita' agganciato (vedi processISP in
 *        Controllore.c): CONFORME+materiale A -> indice 0, CONFORME+
 *        materiale B -> indice 3, CONFORME+non_classificato ->
 *        RIVALUTAZIONE (indice 1), stessa logica usata in produzione
 *        per ISP2 (B_Alacciaio/B_riqualifica/B_TRASH/B_rame).
 *
 * Copre in particolare la decisione presa in seguito alla scoperta del
 * bug di instradamento nel materiale opposto (vedi README): un pezzo
 * "non classificato" da get_Material ora finisce in RIVALUTAZIONE
 * (B_riqualifica) invece di essere instradato comunque in base al tipo
 * dichiarato.
 */

#include "unity.h"
#include "cell.h"
#include "Controllore.h"
#include "object.h"
#include "errors.h"

static cell_t *g_cell;
static controllore_t *g_ctrl;

/* Target dell'ISP: dimensionX_target=100, raggio_target=10 (stessi
 * valori usati in test_sensore_qualita.c, per coerenza). */
#define TARGET_DIMX 100
#define TARGET_RAGGIO 10

void setUp( void )
{
    short int err;

    g_cell = cell_create();
    cell_addBuffer( g_cell, "B1", 5, &err );
    cell_addISP( g_cell, "ISP1", 1, &err );   /* tempo_controllo = 1 passo */
    /* 4 uscite, STESSO ORDINE della convenzione di produzione (vedi
     * plant_config_valid.txt per ISP2): 0=materiale A, 1=RIVALUTAZIONE/
     * non_classificato, 2=SCARTO, 3=materiale B. */
    cell_addBuffer( g_cell, "OUT_A", 5, &err );
    cell_addBuffer( g_cell, "OUT_RIVAL", 5, &err );
    cell_addBuffer( g_cell, "OUT_SCARTO", 5, &err );
    cell_addBuffer( g_cell, "OUT_B", 5, &err );

    cell_connect( g_cell, "B1", "ISP1" );
    cell_connect( g_cell, "ISP1", "OUT_A" );
    cell_connect( g_cell, "ISP1", "OUT_RIVAL" );
    cell_connect( g_cell, "ISP1", "OUT_SCARTO" );
    cell_connect( g_cell, "ISP1", "OUT_B" );

    g_ctrl = controllore_create( g_cell, 0.8, &err );
    controllore_collegaSensoreQualita( g_ctrl, "ISP1", TARGET_DIMX, TARGET_RAGGIO );
}

void tearDown( void )
{
    const char *bufferOutputs[] = { "OUT_A", "OUT_RIVAL", "OUT_SCARTO", "OUT_B", "B1" };
    int i;
    for ( i = 0; i < 5; i++ ) {
        buffer_t *b = cell_getBuffer( g_cell, bufferOutputs[i] );
        if ( b != NULL ) {
            bufferObj_t *cur = b->head;
            while ( cur != NULL ) { object_delete( cur->dato ); cur = cur->next; }
        }
    }
    controllore_destroy( g_ctrl );
    cell_destroy( g_cell );
    g_cell = NULL;
    g_ctrl = NULL;
}

static void inserisci_e_avanza( char tipo, double dimensionX, double raggio )
{
    short int err;
    object_t *obj = object_create( "P1", 5, tipo, 0, dimensionX, raggio, &err );
    int step;

    controllore_ammettiArrivo( g_ctrl, "B1", obj, 0 );
    for ( step = 0; step < 5; step++ ) {
        controllore_step( g_ctrl, step );
    }
}

/* ------------------------------------------------------------------ */
/*  Materiale riconosciuto: instradamento diretto                      */
/* ------------------------------------------------------------------ */

void test_conforme_materiale_A_va_in_OUT_A( void )
{
    /* Dimensioni identiche al target: esito CONFORME, get_Material
     * riconosce 'A' senza ambiguita'. */
    inserisci_e_avanza( 'A', TARGET_DIMX, TARGET_RAGGIO );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT_A" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT_B" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT_RIVAL" ) ) );
}

void test_conforme_materiale_B_va_in_OUT_B( void )
{
    inserisci_e_avanza( 'B', TARGET_DIMX, TARGET_RAGGIO );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT_B" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT_A" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT_RIVAL" ) ) );
}

/* ------------------------------------------------------------------ */
/*  Non classificato: va in RIVALUTAZIONE, MAI nel buffer del tipo      */
/*  dichiarato ne' in quello opposto (decisione presa col gruppo)       */
/* ------------------------------------------------------------------ */

void test_conforme_ma_non_classificato_va_in_RIVALUTAZIONE( void )
{
    /* +5% su dimensionX e +5% su raggio: esito CONFORME (percentualeX/R
     * <=5%, vedi get_qualita), ma il volume (dimX*raggio^2) si discosta
     * di circa +15.8% dal target - FUORI dalla tolleranza +-12% di
     * get_Material (l'elevamento al quadrato del raggio amplifica la
     * deviazione percentuale) - quindi non_classificato. */
    double dimX = TARGET_DIMX * 1.05;
    double raggio = TARGET_RAGGIO * 1.05;

    inserisci_e_avanza( 'A', dimX, raggio );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT_RIVAL" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT_A" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT_B" ) ) );

    TEST_ASSERT_EQUAL_INT( 1, controllore_getMaterialeNonClassificato( g_ctrl, "ISP1" ) );
}

void test_non_classificato_di_tipo_B_va_comunque_in_RIVALUTAZIONE( void )
{
    /* Stesso caso di sopra ma con tipo dichiarato 'B': la destinazione
     * per un pezzo non classificato deve essere la STESSA
     * (RIVALUTAZIONE) indipendentemente dal tipo dichiarato - non deve
     * ne' andare nel proprio buffer "presunto" ne' in quello opposto. */
    double dimX = TARGET_DIMX * 1.05;
    double raggio = TARGET_RAGGIO * 1.05;

    inserisci_e_avanza( 'B', dimX, raggio );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT_RIVAL" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT_A" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT_B" ) ) );
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_conforme_materiale_A_va_in_OUT_A );
    RUN_TEST( test_conforme_materiale_B_va_in_OUT_B );

    RUN_TEST( test_conforme_ma_non_classificato_va_in_RIVALUTAZIONE );
    RUN_TEST( test_non_classificato_di_tipo_B_va_comunque_in_RIVALUTAZIONE );

    return UNITY_END();
}
