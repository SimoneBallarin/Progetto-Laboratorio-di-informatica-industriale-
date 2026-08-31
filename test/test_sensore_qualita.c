/**
 * @file test_sensore_qualita.c
 * @brief Test Unity per get_Material (lib/Sensori/Sensore Qualita).
 *
 * Copre in particolare:
 *  - il nuovo contatore non_classificato (pezzi ne' A ne' B entro
 *    tolleranza, prima invisibili - vedi discussione che ha portato
 *    all'aggiunta di questo contatore);
 *  - il caso object->type diverso da 'A'/'B', che PRIMA di questa
 *    modifica lasciava materiale/e non inizializzate in get_Material
 *    (comportamento indefinito) - oggi il generatore in main.c produce
 *    solo 'A'/'B', quindi questo caso non è mai raggiunto in pratica,
 *    ma lo sarà appena il file oggetti in ingresso sarà collegato.
 */

#include "unity.h"
#include "S_Qualita.h"
#include "object.h"
#include "errors.h"

static SensoreQualita g_sensore;
static MalfunzionamentoSensore g_guasto;

void setUp( void )
{
    /* Target scelti per rendere i tre esiti (A/B/non_classificato)
     * facili da produrre con oggetti costruiti a mano: densita' A=8.96,
     * densita' B=7.85 (vedi get_Material), quindi a parita' di
     * dimensionX/raggio i due materiali producono "ConfrontoA"/
     * "ConfrontoB" diversi tra loro, e un oggetto con dimensionX/raggio
     * molto piccoli rispetto al target non rientra nella tolleranza
     * (+-12%) di nessuno dei due. */
    sensore_qualita_init( &g_sensore, "ISP_TEST", &g_guasto, false, 100, 10 );
}

void tearDown( void )
{
    /* Nessuna allocazione dinamica in SensoreQualita/MalfunzionamentoSensore
     * (entrambe stack-allocated qui): nulla da liberare. */
}

static object_t *crea_oggetto( char tipo, double dimensionX, double raggio )
{
    short int err;
    return object_create( "P1", 5, tipo, 0, dimensionX, raggio, &err );
}

/* ------------------------------------------------------------------ */
/*  Classificazione A/B corretta                                       */
/* ------------------------------------------------------------------ */

void test_get_Material_riconosce_A_entro_tolleranza( void )
{
    /* Stesse dimensioni del target esatto: deve classificare 'A'. */
    object_t *obj = crea_oggetto( 'A', 100, 10 );

    char esito = get_Material( obj, &g_sensore );

    TEST_ASSERT_EQUAL_CHAR( 'A', esito );
    TEST_ASSERT_EQUAL_INT( 1, get_ConteggioMaterialeA( &g_sensore ) );
    TEST_ASSERT_EQUAL_INT( 0, get_ConteggioMaterialeB( &g_sensore ) );
    TEST_ASSERT_EQUAL_INT( 0, get_ConteggioNonClassificato( &g_sensore ) );

    object_delete( obj );
}

void test_get_Material_riconosce_B_entro_tolleranza( void )
{
    object_t *obj = crea_oggetto( 'B', 100, 10 );

    char esito = get_Material( obj, &g_sensore );

    TEST_ASSERT_EQUAL_CHAR( 'B', esito );
    TEST_ASSERT_EQUAL_INT( 1, get_ConteggioMaterialeB( &g_sensore ) );
    TEST_ASSERT_EQUAL_INT( 0, get_ConteggioNonClassificato( &g_sensore ) );

    object_delete( obj );
}

/* ------------------------------------------------------------------ */
/*  Non classificato: ne' A ne' B entro tolleranza                     */
/* ------------------------------------------------------------------ */

