/**
 * @file test_controllore_strategia.c
 * @brief Test Unity di integrazione per Controllore.c: verifica che
 *        STRATEGIA_PRIORITA_BUFFER_AWARE e STRATEGIA_FCFS producano
 *        davvero un ordine di smaltimento diverso, su una mini-cella
 *        B1 -> B2 costruita ad hoc (non l'impianto completo di
 *        plant_config_valid.txt, per tenere il test isolato e veloce).
 *
 * A differenza di test_buffer.c (che testa buffer_insertObject/
 * buffer_removeObject direttamente), questo file verifica che
 * Controllore.c li richiami correttamente con il flag giusto in base
 * alla strategia impostata - il collegamento che l'implementazione
 * della Strategia 2 (sez. 4.1 del progetto preliminare) ha aggiunto.
 */

#include "unity.h"
#include "cell.h"
#include "Controllore.h"
#include "object.h"
#include "errors.h"
#include "registry.h"
#include <string.h>

static cell_t *g_cell;
static controllore_t *g_ctrl;

void setUp( void )
{
    short int err;

    g_cell = cell_create();
    cell_addBuffer( g_cell, "B1", 5, &err );
    cell_addBuffer( g_cell, "B2", 5, &err );
    cell_connect( g_cell, "B1", "B2" );   /* B1 -> B2, nessuna macchina in mezzo */

    g_ctrl = controllore_create( g_cell, 0.8, &err );
}

void tearDown( void )
{
    /* Libera eventuali object_t rimasti in B2 (stesso motivo di
     * libera_tutti_gli_oggetti in app/main.c: buffer_delete/cell_destroy
     * non liberano MAI il payload object_t, resta a carico di chi lo ha
     * inserito - vedi doc di buffer_delete in buffer.h). Senza questo,
     * ogni test che sposta oggetti in B2 perderebbe memoria. */
    buffer_t *b2 = cell_getBuffer( g_cell, "B2" );
    if ( b2 != NULL ) {
        bufferObj_t *cur = b2->head;
        while ( cur != NULL ) {
            object_delete( cur->dato );
            cur = cur->next;
        }
    }

    controllore_destroy( g_ctrl );
    cell_destroy( g_cell );
    g_cell = NULL;
    g_ctrl = NULL;
}

/* Inserisce 3 oggetti a priorita' crescente (P_bassa=1, P_media=5,
 * P_alta=9) in B1, NELL'ORDINE bassa/alta/media - cosi' priorita' e
 * ordine di arrivo non coincidono mai, e i due criteri (priorita' vs
 * FIFO) sono distinguibili da un solo test. */
static void inserisci_tre_oggetti_in_B1( void )
{
    short int err;
    object_t *bassa = object_create( "P_bassa", 1, 'A', 0, 10.0, 5.0, &err );
    object_t *alta  = object_create( "P_alta",  9, 'A', 0, 10.0, 5.0, &err );
    object_t *media = object_create( "P_media", 5, 'A', 0, 10.0, 5.0, &err );

    /* Ordine di ARRIVO: bassa, alta, media. */
    controllore_ammettiArrivo( g_ctrl, "B1", bassa, 0 );
    controllore_ammettiArrivo( g_ctrl, "B1", alta, 0 );
    controllore_ammettiArrivo( g_ctrl, "B1", media, 0 );
}

/* Fa avanzare la simulazione finche' B2 non contiene tutti e 3 gli
 * oggetti (processBuffer sposta un solo oggetto per buffer per passo,
 * quindi servono almeno 3 passi) o si supera un tetto di sicurezza. */
static void avanza_finche_B2_non_e_pieno( int quanti_attesi )
{
    int step;
    buffer_t *b2 = cell_getBuffer( g_cell, "B2" );

    for ( step = 0; step < 20 && buffer_getCount( b2 ) < quanti_attesi; step++ ) {
        controllore_step( g_ctrl, step );
    }
}

/* ------------------------------------------------------------------ */
/*  Strategia di default                                               */
/* ------------------------------------------------------------------ */

void test_strategia_di_default_e_priorita_buffer_aware( void )
{
    /* controllore_create non deve richiedere una chiamata esplicita a
     * controllore_impostaStrategia per avere il comportamento storico
     * (Strategia 1) - vedi doc di STRATEGIA_PRIORITA_BUFFER_AWARE. */
    TEST_ASSERT_EQUAL_INT( STRATEGIA_PRIORITA_BUFFER_AWARE, controllore_getStrategia( g_ctrl ) );
}

