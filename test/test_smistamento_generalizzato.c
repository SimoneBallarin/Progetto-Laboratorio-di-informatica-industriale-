/**
 * @file test_smistamento_generalizzato.c
 * @brief Test Unity per il sistema di smistamento generalizzato delle
 *        ISP (vedi tipo_smistamento_t e controllore_impostaSmistamentoQualita
 *        in Controllore.h, e determinaIndiceUscitaQualita in Controllore.c).
 *
 * Introdotto per rendere il codice indipendente dal layout descritto dal
 * plant_config (vedi README, sezione "Layout 2"): SMISTAMENTO_MATERIALE
 * (instrada solo per materiale, es. l'ISP iniziale del layout 2) e
 * SMISTAMENTO_QUALITA (instrada solo per esito qualità, es. le ISP
 * finali del layout 2, una per linea/materiale) si aggiungono al
 * comportamento storico SMISTAMENTO_MATERIALE_E_QUALITA (layout 1, già
 * coperto da test_isp_routing_materiale.c) e SMISTAMENTO_PASSACARTE.
 *
 * determinaIndiceUscitaQualita è statica (interna a Controllore.c): i
 * test qui sotto la esercitano indirettamente attraverso controllore_step,
 * osservando in quale buffer di uscita finisce l'oggetto - stesso
 * approccio già usato in test_isp_routing_materiale.c.
 */

#include "unity.h"
#include "cell.h"
#include "Controllore.h"
#include "object.h"
#include "errors.h"

static cell_t *g_cell;
static controllore_t *g_ctrl;

#define TARGET_DIMX 100
#define TARGET_RAGGIO 10

/* setUp crea una ISP con 3 uscite (OUT0/OUT1/OUT2): abbastanza per
 * testare sia SMISTAMENTO_MATERIALE (usa solo le prime due, la terza
 * resta disponibile per i non classificati) sia SMISTAMENTO_QUALITA
 * (usa tutte e tre, una per CONFORME/RIVALUTAZIONE/SCARTO). */
void setUp( void )
{
    short int err;

    g_cell = cell_create();
    cell_addBuffer( g_cell, "B1", 5, &err );
    cell_addISP( g_cell, "ISP1", 1, &err );
    cell_addBuffer( g_cell, "OUT0", 5, &err );
    cell_addBuffer( g_cell, "OUT1", 5, &err );
    cell_addBuffer( g_cell, "OUT2", 5, &err );

    cell_connect( g_cell, "B1", "ISP1" );
    cell_connect( g_cell, "ISP1", "OUT0" );
    cell_connect( g_cell, "ISP1", "OUT1" );
    cell_connect( g_cell, "ISP1", "OUT2" );

    g_ctrl = controllore_create( g_cell, 0.8, &err );
    controllore_collegaSensoreQualita( g_ctrl, "ISP1", TARGET_DIMX, TARGET_RAGGIO );
}

void tearDown( void )
{
    const char *buffers[] = { "OUT0", "OUT1", "OUT2", "B1" };
    int i;
    for ( i = 0; i < 4; i++ ) {
        buffer_t *b = cell_getBuffer( g_cell, buffers[i] );
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
/*  SMISTAMENTO_MATERIALE: instrada solo per materiale, ignora qualità */
/* ------------------------------------------------------------------ */

void test_smistamento_materiale_A_va_su_indice_0( void )
{
    controllore_impostaSmistamentoQualita( g_ctrl, "ISP1", SMISTAMENTO_MATERIALE );

    inserisci_e_avanza( 'A', TARGET_DIMX, TARGET_RAGGIO );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT0" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT1" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT2" ) ) );
}

void test_smistamento_materiale_B_va_su_indice_1( void )
{
    controllore_impostaSmistamentoQualita( g_ctrl, "ISP1", SMISTAMENTO_MATERIALE );

    inserisci_e_avanza( 'B', TARGET_DIMX, TARGET_RAGGIO );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT1" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT0" ) ) );
}

void test_smistamento_materiale_ignora_qualita_scadente( void )
{
    /* +11% SOLO su dimensionX (raggio invariato): l'esito qualita' e'
     * SCARTO (percentualeX=11% supera anche la soglia RIVALUTAZIONE del
     * 10%, mentre percentualeR=0% resta ininfluente per il calcolo
     * combinato) - ma il materiale resta comunque riconoscibile come 'A'
     * (get_Material tollera fino a +11% quando solo dimensionX si
     * discosta, dato che il volume dipende linearmente da dimensionX ma
     * quadraticamente dal raggio, qui invariato - verificato
     * empiricamente, vedi commit). Con SMISTAMENTO_MATERIALE l'unico
     * criterio e' il materiale riconosciuto: deve andare comunque
     * sull'indice 0, nonostante l'esito qualita' pessimo. */
    controllore_impostaSmistamentoQualita( g_ctrl, "ISP1", SMISTAMENTO_MATERIALE );

    inserisci_e_avanza( 'A', TARGET_DIMX * 1.11, TARGET_RAGGIO );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT0" ) ) );
}

void test_smistamento_materiale_non_classificato_va_sulla_terza_uscita( void )
{
    /* Con 3 uscite disponibili, un pezzo non classificabile per
     * materiale va sull'ultima (indice 2), invece di finire per default
     * su una delle due uscite "materiale". */
    controllore_impostaSmistamentoQualita( g_ctrl, "ISP1", SMISTAMENTO_MATERIALE );

    inserisci_e_avanza( 'A', TARGET_DIMX * 1.05, TARGET_RAGGIO * 1.05 );  /* non classificato, vedi test_isp_routing_materiale.c */

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT2" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT0" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT1" ) ) );
}

