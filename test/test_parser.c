/**
 * @file test_parser.c
 * @brief Test Unity per lib/parser (parser.c/.h): parsing di file di
 *        configurazione impianto, scenario e oggetti - file validi,
 *        malformati, con ID duplicati, campi mancanti/fuori intervallo
 *        (copertura richiesta esplicitamente dalla traccia, sez. 11:
 *        "i test dovranno coprire almeno parsing").
 *
 * I file usati sono in test/fixtures/ (percorsi relativi: questo test
 * va lanciato dalla ROOT del progetto, stessa convenzione di
 * test/run_tests.sh e degli altri file test_*.c).
 */

#include "unity.h"
#include "parser.h"
#include "cell.h"
#include "Controllore.h"
#include "object.h"
#include "errors.h"
#include "registry.h"

#define FIX "test/fixtures/"

void setUp( void ) {}
void tearDown( void )
{
    /* Il registro (registry.h) e' un singleton GLOBALE ("una sola cella
     * per esecuzione", vedi registry.h): senza svuotarlo qui, il
     * prossimo test che crea una cella con gli STESSI ID (es. "B1")
     * fallirebbe con ERR_DUPLICATE, dato che cell_destroy nei test sopra
     * NON tocca il registro (libera solo le strutture interne della
     * cella) - stesso motivo per cui main.c chiama registry_clear() tra
     * le due esecuzioni della modalita' di confronto, e per cui altri
     * file di test in questa suite (es. test_controllore_strategia.c)
     * lo fanno esplicitamente prima di costruire una seconda cella nello
     * stesso test. Qui va fatto ad OGNI test (non solo quando serve),
     * perche' ogni funzione test_* in questo file crea una cella con gli
     * stessi ID delle altre. */
    registry_clear();
}

/* ------------------------------------------------------------------ */
/*  parser_costruisciCella                                             */
/* ------------------------------------------------------------------ */

void test_costruisciCella_file_valido_crea_tutti_gli_elementi( void )
{
    short int err;
    cell_t *cell = cell_create();

    int creati = parser_costruisciCella( cell, FIX "config_valida.txt", &err );

    /* 2 BUFFER (B1, B2) + 1 ISP + 1 NASTRO + 1 MACCHINA + 4 CONNECT
     * riuscite = 9 "elementi creati" (parser_costruisciCella conta
     * anche i collegamenti CONNECT riusciti, non solo le entita' - vedi
     * gestisci_riga_connect). Le righe MOTORE non contano (gestite da
     * parser_collegaAttuatori, non da questa funzione). */
    TEST_ASSERT_EQUAL_INT( 9, creati );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, err );
    TEST_ASSERT_TRUE( cell_hasBuffer( cell, "B1" ) );
    TEST_ASSERT_TRUE( cell_hasBuffer( cell, "B2" ) );
    TEST_ASSERT_TRUE( cell_hasISP( cell, "ISP1" ) );
    TEST_ASSERT_TRUE( cell_hasNastro( cell, "N1" ) );
    TEST_ASSERT_TRUE( cell_hasMachine( cell, "M" ) );

    /* I collegamenti (CONNECT) devono essere stati applicati davvero,
     * non solo le entita' create isolate. */
    TEST_ASSERT_TRUE( buffer_hasOutput( cell_getBuffer( cell, "B1" ), "ISP1" ) );

    cell_destroy( cell );
}

void test_costruisciCella_file_inesistente( void )
{
    short int err;
    cell_t *cell = cell_create();

    int creati = parser_costruisciCella( cell, FIX "questo_file_non_esiste.txt", &err );

    TEST_ASSERT_EQUAL_INT( 0, creati );
    TEST_ASSERT_EQUAL_INT( ERR_NOT_FOUND, err );

    cell_destroy( cell );
}

