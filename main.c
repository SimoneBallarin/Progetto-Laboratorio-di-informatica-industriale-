/**
 * @file main.c
 * @brief Main della cella meccatronica, per il layout:
 *
 *   [ingresso] -> B1 -> ISP1 -[N1]-> M -> B2 -> ISP2 -> {Alacciaio, riqualifica, TRASH, Blrame}
 *
 * Unione dei due filoni di lavoro sul main:
 *   - scenario (sez. 7 della traccia: file di scenario passato da riga
 *     di comando, per abilitare/disabilitare il guasto sensore senza
 *     ricompilare - parser_caricaScenario/parser_applicaScenario);
 *   - statistiche e log eventi (statistiche_t/log_t, vedi statistiche.h
 *     e log.h).
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
 *   - Lo scenario (guasto abilitato/disabilitato, sez. 5.3/7) va
 *     applicato DOPO parser_collegaSensoriQualita: parser_applicaScenario
 *     richiede che il sensore di qualità sull'ISP del guasto sia già
 *     agganciato (vedi punto 3 nel corpo di main).
 */

#include <stdio.h>
#include <stdlib.h>

#include "object.h"
#include "errors.h"
#include "cell.h"
#include "Controllore.h"
#include "parser.h"
#include "statistiche.h"
#include "log.h"

/* Percorso del file di configurazione impianto: contiene sia il layout
 * della cella (BUFFER/NASTRO/MACCHINA/ISP/CONNECT/MOTORE/DEVIATORE) sia i
 * parametri di simulazione (SIM_STEPS/SIM_PEZZI/SOGLIA_BUFFER/
 * GEN_TARGET_DIMENSIONX/GEN_TARGET_RAGGIO/GEN_ERRORE_PCT). Prima erano
 * tutti #define fissi qui nel main: ora arrivano da qui. */
#define CONFIG_PATH  "lib/parser/plant_config_valid.txt"

/* Percorso di default del file di scenario (sez. 7 della traccia: deve
 * poter cambiare "senza ricompilare il programma" - per questo e'
 * sovrascrivibile da riga di comando, vedi argv[1] in main()). Stessa
 * cartella di CONFIG_PATH per coerenza; il file va creato dal gruppo
 * (non presente in questo pacchetto) con le chiavi SCENARIO_NAME/
 * LOAD_MULTIPLIER/FAULT_ENABLED/FAULT_ISP/FAULT_TIME_ERROR/
 * FAULT_TIME_OK - vedi parser_caricaScenario in parser.c. Se il file
 * non esiste, la simulazione prosegue comunque senza guasto
 * configurato (vedi punto 3 nel corpo di main). */
#define SCENARIO_PATH_DEFAULT  "lib/parser/scenario_difficile.txt"

/* Percorso del file di log eventi (sez. 2.2/7 del progetto: "log degli
 * eventi rilevanti", errori "riportati nel log"). */
#define LOG_PATH  "simulazione.log"

/* Percorso del file di riepilogo statistiche (stesso contenuto stampato
 * a schermo da statistiche_stampa, duplicato su file per poterlo
 * consultare/allegare senza dover ricopiare l'output della console). */
#define STATISTICHE_PATH  "statistiche.txt"

/* Genera un pezzo di esempio in ingresso a B1, alternando materiale e
 * "conformita'" (vicinanza al target di ISP2), giusto per vedere la
 * linea muoversi. Chiamata dal ciclo di simulazione principale, una
 * volta per ogni pezzo, passando lo STEP REALE corrente (non un indice
 * di generazione): cosi' object_getStepCreation rispecchia il vero
 * istante di ingresso nella cella, e stepOut - stepCreation e' un tempo
 * di attraversamento genuino. Il vero flusso di arrivi verra' dal file
 * di configurazione, quando il parser sara' pronto (vedi
 * parser_caricaOggetti, gia' presente in libreria ma non ancora
 * agganciato qui). */
