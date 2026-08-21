/**
 * @file main.c
 * @brief Main della cella meccatronica, per il layout:
 *
 *   [ingresso] -> B1 -> ISP1 -[N1]-> M -> B2 -> ISP2 -> {Alacciaio, riqualifica, TRASH, Blrame}
 *
 * ASSUNZIONI FATTE (da confermare/correggere col gruppo):
 *   - ISP1 ("che materiale è"): non fa alcun controllo qualità reale.
 *     Il materiale è già scritto su object->type alla creazione
 *     dell'oggetto (object_create), quindi ISP1 è semplicemente una
 *     stazione con tempo di attraversamento fisso, un solo ingresso e
 *     una sola uscita (verso N1) — l'esito che calcola internamente
 *     (S_Qualita è comunque agganciato, perché isp_t lo richiede) non
 *     viene usato per instradare, dato che ha un'unica uscita.
 *   - Un solo Nastro (N1) nel layout, tra ISP1 e M, con Motore collegato
 *     (arco blu nel disegno). M -> B2 è un collegamento diretto (arco
 *     grigio, nessun nastro dedicato nel disegno).
 *   - ISP2 (quella con 4 uscite) ha il Deviatore collegato. L'ordine dei
 *     collegamenti verso le uscite è FISSO e deve rispettare la
 *     convenzione aggiunta in Controllore.c:
 *         indice 0 -> CONFORME materiale 'A' -> Alacciaio
 *         indice 1 -> RIVALUTAZIONE           -> riqualifica
 *         indice 2 -> SCARTO                  -> TRASH
 *         indice 3 -> CONFORME materiale 'B'  -> Blrame
 *   - Ogni buffer ha un SensoreBuffer automatico (lo crea
 *     controllore_create per tutti i buffer già presenti nella cella in
 *     quel momento: per questo B1/B2/le 4 uscite vanno aggiunte PRIMA di
 *     chiamare controllore_create).
 *   - Il sensore di presenza (S_Presenza) è collegato solo a B1, l'unico
 *     punto di ingresso di nuovi oggetti dall'esterno (sez. 5.1 del
 *     progetto: "Sensore di presenza in B1"), tramite
 *     controllore_segnalaArrivo ad ogni passo in cui arriva un pezzo.
 */

#include <stdio.h>
#include <stdlib.h>

#include "object.h"
#include "errors.h"
#include "cell.h"
#include "Controllore.h"
#include "parser.h"

/* Percorso del file di configurazione impianto: contiene sia il layout
 * della cella (BUFFER/NASTRO/MACCHINA/ISP/CONNECT/MOTORE/DEVIATORE) sia i
 * parametri di simulazione (SIM_STEPS/SIM_PEZZI/SOGLIA_BUFFER/
 * GEN_TARGET_DIMENSIONX/GEN_TARGET_RAGGIO/GEN_ERRORE_PCT). Prima erano
 * tutti #define fissi qui nel main: ora arrivano da qui. */
#define CONFIG_PATH  "lib/parser/plant_config_valid.txt"

/* Genera qualche oggetto di esempio in ingresso a B1, alternando
 * materiale e "conformità" (vicinanza al target di ISP2), giusto per
 * vedere la linea muoversi. Il vero flusso di arrivi verrà dal file di
 * configurazione, quando il parser sarà pronto. */
static void genera_arrivi_esempio( cell_t *cell, controllore_t *ctrl, int step_arrivo,
                                    const char *id, char tipo, double dimensionX, double raggio )
{
    short int err;
    object_t *obj;
    buffer_t *b1;

    b1 = cell_getBuffer( cell, "B1" );
    if ( b1 == NULL || buffer_isFull( b1 ) ) {
        fprintf( stderr, "B1 pieno o inesistente: arrivo %s scartato\n", id );
        return;
    }
    short int priorita =  (short int)(rand()%11);
    obj = object_create( id, priorita, tipo, step_arrivo, dimensionX, raggio, &err );
    if ( obj == NULL ) {
        fprintf( stderr, "Errore creazione oggetto %s: %d\n", id, err );
        return;
    }

    if ( buffer_insertObject( b1, obj, true ) != OP_SUCCESS ) {
        fprintf( stderr, "Errore inserimento %s in B1\n", id );
        object_delete( obj );
        return;
    }
    object_setLocation( obj, "B1" );

    /* Sensore di presenza in B1 (sez. 5.1): segnaliamo l'arrivo come
     * fronte di salita (presenza=1) in questo passo. */
    controllore_segnalaArrivo( ctrl, "B1", 0, 1 );
    controllore_segnalaArrivo( ctrl, "B1", 0, 0 );
}