void test_costruisciCella_righe_malformate_vengono_scartate_singolarmente( void )
{
    /* config_malformata.txt: B1 (ok), riga senza virgole (sconosciuta,
     * scartata), B2 senza CAPACITY (campo mancante), B3 con CAPACITY
     * negativa (rifiutata da buffer_create), ID vuoto (campo mancante),
     * B4 con CAPACITY non numerica (atoi->0, rifiutata), B5 (ok).
     * Devono sopravvivere SOLO B1 e B5: il parser deve scartare le righe
     * malformate una per una, SENZA fermarsi ne' fare crash. */
    short int err;
    cell_t *cell = cell_create();

    int creati = parser_costruisciCella( cell, FIX "config_malformata.txt", &err );

    TEST_ASSERT_EQUAL_INT( 2, creati );
    TEST_ASSERT_TRUE( cell_hasBuffer( cell, "B1" ) );
    TEST_ASSERT_TRUE( cell_hasBuffer( cell, "B5" ) );
    TEST_ASSERT_FALSE( cell_hasBuffer( cell, "B2" ) );
    TEST_ASSERT_FALSE( cell_hasBuffer( cell, "B3" ) );
    TEST_ASSERT_FALSE( cell_hasBuffer( cell, "B4" ) );

    cell_destroy( cell );
}

void test_costruisciCella_id_duplicato_tiene_solo_il_primo( void )
{
    short int err;
    cell_t *cell = cell_create();

    int creati = parser_costruisciCella( cell, FIX "config_id_duplicato.txt", &err );

    /* B1 (prima occorrenza, capacita' 10) + B2 = 2 creati; la seconda
     * riga B1 (capacita' 20) deve essere rifiutata come ID duplicato,
     * SENZA sovrascrivere ne' duplicare la prima. */
    TEST_ASSERT_EQUAL_INT( 2, creati );
    TEST_ASSERT_EQUAL_INT( 10, buffer_getCapacity( cell_getBuffer( cell, "B1" ) ) );

    cell_destroy( cell );
}