static void genera_arrivi_esempio( cell_t *cell, statistiche_t *stats , controllore_t *ctrl, log_t *log,
                                    int step_arrivo, const char *id, char tipo, double dimensionX, double raggio )
{
    short int err;
    object_t *obj;
    buffer_t *b1;

    b1 = cell_getBuffer( cell, "B1" );
    if ( b1 == NULL || buffer_isFull( b1 ) ) {
        statistiche_registraBlocco( stats, "B1" );
        fprintf( stderr, "B1 pieno o inesistente: arrivo %s scartato\n", id );
        log_evento( log, step_arrivo, LOG_WARNING, "B1 pieno o inesistente: arrivo %s scartato", id );
        return;
    }
    short int priorita =  (short int)(rand()%11);
    obj = object_create( id, priorita, tipo, step_arrivo, dimensionX, raggio, &err );
    if ( obj == NULL ) {
        fprintf( stderr, "Errore creazione oggetto %s: %d\n", id, err );
        log_evento( log, step_arrivo, LOG_ERROR, "Errore creazione oggetto %s (codice %d)", id, err );
        return;
    }

    if ( controllore_ammettiArrivo( ctrl, "B1", obj, step_arrivo ) != OP_SUCCESS ) {
        fprintf( stderr, "Errore inserimento %s in B1\n", id );
        log_evento( log, step_arrivo, LOG_ERROR, "Errore inserimento %s in B1", id );
        object_delete( obj );
        return;
    }
    object_setLocation( obj, "B1" );

    log_evento( log, step_arrivo, LOG_INFO,
                "Arrivo %s in B1 (tipo=%c, priorita=%d, dimX=%.2f, raggio=%.2f)",
                id, tipo, priorita, dimensionX, raggio );

    /* Sensore di presenza in B1 (sez. 5.1): segnaliamo l'arrivo come
     * fronte di salita (presenza=1) in questo passo. */
    controllore_segnalaArrivo( ctrl, "B1", 0, 1 );
    controllore_segnalaArrivo( ctrl, "B1", 0, 0 );
}


// INIZIO MAIN //