void test_impostaStrategia_e_getStrategia_round_trip( void )
{
    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, controllore_impostaStrategia( g_ctrl, STRATEGIA_FCFS ) );
    TEST_ASSERT_EQUAL_INT( STRATEGIA_FCFS, controllore_getStrategia( g_ctrl ) );

    TEST_ASSERT_EQUAL_INT( OP_SUCCESS, controllore_impostaStrategia( g_ctrl, STRATEGIA_PRIORITA_BUFFER_AWARE ) );
    TEST_ASSERT_EQUAL_INT( STRATEGIA_PRIORITA_BUFFER_AWARE, controllore_getStrategia( g_ctrl ) );
}

void test_impostaStrategia_su_null_restituisce_errore( void )
{
    TEST_ASSERT_EQUAL_INT( ERR_NULL_PTR, controllore_impostaStrategia( NULL, STRATEGIA_FCFS ) );
}

/* ------------------------------------------------------------------ */
/*  Strategia 1: l'ordine di USCITA segue la PRIORITA'                 */
/* ------------------------------------------------------------------ */

void test_strategia_priorita_smaltisce_per_priorita_decrescente( void )
{
    inserisci_tre_oggetti_in_B1();
    /* Strategia 1 e' gia' quella di default (vedi test sopra): nessuna
     * chiamata esplicita necessaria qui, per verificare anche il
     * comportamento "out of the box". */

    avanza_finche_B2_non_e_pieno( 3 );

    buffer_t *b2 = cell_getBuffer( g_cell, "B2" );
    TEST_ASSERT_EQUAL_INT( 3, buffer_getCount( b2 ) );

    /* Il PRIMO ad arrivare in B2 deve essere quello a priorita' PIU'
     * ALTA (P_alta), non il primo ad essere arrivato in B1 (P_bassa). */
    bufferObj_t *primo_in_B2 = b2->head;
    TEST_ASSERT_NOT_NULL( primo_in_B2 );
    TEST_ASSERT_EQUAL_STRING( "P_alta", object_getID( primo_in_B2->dato ) );
}

/* ------------------------------------------------------------------ */
/*  Strategia 2: l'ordine di USCITA segue l'ARRIVO (FCFS)              */
/* ------------------------------------------------------------------ */

void test_strategia_fcfs_smaltisce_per_ordine_di_arrivo( void )
{
    controllore_impostaStrategia( g_ctrl, STRATEGIA_FCFS );
    inserisci_tre_oggetti_in_B1();

    avanza_finche_B2_non_e_pieno( 3 );

    buffer_t *b2 = cell_getBuffer( g_cell, "B2" );
    TEST_ASSERT_EQUAL_INT( 3, buffer_getCount( b2 ) );

    /* In FCFS, il PRIMO ad arrivare in B2 deve essere il PRIMO ad
     * essere arrivato in B1 (P_bassa), NON quello a priorita' piu'
     * alta (P_alta) come nella Strategia 1 - questo e' esattamente il
     * comportamento che distingue le due strategie (sez. 4.1). */
    bufferObj_t *primo_in_B2 = b2->head;
    TEST_ASSERT_NOT_NULL( primo_in_B2 );
    TEST_ASSERT_EQUAL_STRING( "P_bassa", object_getID( primo_in_B2->dato ) );
}

