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

/* I parametri della simulazione vengono caricati da plant_config */
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
/* 1bis. Caricamento dei parametri globali della simulazione */
if ( !parser_caricaSimulazione( path_config, &sim_config, &err ) ) {
    fprintf( stderr, "Errore caricamento configurazione simulazione: %d\n", err );
    cell_destroy( cell );
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
    ctrl = controllore_create( cell, sim_config.soglia_buffer, &err );
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

/* Caricamento dello scenario */
if ( !parser_caricaScenario( path_scenario, &scenario, &err ) ) {
    fprintf( stderr, "Errore caricamento scenario '%s' (codice %d)\n",
             path_scenario, err );
    controllore_destroy( ctrl );
    cell_destroy( cell );
    return 1;
}

printf( "Scenario '%s': load_multiplier=%.2f, guasto=%s",
        scenario.nome,
        scenario.moltiplicatore_carico,
        scenario.guasto_abilitato ? "ABILITATO" : "disabilitato" );

if ( scenario.n_guasto_isp > 0 ) {
    printf( " su " );

    for ( int i = 0; i < scenario.n_guasto_isp; i++ ) {
        if ( i > 0 ) {
            printf( ", " );
        }
        printf( "%s", scenario.guasto_isp_id[i] );
    }
}

printf( "\n" );
    err = parser_applicaScenario( cell, &scenario );
    if ( err != OP_SUCCESS ) {
        fprintf( stderr, "ATTENZIONE: applicazione scenario fallita (codice %d)\n", err );
    }

    /* 6. Oggetti in ingresso, dal file oggetti. */
    int oggetti_caricati = parser_caricaOggetti( cell, ctrl, path_oggetti, BUFFER_INGRESSO, &err );
    printf( "Oggetti caricati in %s: %d\n", BUFFER_INGRESSO, oggetti_caricati );

    /* 7. Simulazione */
   for ( step = 0; step < sim_config.n_step_simulazione; step++ ) {
    controllore_step( ctrl, step );
    }

    printf( "=== Stato finale (dopo %d passi) ===\n", sim_config.n_step_simulazione );
    controllore_print( ctrl );
    printf( "Completati: %ld, ancora in coda (pending): %d\n",
            controllore_getCompletati( ctrl ), controllore_getPendingCount( ctrl ) );

   /* Gli object_t ancora presenti nelle code interne del controllore
 * vengono liberati automaticamente da controllore_destroy().
 * Gli oggetti già trasferiti ai buffer/cella vengono invece gestiti
 * dalla distruzione delle rispettive strutture. */

    controllore_destroy( ctrl );
    cell_destroy( cell );

    return 0;
}