int main( int argc, char *argv[] )
{
    cell_t *cell;
    controllore_t *ctrl;
    log_t *log;
    short int err, logErr;
    int step;

    SimulationConfig sim;
    int elementi_cella, attuatori_collegati;

    /* Scenario da usare in questa esecuzione: passato da riga di comando
     * (es. "./main.exe lib/parser/scenario_difficile.txt") oppure, se
     * non specificato, quello nominale di default. Cosi' si cambia
     * scenario senza ricompilare, come richiesto dalla traccia (sez. 7). */
    const char *scenario_path = ( argc > 1 ) ? argv[1] : SCENARIO_PATH_DEFAULT;

    /* Log eventi (sez. 2.2/7 del progetto): creato per primo, cosi' da
     * poter registrare anche gli errori che avvengono prima che cella e
     * controllore esistano. Se il file non e' apribile, la simulazione
     * prosegue comunque senza log (log_evento con log==NULL non fa
     * nulla): un log mancante non deve bloccare la simulazione. */
    log = log_create( LOG_PATH, false, &logErr );
    if ( log == NULL ) {
        fprintf( stderr, "Attenzione: impossibile aprire il file di log '%s' (codice %d), si procede senza\n",
                 LOG_PATH, logErr );
    } else {
        printf( "Log eventi: %s\n", LOG_PATH );
    }

    /* 0. Parametri di simulazione dal file di configurazione (prima
     * erano #define fissi qui nel main). */
    parser_caricaSimulazione( CONFIG_PATH, &sim, &err );

    cell = cell_create();
    if ( cell == NULL ) {
        fprintf( stderr, "Errore creazione cella\n" );
        log_evento( log, -1, LOG_ERROR, "Errore creazione cella" );
        log_destroy( log );
        return 1;
    }

    /* 1. Layout della cella dal file di configurazione. */
    elementi_cella = parser_costruisciCella( cell, CONFIG_PATH, &err );
    printf( "Configurazione impianto '%s': %d elementi creati\n", CONFIG_PATH, elementi_cella );
    log_evento( log, -1, LOG_INFO, "Configurazione impianto '%s': %d elementi creati", CONFIG_PATH, elementi_cella );
    if ( elementi_cella == 0 ) {
        log_evento( log, -1, LOG_ERROR, "Nessun elemento creato dal file di configurazione '%s'", CONFIG_PATH );
        cell_destroy( cell );
        log_destroy( log );
        return 1;
    }

    ctrl = controllore_create( cell, sim.soglia_buffer, &err );
    if ( ctrl == NULL ) {
        fprintf( stderr, "Errore creazione controllore: %d\n", err );
        log_evento( log, -1, LOG_ERROR, "Errore creazione controllore (codice %d)", err );
        cell_destroy( cell );
        log_destroy( log );
        return 1;
    }
    controllore_collegaLog( ctrl, log );
    
    statistiche_t *stats = statistiche_create( &err );
    statistiche_monitoraBuffer( stats, "B1" );   
    statistiche_monitoraBuffer( stats, "B2" );   
    statistiche_monitoraBuffer( stats, "B_rame" );   
    statistiche_monitoraBuffer( stats, "B_Alacciaio" );  
    statistiche_monitoraBuffer( stats, "B_riqualifica" ); 
    statistiche_monitoraBuffer( stats, "B_TRASH" );      
    statistiche_monitoraMotore( stats, "M" );  
    statistiche_monitoraISP( stats, "ISP1" );  
    statistiche_monitoraISP( stats, "ISP2" );             
    statistiche_monitoraSensorePresenza( stats, "B1" );

    /* 2. Motore/Deviatore dal file di configurazione. */
    attuatori_collegati = parser_collegaAttuatori( ctrl, CONFIG_PATH, &err );
    int sensori_qualita = parser_collegaSensoriQualita( ctrl, CONFIG_PATH, &err );
    int sensori_buffer = parser_collegaSensoriBuffer( ctrl, CONFIG_PATH, &err );
        printf( "Sensori di buffer collegati: %d\n", sensori_buffer );
    int sensori_presenza = parser_collegaSensoriPresenza( ctrl, CONFIG_PATH, &err );
    printf( "Sensori di presenza collegati: %d\n", sensori_presenza );
    printf( "Sensori di qualita' collegati: %d\n", sensori_qualita );
    printf( "Attuatori collegati: %d\n", attuatori_collegati );
    log_evento( log, -1, LOG_INFO,
                "Sensori/attuatori collegati: buffer=%d presenza=%d qualita'=%d attuatori=%d",
                sensori_buffer, sensori_presenza, sensori_qualita, attuatori_collegati );
    if ( attuatori_collegati == 0 ) {
        log_evento( log, -1, LOG_ERROR, "Nessun attuatore collegato: impianto non valido" );
        controllore_destroy( ctrl );
        cell_destroy( cell );
        log_destroy( log );
        return 1;
    }

    /* 3. Scenario (guasto abilitato/disabilitato + i suoi tempi, sez. 5.3
     * e 7 del progetto): va DOPO parser_collegaSensoriQualita, perche'
     * parser_applicaScenario richiede che il sensore di qualita'
     * sull'ISP del guasto sia gia' agganciato. */
    {
        ScenarioConfig scenario;
        int scenario_letto = parser_caricaScenario( scenario_path, &scenario, &err );
        if ( scenario_letto ) {
            short int err_scenario = parser_applicaScenario( ctrl, &scenario );
            printf( "Scenario '%s' applicato: guasto %s su %s (err=%d)\n",
                    scenario.nome, scenario.guasto_abilitato ? "ABILITATO" : "disabilitato",
                    scenario.guasto_isp_id, err_scenario );
            log_evento( log, -1, ( err_scenario == OP_SUCCESS ) ? LOG_INFO : LOG_ERROR,
                        "Scenario '%s' (%s) applicato: guasto %s su '%s' (err=%d)",
                        scenario_path, scenario.nome,
                        scenario.guasto_abilitato ? "ABILITATO" : "disabilitato",
                        scenario.guasto_isp_id, err_scenario );
        } else {
            fprintf( stderr, "Scenario '%s' non caricato, si prosegue senza guasto configurato\n", scenario_path );
            log_evento( log, -1, LOG_WARNING,
                        "Scenario '%s' non caricato, si prosegue senza guasto configurato", scenario_path );
        }
    }

    /* Backlog iniziale (sez. 2.2/6 del progetto): tutti i pezzi di prova
     * arrivano in un unico burst allo STEP REALE 0 - non prima del ciclo
     * con uno pseudo-step come nella versione con il bug (il loro indice
     * di generazione, che non corrispondeva a nessun istante reale), ma
     * come primo evento del ciclo di simulazione vero e proprio. Questo
     * fa partire B1 gia' pieno, cosi' che buffer_removeObject scelga
     * davvero tra piu' candidati in base alla priorita' (Strategia 1,
     * sez. 4.1) invece di trovare quasi sempre un solo pezzo in coda, e
     * le statistiche di occupazione buffer partono da un livello
     * realistico invece che da zero. stepCreation=0 per tutti resta
     * corretto (non un compromesso): rappresenta fedelmente il fatto che
     * sono tutti presenti fin dal primo istante della simulazione, quindi
     * stepOut - stepCreation misura il vero tempo totale in sistema
     * (coda + attraversamento). */
    {
        char id[16];
        int idx;
        for ( idx = 0; idx < sim.n_pezzi_prova; idx++ ) {
            char tipo;
            if ( rand() % 2 == 0 ) {
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
            genera_arrivi_esempio( cell, stats, ctrl, log, 0, id, tipo, dimensionX, raggio );
        }
    }

    printf( "=== Stato iniziale (dopo il backlog di ingresso, prima di far girare la simulazione) ===\n" );
    controllore_print( ctrl );
    printf( "\n" );

    log_evento( log, -1, LOG_INFO, "Avvio simulazione: %d passi", sim.n_step_simulazione );

    /* Contatori "precedenti" di anomalie del sensore di qualita', per
     * registrare nel log solo il MOMENTO in cui una nuova anomalia
     * viene rilevata (non il totale cumulativo ad ogni passo). Se
     * un'ISP non ha nessun sensore agganciato, il getter restituisce un
     * codice ERR_* negativo: il confronto "> prev" resta sempre falso,
     * quindi non serve nessun controllo aggiuntivo qui. */
    long anomalieISP1_prev = 0;
    long anomalieISP2_prev = 0;

    for ( step = 0; step < sim.n_step_simulazione; step++ ) {
        controllore_step( ctrl, step );
        statistiche_campiona( stats, ctrl );

        long anomalieISP1 = controllore_getAnomalieQualita( ctrl, "ISP1" );
        if ( anomalieISP1 > anomalieISP1_prev ) {
            log_evento( log, step, LOG_WARNING,
                        "Sensore di qualita' ISP1 in anomalia (anomalie totali=%ld)", anomalieISP1 );
            anomalieISP1_prev = anomalieISP1;
        }
        long anomalieISP2 = controllore_getAnomalieQualita( ctrl, "ISP2" );
        if ( anomalieISP2 > anomalieISP2_prev ) {
            log_evento( log, step, LOG_WARNING,
                        "Sensore di qualita' ISP2 in anomalia (anomalie totali=%ld)", anomalieISP2 );
            anomalieISP2_prev = anomalieISP2;
        }
    }

    printf( "=== Stato finale (dopo %d passi) ===\n", sim.n_step_simulazione );
    controllore_print( ctrl );
    printf( "Completati: %ld, ancora in coda (pending): %d\n",
            controllore_getCompletati( ctrl ), controllore_getPendingCount( ctrl ) );
    log_evento( log, sim.n_step_simulazione, LOG_INFO,
                "Fine simulazione: completati=%ld, ancora in coda (pending)=%d",
                controllore_getCompletati( ctrl ), controllore_getPendingCount( ctrl ) );

    // STATISTICHE //
    {
    const char *buffer_terminali[] = { "B_Alacciaio", "B_riqualifica", "B_TRASH", "B_rame" };
    int nb = (int) ( sizeof( buffer_terminali ) / sizeof( buffer_terminali[0] ) );
    int bi;
    for ( bi = 0; bi < nb; bi++ ) {
        buffer_t *b = cell_getBuffer( cell, buffer_terminali[bi] );
        bufferObj_t *cur = b->head;
        while ( cur != NULL ) {
            statistiche_registraCompletamento( stats, cur->dato, 40 ); /* 40 = scadenza in passi, a piacere/ soglia di completamento */
            cur = cur->next;
        }
    }
}


    statistiche_stampa( stats, ctrl, sim.n_step_simulazione, STATISTICHE_PATH );
    statistiche_destroy( stats ); 

    log_stampaRiepilogo( log );
    log_destroy( log );

    /* NB: la pulizia degli object_t inseriti resta da fare (nessun
     * modulo del progetto li libera automaticamente): qui li lasciamo
     * volutamente, dato che serve prima decidere chi ne è responsabile
     * a fine simulazione (o se il progetto li considera "persi" nei
     * buffer di uscita finché non c'è un log/registro dedicato). */

    controllore_destroy( ctrl );
    cell_destroy( cell );

    return 0;
}