void test_costruisciCella_null_restituisce_errore( void )
{
    short int err;
    TEST_ASSERT_EQUAL_INT( 0, parser_costruisciCella( NULL, FIX "config_valida.txt", &err ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, err );
}

void test_costruisciCella_righe_di_commento_vengono_ignorate( void )
{
    /* config_con_commenti.txt: righe che iniziano con '#' (anche con
     * spazi prima) devono essere trattate come righe vuote - ne'
     * generare un "tipo di record sconosciuto" ne' interferire col
     * parsing delle righe vere attorno. */
    short int err;
    cell_t *cell = cell_create();

    int creati = parser_costruisciCella( cell, FIX "config_con_commenti.txt", &err );

    TEST_ASSERT_EQUAL_INT( 2, creati );
    TEST_ASSERT_TRUE( cell_hasBuffer( cell, "B1" ) );
    TEST_ASSERT_TRUE( cell_hasBuffer( cell, "B2" ) );

    cell_destroy( cell );
}

void test_caricaSimulazione_ignora_righe_di_commento( void )
{
    SimulationConfig sim;
    short int err;

    int letto = parser_caricaSimulazione( FIX "config_con_commenti.txt", &sim, &err );

    TEST_ASSERT_EQUAL_INT( 1, letto );
    TEST_ASSERT_EQUAL_INT( 42, sim.n_step_simulazione );
}

void test_caricaSimulazione_SIM_PEZZI_B2_default_zero( void )
{
    /* config_valida.txt non ha SIM_PEZZI_B2: il default (0, nessun
     * pre-caricamento) deve valere, non lasciare il campo indefinito. */
    SimulationConfig sim;
    short int err;

    parser_caricaSimulazione( FIX "config_valida.txt", &sim, &err );

    TEST_ASSERT_EQUAL_INT( 0, sim.n_pezzi_prova_b2 );
}

void test_caricaSimulazione_SIM_PEZZI_B2_letto_correttamente( void )
{
    SimulationConfig sim;
    short int err;

    int letto = parser_caricaSimulazione( "lib/parser/plant_config_layout1.txt", &sim, &err );

    TEST_ASSERT_EQUAL_INT( 1, letto );
    TEST_ASSERT_EQUAL_INT( 0, sim.n_pezzi_prova_b2 );  /* valore attuale nel file di produzione */
}

void test_caricaSimulazione_SCADENZA_STEP_default_40( void )
{
    /* config_valida.txt non ha SCADENZA_STEP: il default storico (40)
     * deve valere, non lasciare il campo indefinito. */
    SimulationConfig sim;
    short int err;

    parser_caricaSimulazione( FIX "config_valida.txt", &sim, &err );

    TEST_ASSERT_EQUAL_INT( 40, sim.scadenza_step );
}

void test_caricaSimulazione_SCADENZA_STEP_letta_correttamente( void )
{
    SimulationConfig sim;
    short int err;

    int letto = parser_caricaSimulazione( "lib/parser/plant_config_layout1.txt", &sim, &err );

    TEST_ASSERT_EQUAL_INT( 1, letto );
    TEST_ASSERT_EQUAL_INT( 40, sim.scadenza_step );  /* valore attuale nel file di produzione */
}

/* ------------------------------------------------------------------ */
/*  parser_caricaSimulazione                                           */
/* ------------------------------------------------------------------ */

void test_caricaSimulazione_file_valido( void )
{
    SimulationConfig sim;
    short int err;

    int letto = parser_caricaSimulazione( FIX "config_valida.txt", &sim, &err );

    TEST_ASSERT_EQUAL_INT( 1, letto );
    TEST_ASSERT_EQUAL_INT( 50, sim.n_step_simulazione );
    TEST_ASSERT_EQUAL_INT( 5, sim.n_pezzi_prova );
    TEST_ASSERT_EQUAL_DOUBLE( 0.8, sim.soglia_buffer );
}

void test_caricaSimulazione_file_inesistente_mantiene_i_default( void )
{
    /* Anche se il file non si apre, out deve restare popolato con
     * valori di default sensati (non spazzatura/non azzerato a meta') -
     * vedi doc in parser.h: i default sono scritti PRIMA di tentare
     * fopen. */
    SimulationConfig sim;
    short int err;

    int letto = parser_caricaSimulazione( FIX "non_esiste.txt", &sim, &err );

    TEST_ASSERT_EQUAL_INT( 0, letto );
    TEST_ASSERT_EQUAL_INT( ERR_NOT_FOUND, err );
    TEST_ASSERT_EQUAL_INT( 60, sim.n_step_simulazione );  /* default documentato in parser.c */
    TEST_ASSERT_EQUAL_DOUBLE( 0.8, sim.soglia_buffer );
}

/* ------------------------------------------------------------------ */
/*  parser_caricaScenario                                              */
/* ------------------------------------------------------------------ */

void test_caricaScenario_guasto_singolo( void )
{
    ScenarioConfig scenario;
    short int err;

    /* Uso uno scenario "vero" del progetto (non una fixture dedicata):
     * verifica anche che il formato a singola ISP, usato in produzione
     * da scenario_nominale_layout1.txt/scenario_difficile_layout1.txt, resti valido
     * dopo l'estensione a lista (vedi ScenarioConfig in parser.h). */
    int letto = parser_caricaScenario( "lib/parser/scenario_difficile_layout1.txt", &scenario, &err );

    TEST_ASSERT_EQUAL_INT( 1, letto );
    TEST_ASSERT_TRUE( scenario.guasto_abilitato );
    TEST_ASSERT_EQUAL_INT( 1, scenario.n_guasto_isp );
    TEST_ASSERT_EQUAL_STRING( "ISP2", scenario.guasto_isp_id[0] );
}

void test_caricaScenario_guasto_su_piu_isp( void )
{
    ScenarioConfig scenario;
    short int err;

    int letto = parser_caricaScenario( FIX "scenario_doppio.txt", &scenario, &err );

    TEST_ASSERT_EQUAL_INT( 1, letto );
    TEST_ASSERT_EQUAL_INT( 2, scenario.n_guasto_isp );
    TEST_ASSERT_EQUAL_STRING( "ISP1", scenario.guasto_isp_id[0] );
    TEST_ASSERT_EQUAL_STRING( "ISP2", scenario.guasto_isp_id[1] );
}

void test_caricaScenario_troppe_isp_vengono_troncate( void )
{
    /* scenario_troppi_guasti.txt elenca 5 ISP (MAX_GUASTO_ISP = 4): le
     * prime 4 devono essere tenute, la quinta scartata con un avviso -
     * non deve ne' fare overflow dell'array ne' crashare. */
    ScenarioConfig scenario;
    short int err;

    int letto = parser_caricaScenario( FIX "scenario_troppi_guasti.txt", &scenario, &err );

    TEST_ASSERT_EQUAL_INT( 1, letto );
    TEST_ASSERT_EQUAL_INT( 4, scenario.n_guasto_isp );
    TEST_ASSERT_EQUAL_STRING( "ISP1", scenario.guasto_isp_id[0] );
    TEST_ASSERT_EQUAL_STRING( "ISP4", scenario.guasto_isp_id[3] );
}

void test_caricaScenario_file_inesistente( void )
{
    ScenarioConfig scenario;
    short int err;

    int letto = parser_caricaScenario( FIX "non_esiste.txt", &scenario, &err );

    TEST_ASSERT_EQUAL_INT( 0, letto );
    TEST_ASSERT_EQUAL_INT( ERR_NOT_FOUND, err );
}

/* ------------------------------------------------------------------ */
/*  parser_caricaOggetti                                               */
/* ------------------------------------------------------------------ */

static cell_t *g_cell;
static controllore_t *g_ctrl;

static void setup_cella_minima( void )
{
    short int err;
    g_cell = cell_create();
    cell_addBuffer( g_cell, "B1", 10, &err );
    g_ctrl = controllore_create( g_cell, 0.8, &err );
}

static void teardown_cella_minima( void )
{
    /* Libera sia eventuali oggetti gia' entrati in B1 sia quelli ancora
     * schedulati (questi ultimi liberati automaticamente da
     * controllore_destroy, vedi doc di controllore_schedulaArrivo). */
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

void test_caricaOggetti_file_valido_schedula_tutti( void )
{
    short int err;
    setup_cella_minima();

    int caricati = parser_caricaOggetti( g_cell, g_ctrl, FIX "oggetti_validi.txt", "B1", &err );

    TEST_ASSERT_EQUAL_INT( 3, caricati );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, err );
    /* Nessuno ancora entrato in B1: sono tutti SCHEDULATI (vedi
     * parser_caricaOggetti aggiornato), non inseriti subito - anche
     * OBJ1 con ARRIVAL_STEP=0 entra solo al primo controllore_step. */
    TEST_ASSERT_EQUAL_INT( 0, buffer_getCount( cell_getBuffer( g_cell, "B1" ) ) );
    TEST_ASSERT_EQUAL_INT( 3, controllore_getArriviSchedulatiCount( g_ctrl ) );

    teardown_cella_minima();
}

void test_caricaOggetti_righe_malformate_vengono_scartate_singolarmente( void )
{
    /* oggetti_malformati.txt: BUONO1 (ok), ID vuoto, TYPE non valido,
     * PRIORITY troppo alta, PRIORITY negativa, DIMENSIONE negativa,
     * riga incompleta, BUONO2 (ok) - devono sopravvivere solo i 2 "ok",
     * il resto scartato riga per riga senza fermare il caricamento. */
    short int err;
    setup_cella_minima();

    int caricati = parser_caricaOggetti( g_cell, g_ctrl, FIX "oggetti_malformati.txt", "B1", &err );

    TEST_ASSERT_EQUAL_INT( 2, caricati );

    teardown_cella_minima();
}

void test_caricaOggetti_buffer_ingresso_inesistente( void )
{
    short int err;
    setup_cella_minima();

    int caricati = parser_caricaOggetti( g_cell, g_ctrl, FIX "oggetti_validi.txt", "B_FANTASMA", &err );

    TEST_ASSERT_EQUAL_INT( 0, caricati );
    TEST_ASSERT_EQUAL_INT( ERR_NOT_FOUND, err );

    teardown_cella_minima();
}

void test_caricaOggetti_file_inesistente( void )
{
    short int err;
    setup_cella_minima();

    int caricati = parser_caricaOggetti( g_cell, g_ctrl, FIX "non_esiste.txt", "B1", &err );

    TEST_ASSERT_EQUAL_INT( 0, caricati );
    TEST_ASSERT_EQUAL_INT( ERR_NOT_FOUND, err );

    teardown_cella_minima();
}

void test_caricaOggetti_funziona_su_un_buffer_diverso_da_B1( void )
{
    /* parser_caricaOggetti accetta il buffer di destinazione come
     * parametro esplicito (bufferIngressoID): deve funzionare
     * identicamente per QUALUNQUE buffer, non solo "B1" - verifica
     * diretta della generalizzazione usata per il pre-caricamento di B2
     * (vedi app/main.c, oggetti_b2_path). */
    short int err;
    cell_t *cell = cell_create();
    cell_addBuffer( cell, "B2", 10, &err );
    controllore_t *ctrl = controllore_create( cell, 0.8, &err );

    int caricati = parser_caricaOggetti( cell, ctrl, FIX "oggetti_validi.txt", "B2", &err );

    TEST_ASSERT_EQUAL_INT( 3, caricati );
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, err );
    TEST_ASSERT_EQUAL_INT( 3, controllore_getArriviSchedulatiCount( ctrl ) );

    controllore_step( ctrl, 0 );
    /* OBJ1 ha ARRIVAL_STEP=0 nel fixture: deve entrare in B2, non in
     * un ipotetico "B1" (che qui non esiste nemmeno nella cella). */
    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( cell, "B2" ) ) );

    bufferObj_t *cur = cell_getBuffer( cell, "B2" )->head;
    while ( cur != NULL ) { object_delete( cur->dato ); cur = cur->next; }
    controllore_destroy( ctrl );
    cell_destroy( cell );
}