/* ------------------------------------------------------------------ */
/*  SMISTAMENTO_QUALITA: instrada solo per esito, ignora il materiale  */
/* ------------------------------------------------------------------ */

void test_smistamento_qualita_conforme_va_su_indice_0_indipendentemente_dal_materiale( void )
{
    controllore_impostaSmistamentoQualita( g_ctrl, "ISP1", SMISTAMENTO_QUALITA );

    /* Materiale 'B', ma con SMISTAMENTO_QUALITA non deve fare differenza:
     * l'indice dipende solo dall'esito (CONFORME qui). */
    inserisci_e_avanza( 'B', TARGET_DIMX, TARGET_RAGGIO );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT0" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT1" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT2" ) ) );
}

void test_smistamento_qualita_rivalutazione_va_su_indice_1( void )
{
    controllore_impostaSmistamentoQualita( g_ctrl, "ISP1", SMISTAMENTO_QUALITA );

    /* +8% su entrambe: fuori dalla soglia CONFORME (5%) ma dentro
     * RIVALUTAZIONE (10%, vedi default in S_Qualita.c). */
    inserisci_e_avanza( 'A', TARGET_DIMX * 1.08, TARGET_RAGGIO * 1.08 );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT1" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT0" ) ) );
}

void test_smistamento_qualita_scarto_va_su_indice_2( void )
{
    controllore_impostaSmistamentoQualita( g_ctrl, "ISP1", SMISTAMENTO_QUALITA );

    inserisci_e_avanza( 'A', TARGET_DIMX * 0.5, TARGET_RAGGIO * 0.5 );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT2" ) ) );
}

/* ------------------------------------------------------------------ */
/*  SMISTAMENTO_PASSACARTE: sempre indice 0, qualunque cosa succeda    */
/* ------------------------------------------------------------------ */

void test_smistamento_passacarte_ignora_qualsiasi_esito( void )
{
    controllore_impostaSmistamentoQualita( g_ctrl, "ISP1", SMISTAMENTO_PASSACARTE );

    /* Anche un pezzo palesemente SCARTO va comunque su OUT0. */
    inserisci_e_avanza( 'A', TARGET_DIMX * 0.5, TARGET_RAGGIO * 0.5 );

    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "OUT0" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT1" ) ) );
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "OUT2" ) ) );
}

/* ------------------------------------------------------------------ */
/*  controllore_impostaSmistamentoQualita: validazione                 */
/* ------------------------------------------------------------------ */

void test_imposta_smistamento_su_isp_senza_sensore_restituisce_not_found( void )
{
    TEST_ASSERT_EQUAL_INT( ERR_NOT_FOUND, controllore_impostaSmistamentoQualita( g_ctrl, "ISP_INESISTENTE", SMISTAMENTO_QUALITA ) );
}

void test_imposta_smistamento_su_null_restituisce_errore( void )
{
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, controllore_impostaSmistamentoQualita( NULL, "ISP1", SMISTAMENTO_QUALITA ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, controllore_impostaSmistamentoQualita( g_ctrl, NULL, SMISTAMENTO_QUALITA ) );
}

/* ------------------------------------------------------------------ */
/*  controllore_haMotoreCollegato                                      */
/* ------------------------------------------------------------------ */

void test_ha_motore_collegato_falso_se_nessun_motore_agganciato( void )
{
    /* setUp non aggancia nessun Motore a ISP1/B1/OUT0: nessuno di questi
     * ID e' comunque un target valido per un motore (ISP/buffer, non
     * nastro/macchina), ma la funzione deve restituire false in modo
     * pulito, non crashare. */
    TEST_ASSERT_FALSE( controllore_haMotoreCollegato( g_ctrl, "ISP1" ) );
    TEST_ASSERT_FALSE( controllore_haMotoreCollegato( g_ctrl, "NASTRO_INESISTENTE" ) );
}

void test_ha_motore_collegato_vero_dopo_collegaMotore( void )
{
    short int err;
    cell_addNastro( g_cell, "N1", 5, 2, &err );
    cell_connect( g_cell, "OUT0", "N1" );

    controllore_collegaMotore( g_ctrl, "N1", 10, 2 );

    TEST_ASSERT_TRUE( controllore_haMotoreCollegato( g_ctrl, "N1" ) );
}

void test_ha_motore_collegato_su_null_restituisce_falso( void )
{
    TEST_ASSERT_FALSE( controllore_haMotoreCollegato( NULL, "N1" ) );
    TEST_ASSERT_FALSE( controllore_haMotoreCollegato( g_ctrl, NULL ) );
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_smistamento_materiale_A_va_su_indice_0 );
    RUN_TEST( test_smistamento_materiale_B_va_su_indice_1 );
    RUN_TEST( test_smistamento_materiale_ignora_qualita_scadente );
    RUN_TEST( test_smistamento_materiale_non_classificato_va_sulla_terza_uscita );

    RUN_TEST( test_smistamento_qualita_conforme_va_su_indice_0_indipendentemente_dal_materiale );
    RUN_TEST( test_smistamento_qualita_rivalutazione_va_su_indice_1 );
    RUN_TEST( test_smistamento_qualita_scarto_va_su_indice_2 );

    RUN_TEST( test_smistamento_passacarte_ignora_qualsiasi_esito );

    RUN_TEST( test_imposta_smistamento_su_isp_senza_sensore_restituisce_not_found );
    RUN_TEST( test_imposta_smistamento_su_null_restituisce_errore );

    RUN_TEST( test_ha_motore_collegato_falso_se_nessun_motore_agganciato );
    RUN_TEST( test_ha_motore_collegato_vero_dopo_collegaMotore );
    RUN_TEST( test_ha_motore_collegato_su_null_restituisce_falso );

    return UNITY_END();
}