void test_le_due_strategie_producono_ordini_diversi_sugli_stessi_dati( void )
{
    /* Test "gemello" dei due precedenti, messi a confronto diretto
     * nello stesso test: stessi 3 oggetti, stesso ordine di arrivo,
     * unica differenza la strategia -> il PRIMO oggetto smaltito deve
     * differire tra le due esecuzioni. Rieseguito da zero su due celle
     * indipendenti (setUp/tearDown girano solo attorno a un singolo
     * test Unity, non tra due "run" dentro lo stesso test), quindi qui
     * costruiamo la seconda cella a mano. */
    short int err;
    char primo_priorita[IDLENGTH];
    char primo_fcfs[IDLENGTH];
    bufferObj_t *b2head;

    /* --- Run 1: Strategia 1 (quella gia' attiva su g_cell/g_ctrl) --- */
    inserisci_tre_oggetti_in_B1();
    avanza_finche_B2_non_e_pieno( 3 );
    b2head = cell_getBuffer( g_cell, "B2" )->head;
    strncpy( primo_priorita, object_getID( b2head->dato ), IDLENGTH );

    /* Libera SUBITO gli object_t appena spostati in B2 di g_cell, PRIMA
     * di registry_clear() qui sotto: cell_getBuffer passa dal registro
     * GLOBALE (vedi cell.c: "return registry_getBuffer(ID)"), quindi
     * dopo registry_clear() anche g_cell (ancora vivo) non sarebbe piu'
     * raggiungibile con cell_getBuffer - il tearDown di questo test
     * arriverebbe troppo tardi e lascerebbe un leak (scoperto proprio
     * scrivendo questo test). */
    {
        buffer_t *b2_1 = cell_getBuffer( g_cell, "B2" );
        bufferObj_t *cur = b2_1->head;
        while ( cur != NULL ) { object_delete( cur->dato ); cur = cur->next; }
    }

    /* --- Run 2: stessa identica configurazione, ma Strategia 2 ---
     * Il registro (registry.h) e' un singleton GLOBALE ("una sola cella
     * per esecuzione", vedi registry.h): senza svuotarlo qui, B1/B2 di
     * cell2 collidono con quelli di g_cell ancora vivi (setUp/tearDown
     * di Unity girano una volta per TEST, non per singola "run" dentro
     * lo stesso test) - stesso motivo per cui main.c chiama
     * registry_clear() tra le due esecuzioni della modalita' di
     * confronto (vedi esegui_simulazione in app/main.c). */
    registry_clear();
    cell_t *cell2 = cell_create();
    cell_addBuffer( cell2, "B1", 5, &err );
    cell_addBuffer( cell2, "B2", 5, &err );
    cell_connect( cell2, "B1", "B2" );
    controllore_t *ctrl2 = controllore_create( cell2, 0.8, &err );
    controllore_impostaStrategia( ctrl2, STRATEGIA_FCFS );

    object_t *bassa = object_create( "P_bassa", 1, 'A', 0, 10.0, 5.0, &err );
    object_t *alta  = object_create( "P_alta",  9, 'A', 0, 10.0, 5.0, &err );
    object_t *media = object_create( "P_media", 5, 'A', 0, 10.0, 5.0, &err );
    controllore_ammettiArrivo( ctrl2, "B1", bassa, 0 );
    controllore_ammettiArrivo( ctrl2, "B1", alta, 0 );
    controllore_ammettiArrivo( ctrl2, "B1", media, 0 );

    {
        int step;
        buffer_t *b2_2 = cell_getBuffer( cell2, "B2" );
        for ( step = 0; step < 20 && buffer_getCount( b2_2 ) < 3; step++ ) {
            controllore_step( ctrl2, step );
        }
    }
    b2head = cell_getBuffer( cell2, "B2" )->head;
    strncpy( primo_fcfs, object_getID( b2head->dato ), IDLENGTH );

    TEST_ASSERT_EQUAL_STRING( "P_alta", primo_priorita );
    TEST_ASSERT_EQUAL_STRING( "P_bassa", primo_fcfs );
    TEST_ASSERT_NOT_EQUAL( 0, strcmp( primo_priorita, primo_fcfs ) );

    /* Pulizia della seconda cella (g_cell/g_ctrl restano a tearDown). */
    {
        buffer_t *b2_2 = cell_getBuffer( cell2, "B2" );
        bufferObj_t *cur = b2_2->head;
        while ( cur != NULL ) { object_delete( cur->dato ); cur = cur->next; }
    }
    controllore_destroy( ctrl2 );
    cell_destroy( cell2 );
}

int main( void )
{
    UNITY_BEGIN();

    RUN_TEST( test_strategia_di_default_e_priorita_buffer_aware );
    RUN_TEST( test_impostaStrategia_e_getStrategia_round_trip );
    RUN_TEST( test_impostaStrategia_su_null_restituisce_errore );

    RUN_TEST( test_strategia_priorita_smaltisce_per_priorita_decrescente );
    RUN_TEST( test_strategia_fcfs_smaltisce_per_ordine_di_arrivo );
    RUN_TEST( test_le_due_strategie_producono_ordini_diversi_sugli_stessi_dati );

    return UNITY_END();
}