void test_get_Material_fuori_tolleranza_incrementa_non_classificato( void )
{
    /* dimensionX/raggio MOLTO piu' piccoli del target (100/10): il
     * "ConfrontoA"/"ConfrontoB" risultante (proporzionale a
     * dimensionX*raggio^2) cade fuori dalla tolleranza +-12% di
     * entrambi i materiali. */
    object_t *obj = crea_oggetto( 'A', 1, 1 );

    char esito = get_Material( obj, &g_sensore );

    TEST_ASSERT_EQUAL_INT( 0, esito );  /* 0 = non classificato, vedi doc get_Material */
    TEST_ASSERT_EQUAL_INT( 0, get_ConteggioMaterialeA( &g_sensore ) );
    TEST_ASSERT_EQUAL_INT( 0, get_ConteggioMaterialeB( &g_sensore ) );
    TEST_ASSERT_EQUAL_INT( 1, get_ConteggioNonClassificato( &g_sensore ) );

    object_delete( obj );
}

void test_get_Material_non_classificato_si_accumula_su_piu_chiamate( void )
{
    object_t *o1 = crea_oggetto( 'A', 1, 1 );
    object_t *o2 = crea_oggetto( 'B', 1, 1 );

    get_Material( o1, &g_sensore );
    get_Material( o2, &g_sensore );

    TEST_ASSERT_EQUAL_INT( 2, get_ConteggioNonClassificato( &g_sensore ) );

    object_delete( o1 );
    object_delete( o2 );
}

/* ------------------------------------------------------------------ */
/*  Tipo sconosciuto: non deve leggere materiale/e non inizializzate   */
/* ------------------------------------------------------------------ */

void test_get_Material_tipo_sconosciuto_e_non_classificato_senza_crash( void )
{
    /* object->type = 'Z': ne' 'A' ne' 'B'. PRIMA della correzione,
     * materiale/e non venivano mai assegnate in questo caso e il
     * confronto successivo le leggeva non inizializzate (comportamento
     * indefinito, il risultato poteva essere 'A', 'B' o non_classificato
     * a seconda della spazzatura in memoria - quindi ASSERT_EQUAL qui
     * sarebbe stato inaffidabile prima della fix). Ora il caso e'
     * gestito esplicitamente: sempre non_classificato, mai un crash o un
     * risultato casuale. */
    object_t *obj = crea_oggetto( 'Z', 100, 10 );

    char esito = get_Material( obj, &g_sensore );

    TEST_ASSERT_EQUAL_INT( 0, esito );
    TEST_ASSERT_EQUAL_INT( 1, get_ConteggioNonClassificato( &g_sensore ) );
    TEST_ASSERT_EQUAL_INT( 0, get_ConteggioMaterialeA( &g_sensore ) );
    TEST_ASSERT_EQUAL_INT( 0, get_ConteggioMaterialeB( &g_sensore ) );

    object_delete( obj );
}

/* ------------------------------------------------------------------ */
/*  Regressione: niente piu' instradamento nel materiale OPPOSTO        */
/* ------------------------------------------------------------------ */

void test_get_Material_non_instrada_piu_nel_materiale_opposto( void )
{
    /* Stesso identico caso usato per dimostrare il bug dal vivo (vedi
     * discussione): un oggetto dichiarato 'A', con dimensioni che
     * cadono FUORI dalla finestra di tolleranza di 'A' ma che PRIMA
     * della correzione cadevano (per puro caso numerico, complice il
     * riuso della soglia sbagliata) dentro quella - allora sbagliata -
     * di 'B'. Target 100/10 (vedi setUp), dimensioni scelte per essere
     * abbastanza fuori tolleranza da 'A' (+-12% di volume) da non
     * classificare piu' come 'A', ma che secondo il vecchio bug
     * sarebbero risultate 'B'. */
    object_t *obj = crea_oggetto( 'A', 97.3, 6.0 );

    char esito = get_Material( obj, &g_sensore );

    /* Deve risultare non_classificato (dimensioni troppo diverse dal
     * target per il materiale dichiarato), MAI 'B' - un oggetto
     * dichiarato 'A' non deve mai poter finire instradato nel buffer
     * del materiale opposto. */
    TEST_ASSERT_NOT_EQUAL( 'B', esito );
    TEST_ASSERT_EQUAL_INT( 0, get_ConteggioMaterialeB( &g_sensore ) );
    TEST_ASSERT_EQUAL_INT( 1, get_ConteggioNonClassificato( &g_sensore ) );

    object_delete( obj );
}

/* ------------------------------------------------------------------ */
/*  Puntatori NULL                                                     */
/* ------------------------------------------------------------------ */