void test_caricaOggetti_arrivi_rispettano_ARRIVAL_STEP( void )
{
    /* Verifica end-to-end: dopo aver caricato oggetti_validi.txt
     * (OBJ1 a step 0, OBJ2 a step 5, OBJ3 a step 10), far girare la
     * simulazione passo per passo e controllare che entrino esattamente
     * quando previsto, non prima. */
    short int err;
    setup_cella_minima();

    parser_caricaOggetti( g_cell, g_ctrl, FIX "oggetti_validi.txt", "B1", &err );

    controllore_step( g_ctrl, 0 );
    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "B1" ) ) );  /* solo OBJ1 */

    controllore_step( g_ctrl, 1 );
    controllore_step( g_ctrl, 2 );
    controllore_step( g_ctrl, 3 );
    controllore_step( g_ctrl, 4 );
    TEST_ASSERT_EQUAL_INT( 1, buffer_getCount( cell_getBuffer( g_cell, "B1" ) ) );  /* OBJ2 non ancora */

    controllore_step( g_ctrl, 5 );
    TEST_ASSERT_EQUAL_INT( 2, buffer_getCount( cell_getBuffer( g_cell, "B1" ) ) );  /* OBJ2 arrivato */

    controllore_step( g_ctrl, 10 );
    TEST_ASSERT_EQUAL_INT( 3, buffer_getCount( cell_getBuffer( g_cell, "B1" ) ) );  /* anche OBJ3 */
    TEST_ASSERT_EQUAL_INT( 0, controllore_getArriviSchedulatiCount( g_ctrl ) );

    teardown_cella_minima();
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_costruisciCella_file_valido_crea_tutti_gli_elementi );
    RUN_TEST( test_costruisciCella_file_inesistente );
    RUN_TEST( test_costruisciCella_righe_malformate_vengono_scartate_singolarmente );
    RUN_TEST( test_costruisciCella_id_duplicato_tiene_solo_il_primo );
    RUN_TEST( test_costruisciCella_null_restituisce_errore );
    RUN_TEST( test_costruisciCella_righe_di_commento_vengono_ignorate );

    RUN_TEST( test_caricaSimulazione_file_valido );
    RUN_TEST( test_caricaSimulazione_file_inesistente_mantiene_i_default );
    RUN_TEST( test_caricaSimulazione_ignora_righe_di_commento );
    RUN_TEST( test_caricaSimulazione_SIM_PEZZI_B2_default_zero );
    RUN_TEST( test_caricaSimulazione_SIM_PEZZI_B2_letto_correttamente );

    RUN_TEST( test_caricaSimulazione_SCADENZA_STEP_default_40 );
    RUN_TEST( test_caricaSimulazione_SCADENZA_STEP_letta_correttamente );

    RUN_TEST( test_caricaScenario_guasto_singolo );
    RUN_TEST( test_caricaScenario_guasto_su_piu_isp );
    RUN_TEST( test_caricaScenario_troppe_isp_vengono_troncate );
    RUN_TEST( test_caricaScenario_file_inesistente );

    RUN_TEST( test_caricaOggetti_file_valido_schedula_tutti );
    RUN_TEST( test_caricaOggetti_funziona_su_un_buffer_diverso_da_B1 );
    RUN_TEST( test_caricaOggetti_righe_malformate_vengono_scartate_singolarmente );
    RUN_TEST( test_caricaOggetti_buffer_ingresso_inesistente );
    RUN_TEST( test_caricaOggetti_file_inesistente );
    RUN_TEST( test_caricaOggetti_arrivi_rispettano_ARRIVAL_STEP );

    return UNITY_END();
}
