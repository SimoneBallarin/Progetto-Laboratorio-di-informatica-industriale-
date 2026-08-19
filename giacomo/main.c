/**
 * @file main.c
 * @brief Main della cella meccatronica, ora costruita a partire dai file
 *        di configurazione (impianto, oggetti, scenario) invece che da
 *        parametri hardcoded — vedi lib/Parser.
 *
 * Uso: ./programma <config_impianto> <oggetti> <scenario>
 * Se i tre argomenti non sono forniti, usa i file in examples/ di default.
 */

#include <stdio.h>
#include <stdlib.h>

#include "object.h"
#include "errors.h"
#include "cell.h"
#include "Controllore.h"
#include "parser.h"

#define SOGLIA_BUFFER       0.8
#define N_STEP_SIMULAZIONE  60
#define BUFFER_INGRESSO     "B1"

int main( int argc, char *argv[] )
{
    const char *path_config    = ( argc > 1 ) ? argv[1] : "examples/plant_config.txt";
    const char *path_oggetti   = ( argc > 2 ) ? argv[2] : "examples/objects_nominal.txt";
    const char *path_scenario  = ( argc > 3 ) ? argv[3] : "examples/scenario_nominal.txt";

    cell_t *cell;
    controllore_t *ctrl;
    short int err;
    int step;

    /* 1. Cella vuota */
    cell = cell_create();
    if ( cell == NULL ) {
        fprintf( stderr, "Errore creazione cella\n" );
        return 1;
    }

    /* 2. Costruzione della cella dal file di configurazione impianto:
     *    crea buffer/nastri/macchine/ISP e li collega. */
    int elementi = parser_costruisciCella( cell, path_config, &err );
    printf( "Configurazione impianto '%s': %d elementi creati\n", path_config, elementi );
    if ( elementi == 0 ) {
        fprintf( stderr, "Nessun elemento creato dalla configurazione: interrompo\n" );
        cell_destroy( cell );
        return 1;
    }

    /* 3. Il controllore va creato DOPO aver aggiunto tutti i buffer:
     *    crea automaticamente un SensoreBuffer per ognuno di quelli
     *    già presenti in questo momento (vedi Controllore.h). */
    ctrl = controllore_create( cell, SOGLIA_BUFFER, &err );
    if ( ctrl == NULL ) {
        fprintf( stderr, "Errore creazione controllore: %d\n", err );
        cell_destroy( cell );
        return 1;
    }

    /* 4. Collegamento di Motori/Deviatori, sempre dal file di
     *    configurazione impianto (righe MOTORE/DEVIATORE). */
    int attuatori = parser_collegaAttuatori( ctrl, path_config, &err );
    printf( "Attuatori collegati: %d\n", attuatori );

    /* 4bis. Ora che gli attuatori sono collegati, verifichiamo la
     *       coerenza della cella (es. Deviatore presente dove serve). */
    err = cell_validateAll( cell );
    if ( err != OP_SUCCESS ) {
        fprintf( stderr, "ATTENZIONE: cell_validateAll ha trovato un problema (codice %d)\n", err );
    }

    /* 5. Scenario: applica (o disattiva esplicitamente) il guasto del
     *    sensore di qualità configurato nel file di scenario. */
    ScenarioConfig scenario;
    parser_caricaScenario( path_scenario, &scenario, &err );
    printf( "Scenario '%s': load_multiplier=%.2f, guasto=%s su %s\n",
            scenario.nome, scenario.moltiplicatore_carico,
            scenario.guasto_abilitato ? "ABILITATO" : "disabilitato",
            scenario.guasto_isp_id );
    err = parser_applicaScenario( cell, &scenario );
    if ( err != OP_SUCCESS ) {
        fprintf( stderr, "ATTENZIONE: applicazione scenario fallita (codice %d)\n", err );
    }

    /* 6. Oggetti in ingresso, dal file oggetti. */
    int oggetti_caricati = parser_caricaOggetti( cell, ctrl, path_oggetti, BUFFER_INGRESSO, &err );
    printf( "Oggetti caricati in %s: %d\n", BUFFER_INGRESSO, oggetti_caricati );

    /* 7. Simulazione */
    for ( step = 0; step < N_STEP_SIMULAZIONE; step++ ) {
        controllore_step( ctrl, step );
    }

    printf( "=== Stato finale (dopo %d passi) ===\n", N_STEP_SIMULAZIONE );
    controllore_print( ctrl );
    printf( "Completati: %ld, ancora in coda (pending): %d\n",
            controllore_getCompletati( ctrl ), controllore_getPendingCount( ctrl ) );

    /* NB: la pulizia degli object_t inseriti resta da fare (nessun
     * modulo del progetto li libera automaticamente): vedi nota nel
     * messaggio di revisione — da decidere insieme al gruppo. */

    controllore_destroy( ctrl );
    cell_destroy( cell );

    return 0;
}