void test_get_Material_su_null_non_crasha( void )
{
    object_t *obj = crea_oggetto( 'A', 100, 10 );

    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, get_Material( NULL, &g_sensore ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, get_Material( obj, NULL ) );
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, get_ConteggioNonClassificato( NULL ) );

    object_delete( obj );
}

/* ------------------------------------------------------------------ */
/*  Tolleranze di classificazione (CONFORME/RIVALUTAZIONE/SCARTO)      */
/*  configurabili: vedi sensore_qualita_imposta_tolleranze e la nuova  */
/*  possibilita' di impostarle da plant_config (TOLLERANZA_CONFORME/   */
/*  TOLLERANZA_RIVALUTAZIONE sulla riga ISP).                          */
/* ------------------------------------------------------------------ */

void test_imposta_tolleranze_valori_validi( void )
{
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, sensore_qualita_imposta_tolleranze( &g_sensore, 3, 8 ) );
}

void test_imposta_tolleranze_rivalutazione_minore_di_conforme_fallisce( void )
{
    /* RIVALUTAZIONE deve restare >= CONFORME (vedi get_qualita: prima
     * si controlla CONFORME, poi RIVALUTAZIONE): altrimenti nessun
     * pezzo potrebbe mai risultare RIVALUTAZIONE. */
    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, sensore_qualita_imposta_tolleranze( &g_sensore, 10, 5 ) );
}

void test_imposta_tolleranze_valori_non_positivi_falliscono( void )
{
    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, sensore_qualita_imposta_tolleranze( &g_sensore, 0, 10 ) );
    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, sensore_qualita_imposta_tolleranze( &g_sensore, 5, 0 ) );
    TEST_ASSERT_EQUAL_INT( ERR_OUT_OF_RANGE, sensore_qualita_imposta_tolleranze( &g_sensore, -1, 10 ) );
}

void test_imposta_tolleranze_su_null_restituisce_errore( void )
{
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, sensore_qualita_imposta_tolleranze( NULL, 5, 10 ) );
}

void test_get_qualita_rispetta_tolleranze_strette( void )
{
    /* Target 100/10 (vedi setUp): un pezzo scostato del 6% sarebbe
     * CONFORME col default (tolleranza 5%/10%: 6% < 10 -> RIVALUTAZIONE)
     * ma con una tolleranza CONFORME piu' larga (7%) deve risultare
     * CONFORME; ristretta a 3%/4%, lo stesso pezzo deve risultare
     * SCARTO (6% supera anche la soglia RIVALUTAZIONE=4%). */
    object_t *obj = crea_oggetto( 'A', 106.0, 10.6 );  /* +6% su entrambi */

    sensore_qualita_imposta_tolleranze( &g_sensore, 7, 10 );
    TEST_ASSERT_EQUAL_INT( CONFORME, get_qualita( &g_sensore, &g_guasto, 0, obj, true ) );

    sensore_qualita_imposta_tolleranze( &g_sensore, 3, 4 );
    TEST_ASSERT_EQUAL_INT( SCARTO, get_qualita( &g_sensore, &g_guasto, 1, obj, true ) );

    object_delete( obj );
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_get_Material_riconosce_A_entro_tolleranza );
    RUN_TEST( test_get_Material_riconosce_B_entro_tolleranza );

    RUN_TEST( test_get_Material_fuori_tolleranza_incrementa_non_classificato );
    RUN_TEST( test_get_Material_non_classificato_si_accumula_su_piu_chiamate );

    RUN_TEST( test_get_Material_tipo_sconosciuto_e_non_classificato_senza_crash );
    RUN_TEST( test_get_Material_non_instrada_piu_nel_materiale_opposto );

    RUN_TEST( test_get_Material_su_null_non_crasha );

    RUN_TEST( test_imposta_tolleranze_valori_validi );
    RUN_TEST( test_imposta_tolleranze_rivalutazione_minore_di_conforme_fallisce );
    RUN_TEST( test_imposta_tolleranze_valori_non_positivi_falliscono );
    RUN_TEST( test_imposta_tolleranze_su_null_restituisce_errore );
    RUN_TEST( test_get_qualita_rispetta_tolleranze_strette );

    return UNITY_END();
}