// INIZIO MAIN //

int main( void )
{
    cell_t *cell;
    controllore_t *ctrl;
    short int err;
    int step;

    SimulationConfig sim;
    int elementi_cella, attuatori_collegati;

    /* 0. Parametri di simulazione dal file di configurazione (prima
     * erano #define fissi qui nel main). */
    parser_caricaSimulazione( CONFIG_PATH, &sim, &err );

    cell = cell_create();
    if ( cell == NULL ) {
        fprintf( stderr, "Errore creazione cella\n" );
        return 1;
    }

    /* 1. Layout della cella dal file di configurazione. */
    elementi_cella = parser_costruisciCella( cell, CONFIG_PATH, &err );
    printf( "Configurazione impianto '%s': %d elementi creati\n", CONFIG_PATH, elementi_cella );
    if ( elementi_cella == 0 ) {
        cell_destroy( cell );
        return 1;
    }

    ctrl = controllore_create( cell, sim.soglia_buffer, &err );
    if ( ctrl == NULL ) {
        fprintf( stderr, "Errore creazione controllore: %d\n", err );
        cell_destroy( cell );
        return 1;
    }

    /* 2. Motore/Deviatore dal file di configurazione. */
    attuatori_collegati = parser_collegaAttuatori( ctrl, CONFIG_PATH, &err );
    int sensori_qualita = parser_collegaSensoriQualita( ctrl, CONFIG_PATH, &err );
    int sensori_buffer = parser_collegaSensoriBuffer( ctrl, CONFIG_PATH, &err );
        printf( "Sensori di buffer collegati: %d\n", sensori_buffer );
    int sensori_presenza = parser_collegaSensoriPresenza( ctrl, CONFIG_PATH, &err );
    printf( "Sensori di presenza collegati: %d\n", sensori_presenza );
    printf( "Sensori di qualita' collegati: %d\n", sensori_qualita );
    printf( "Attuatori collegati: %d\n", attuatori_collegati );
    if ( attuatori_collegati == 0 ) {
        controllore_destroy( ctrl );
        cell_destroy( cell );
        return 1;
    }

    {
        char id[16];
        int idx;
        for ( idx = 0; idx < sim.n_pezzi_prova; idx++ ) {
            char tipo;
            if (rand() % 2 == 0) {
                tipo = 'A';
             } else {
                tipo = 'B';
              }
            /* Errore casuale in un intervallo di +/- sim.gen_errore_pct
             * punti percentuali (stessa logica di Simone, ma con
             * l'ampiezza letta da file invece che fissa a 2). */
            double scarto_pct = (double) ( rand() % ( 2 * sim.gen_errore_pct + 1 ) - sim.gen_errore_pct );
            double dimensionX = sim.gen_target_dimensionX + ( sim.gen_target_dimensionX * scarto_pct / 100.0 );
            double raggio     = sim.gen_target_raggio     + ( sim.gen_target_raggio     * scarto_pct / 100.0 );

            snprintf( id, sizeof( id ), "P%d", idx + 1 );
            genera_arrivi_esempio( cell, ctrl, idx, id, tipo, dimensionX, raggio );
        }
    }

    printf( "=== Stato iniziale (dopo la creazione degli oggetti, prima di far girare la simulazione) ===\n" );
    controllore_print( ctrl );
    printf( "\n" );

    for ( step = 0; step < sim.n_step_simulazione; step++ ) {
        controllore_step( ctrl, step );
    }

    printf( "=== Stato finale (dopo %d passi) ===\n", sim.n_step_simulazione );
    controllore_print( ctrl );
    printf( "Completati: %ld, ancora in coda (pending): %d\n",
            controllore_getCompletati( ctrl ), controllore_getPendingCount( ctrl ) );

    // STATISTICHE //

    /* NB: la pulizia degli object_t inseriti resta da fare (nessun
     * modulo del progetto li libera automaticamente): qui li lasciamo
     * volutamente, dato che serve prima decidere chi ne è responsabile
     * a fine simulazione (o se il progetto li considera "persi" nei
     * buffer di uscita finché non c'è un log/registro dedicato). */

    controllore_destroy( ctrl );
    cell_destroy( cell );

    return 0;
}
