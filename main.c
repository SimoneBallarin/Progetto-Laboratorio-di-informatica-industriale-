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
 *
 * MODALITA' D'USO (riga di comando, o argomento nel launch.json di VSCode):
 *   ./main.exe [config_path] [oggetti_path] [scenario_path]
 *       Tutti e tre opzionali (sez. 8/10 della traccia: "poter essere
 *       eseguito da linea di comando indicando almeno i file di
 *       configurazione, oggetti e scenario"):
 *         - config_path: default lib/parser/plant_config_valid.txt
 *         - oggetti_path: NESSUN default - se omesso (o "-"), gli
 *           arrivi sono generati casualmente come sempre (SIM_PEZZI
 *           pezzi a step 0); se specificato, gli arrivi sono letti dal
 *           file, ciascuno al proprio ARRIVAL_STEP (vedi
 *           parser_caricaOggetti in parser.c). Esempio pronto:
 *           lib/parser/oggetti_esempio.txt
 *         - scenario_path: default lib/parser/scenario_nominale.txt
 *       Esempi:
 *         ./main.exe
 *             comportamento storico, config/scenario di default, arrivi casuali
 *         ./main.exe lib/parser/plant_config_valid.txt - lib/parser/scenario_difficile.txt
 *             config esplicito, arrivi casuali ("-"), scenario difficile
 *         ./main.exe lib/parser/plant_config_valid.txt lib/parser/oggetti_esempio.txt
 *             arrivi letti dal file oggetti, scenario di default
 *
 *       OGNI esecuzione fa SEMPRE, in automatico, senza bisogno di
 *       nessun flag aggiuntivo: simula con la Strategia 1 (priorità +
 *       buffer-aware) e ne stampa il resoconto completo, poi rilancia
 *       la STESSA identica configurazione (stessi file, stesso seed)
 *       con la Strategia 2 (FCFS, vedi strategia_controllo_t in
 *       Controllore.h) e stampa un confronto affiancato tra le due
 *       (vedi stampaConfronto/CONFRONTO_PATH). Le due esecuzioni
 *       condividono lo stesso seed (vedi SEED_ESECUZIONE) cosi' che gli
 *       arrivi (casuali o da file) siano gli stessi in entrambe: le
 *       differenze nei risultati riflettono quindi solo la strategia di
 *       controllo, non un caso diverso di dati in ingresso.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "errors.h"
#include "cell.h"
#include "Controllore.h"
#include "parser.h"
#include "statistiche.h"
#include "log.h"
#include "registry.h"

/* Percorso del file di configurazione impianto: contiene sia il layout
 * della cella (BUFFER/NASTRO/MACCHINA/ISP/CONNECT/MOTORE/DEVIATORE) sia i
 * parametri di simulazione (SIM_STEPS/SIM_PEZZI/SOGLIA_BUFFER/
 * GEN_TARGET_DIMENSIONX/GEN_TARGET_RAGGIO/GEN_ERRORE_PCT). Prima erano
 * tutti #define fissi qui nel main: ora arrivano da qui. */
#define CONFIG_PATH_DEFAULT  "lib/parser/plant_config_valid.txt"

/* Percorso di default del file di scenario (sez. 7 della traccia: deve
 * poter cambiare "senza ricompilare il programma" - per questo e'
 * sovrascrivibile da riga di comando, vedi argv[] in main()). */
#define SCENARIO_PATH_DEFAULT  "lib/parser/scenario_nominale.txt"

/* Percorso di default del file oggetti (sez. 8/10 della traccia:
 * "almeno i file di configurazione, oggetti e scenario" da riga di
 * comando). NON e' usato automaticamente come gli altri due default: se
 * l'utente non passa esplicitamente un percorso oggetti su riga di
 * comando (argv[2], vedi main()), la simulazione ricade sul generatore
 * casuale storico (SIM_PEZZI pezzi con dimensioni intorno a
 * GEN_TARGET_DIMENSIONX/RAGGIO, vedi il blocco "Backlog iniziale" sotto)
 * invece di aprire questo file - tenuto qui solo come constante pronta
 * da passare esplicitamente (es. da .vscode/launch.json) per provare il
 * caricamento da file senza doverne scrivere uno nuovo. */
#define OGGETTI_PATH_DEFAULT  "lib/parser/oggetti_esempio.txt"

/* Stessa logica di OGGETTI_PATH_DEFAULT sopra, ma per B2 (argv[4], vedi
 * main()): NON usata automaticamente, solo una constante pronta per
 * provare il caricamento da file senza doverne scrivere uno nuovo. */
#define OGGETTI_B2_PATH_DEFAULT  "lib/parser/oggetti_b2_esempio.txt"

/* Percorso del file di log eventi (sez. 2.2/7 del progetto: "log degli
 * eventi rilevanti", errori "riportati nel log"). */
#define LOG_PATH  "simulazione.log"

/* Percorso del file di riepilogo statistiche (stesso contenuto stampato
 * a schermo da statistiche_stampa, duplicato su file per poterlo
 * consultare/allegare senza dover ricopiare l'output della console). */
#define STATISTICHE_PATH  "statistiche.txt"

/* Percorsi usati per il resoconto della Strategia 2 (rilanciata in
 * automatico su ogni esecuzione, vedi doc in testa al file) e per il
 * confronto vero e proprio - la Strategia 1 scrive invece sui percorsi
 * "storici" LOG_PATH/STATISTICHE_PATH sopra, per non cambiare dove il
 * resoconto principale e' sempre stato. */
#define LOG_PATH_STRATEGIA2        "simulazione_strategia2.log"
#define STATISTICHE_PATH_STRATEGIA2 "statistiche_strategia2.txt"
#define CONFRONTO_PATH             "confronto_strategie.txt"

/* Seed fisso per rand(), riapplicato con srand() all'inizio di OGNI
 * esecuzione (vedi esegui_simulazione): senza questo, la seconda run
 * (Strategia 2) erediterebbe lo stato di rand() lasciato dalla prima
 * (nessuna delle due sarebbe seminata esplicitamente, vedi assenza di
 * srand() nel resto del progetto), generando arrivi diversi da quelli
 * della prima run e rendendo il confronto tra strategie non pulito
 * (differenze dovute in parte al caso, non solo alla strategia). Il
 * valore 1u riproduce anche lo stesso comportamento di rand() che
 * questo main ha sempre avuto per la Strategia 1 (il C standard
 * garantisce che rand() senza una srand() precedente si comporti come
 * se srand(1) fosse stata chiamata). */
#define SEED_ESECUZIONE  1u

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
/**
 * @brief Crea un oggetto di prova e lo inserisce in un buffer di
 *        ingresso qualunque (parametro bufferID: "B1" per il backlog
 *        storico, "B2" per il pre-caricamento opzionale - vedi
 *        SIM_PEZZI_B2 in plant_config_valid.txt).
 *
 * Generalizzata da una versione precedente che scriveva "B1" fisso in
 * tre punti diversi della funzione: usata ora anche per B2, senza
 * duplicare la logica.
 */
static void genera_arrivi_esempio( cell_t *cell, statistiche_t *stats, controllore_t *ctrl, log_t *log,
                                    const char *bufferID, int step_arrivo, const char *id, char tipo,
                                    double dimensionX, double raggio )
{
    short int err;
    object_t *obj;
    buffer_t *b;

    b = cell_getBuffer( cell, bufferID );
    if ( b == NULL || buffer_isFull( b ) ) {
        statistiche_registraBlocco( stats, bufferID );
        fprintf( stderr, "%s pieno o inesistente: arrivo %s scartato\n", bufferID, id );
        log_evento( log, step_arrivo, LOG_WARNING, "%s pieno o inesistente: arrivo %s scartato", bufferID, id );
        return  ;
    }
    short int priorita =  (short int)(rand()%11);
    obj = object_create( id, priorita, tipo, step_arrivo, dimensionX, raggio, &err );
    if ( obj == NULL ) {
        fprintf( stderr, "Errore creazione oggetto %s: %d\n", id, err );
        log_evento( log, step_arrivo, LOG_ERROR, "Errore creazione oggetto %s (codice %d)", id, err );
        return  ;
    }

    if ( controllore_ammettiArrivo( ctrl, bufferID, obj, step_arrivo ) != OP_SUCCESS ) {
        fprintf( stderr, "Errore inserimento %s in %s\n", id, bufferID );
        log_evento( log, step_arrivo, LOG_ERROR, "Errore inserimento %s in %s", id, bufferID );
        object_delete( obj );
        return  ;
    }
    object_setLocation( obj, bufferID );

    log_evento( log, step_arrivo, LOG_INFO,
                "Arrivo %s in %s (tipo=%c, priorita=%d, dimX=%.2f, raggio=%.2f)",
                id, bufferID, tipo, priorita, dimensionX, raggio );

    /* Sensore di presenza (sez. 5.1): segnaliamo l'arrivo come
     * fronte di salita (presenza=1) in questo passo. */
    controllore_segnalaArrivo( ctrl, bufferID, 0, 1 );
    controllore_segnalaArrivo( ctrl, bufferID, 0, 0 );
}


/**
 * @brief Libera TUTTI gli object_t ancora presenti nella cella a fine
 *        simulazione: nei buffer, sui nastri, e dentro macchine/ISP
 *        eventualmente ancora occupate in quel momento.
 *
 * Nessun modulo del progetto libera mai il payload object_t da solo -
 * buffer_delete/nastro_delete/machine_delete/isp_delete lo documentano
 * esplicitamente ("resta di proprietà di chi lo ha creato"): senza
 * questa pulizia, ogni object_t creato con object_create diventa un
 * memory leak quando cell_destroy libera le strutture che lo contenevano
 * (confermato con AddressSanitizer/LeakSanitizer). Va chiamata DOPO che
 * il ciclo di simulazione è finito e PRIMA di cell_destroy - gli
 * object_t ancora nella coda pending del Controllore sono invece
 * responsabilità di controllore_destroy (vedi Controllore.h), perché
 * quella coda è privata e main non può raggiungerla.
 * @param cell Puntatore alla cella.
 */
static void libera_tutti_gli_oggetti( cell_t *cell )
{
    int n, idx;
    char ID[IDLENGTH];

    /* Buffer: ogni bufferObj_t wrapper puo' contenere un object_t. */
    n = cell_getBufferCount( cell );
    for ( idx = 0; idx < n; idx++ ) {
        if ( cell_getBufferIDAt( cell, idx, ID ) != OP_SUCCESS ) { continue; }
        buffer_t *b = cell_getBuffer( cell, ID );
        if ( b == NULL ) { continue; }
        bufferObj_t *cur = b->head;
        while ( cur != NULL ) {
            object_delete( cur->dato );
            cur = cur->next;
        }
    }

    /* Nastri: stesso schema dei buffer (lista di nastroObj_t). */
    n = cell_getNastroCount( cell );
    for ( idx = 0; idx < n; idx++ ) {
        if ( cell_getNastroIDAt( cell, idx, ID ) != OP_SUCCESS ) { continue; }
        nastro_t *nas = cell_getNastro( cell, ID );
        if ( nas == NULL ) { continue; }
        nastroObj_t *cur = nas->head;
        while ( cur != NULL ) {
            object_delete( cur->dato );
            cur = cur->next;
        }
    }

    /* Macchine: al massimo un oggetto alla volta, se ancora occupate
     * (es. lavorazione interrotta a metà dalla fine della simulazione,
     * vedi il caso "P16" discusso in conversazione). */
    n = cell_getMachineCount( cell );
    for ( idx = 0; idx < n; idx++ ) {
        if ( cell_getMachineIDAt( cell, idx, ID ) != OP_SUCCESS ) { continue; }
        machine_t *m = cell_getMachine( cell, ID );
        if ( m != NULL && m->oggetto_in_lavorazione != NULL ) {
            object_delete( m->oggetto_in_lavorazione );
        }
    }

    /* ISP: stesso schema delle macchine. */
    n = cell_getISPCount( cell );
    for ( idx = 0; idx < n; idx++ ) {
        if ( cell_getISPIDAt( cell, idx, ID ) != OP_SUCCESS ) { continue; }
        isp_t *i = cell_getISP( cell, ID );
        if ( i != NULL && i->oggetto_in_controllo != NULL ) {
            object_delete( i->oggetto_in_controllo );
        }
    }
}


// INIZIO MAIN //

/**
 * @brief Esegue UNA simulazione completa (build cella, backlog, ciclo di
 *        simulazione, statistiche, teardown) con la strategia indicata,
 *        e restituisce il riepilogo aggregato delle metriche.
 *
 * Corpo unico usato sia dall'esecuzione singola (comportamento storico
 * di questo main) sia dalla modalita' "--confronto" (due chiamate a
 * questa funzione, una per strategia): evita di mantenere due copie
 * dell'orchestrazione cella/controllore/statistiche che potrebbero
 * divergere nel tempo.
 *
 * Chiama srand(seed) come primo passo: senza questo, rand() (usato per
 * generare gli arrivi qui e per la tolleranza di lavorazione in
 * machine.c) partirebbe dal seed di default (1) alla prima chiamata di
 * processo, ma da uno stato indefinito/ereditato a ogni chiamata
 * successiva nello stesso processo (rilevante per "--confronto", che
 * chiama questa funzione due volte) - vedi SEED_ESECUZIONE.
 *
 * @param config_path Percorso del file di configurazione impianto.
 * @param oggetti_path Percorso del file oggetti in ingresso, o NULL per
 *        usare il generatore casuale storico (vedi "Backlog iniziale"
 *        nel corpo della funzione) invece di leggere da file.
 * @param scenario_path Percorso del file di scenario (guasto sensore).
 * @param strategia Strategia di controllo da usare per questa esecuzione.
 * @param seed Seed per srand(), riapplicato a inizio funzione.
 * @param log_path Percorso del file di log eventi per questa esecuzione.
 * @param statistiche_path Percorso del file statistiche per questa esecuzione.
 * @param stampa_stato_console Se true, stampa su stdout lo stato
 *        iniziale/finale della cella (controllore_print) come nella
 *        versione storica del main - disattivato in modalita'
 *        "--confronto" per non affollare la console con due run intere
 *        prima della tabella di confronto vera e propria (restano
 *        comunque disponibili nei rispettivi file di log/statistiche).
 * @param out_riepilogo Destinazione del riepilogo aggregato (vedi
 *        statistiche_getRiepilogo in statistiche.h). Allocato dal
 *        chiamante; non toccato se la funzione fallisce prima di
 *        raggiungere il ciclo di simulazione (vedi valore di ritorno).
 * @return OP_SUCCESS se l'esecuzione e' arrivata in fondo e
 *         out_riepilogo e' stato scritto, un codice ERR_* altrimenti
 *         (vedi errors.h) - stessa casistica di fallimento gestita nella
 *         versione originale del main (cella non creata, config vuota,
 *         nessun attuatore collegato).
 */
static short int esegui_simulazione( const char *config_path, const char *oggetti_path, const char *scenario_path,
                                      const char *oggetti_b2_path,
                                      strategia_controllo_t strategia,
                                      unsigned int seed, const char *log_path, const char *statistiche_path,
                                      bool stampa_stato_console, statistiche_riepilogo_t *out_riepilogo )
{
    cell_t *cell;
    controllore_t *ctrl;
    log_t *log;
    statistiche_t *stats;
    short int err, logErr;
    int step;

    SimulationConfig sim;
    int elementi_cella, attuatori_collegati;

    srand( seed );

    /* Log eventi (sez. 2.2/7 del progetto): creato per primo, cosi' da
     * poter registrare anche gli errori che avvengono prima che cella e
     * controllore esistano. Se il file non e' apribile, la simulazione
     * prosegue comunque senza log (log_evento con log==NULL non fa
     * nulla): un log mancante non deve bloccare la simulazione. */
    log = log_create( log_path, false, &logErr );
    if ( log == NULL ) {
        fprintf( stderr, "Attenzione: impossibile aprire il file di log '%s' (codice %d), si procede senza\n",
                 log_path, logErr );
    } else {
        printf( "Log eventi: %s\n", log_path );
    }

    /* 0. Parametri di simulazione dal file di configurazione (prima
     * erano #define fissi qui nel main). */
    parser_caricaSimulazione( config_path, &sim, &err );

    cell = cell_create();
    if ( cell == NULL ) {
        fprintf( stderr, "Errore creazione cella\n" );
        log_evento( log, -1, LOG_ERROR, "Errore creazione cella" );
        log_destroy( log );
        return ERR_ALLOC;
    }

    /* 1. Layout della cella dal file di configurazione. */
    elementi_cella = parser_costruisciCella( cell, config_path, &err );
    printf( "Configurazione impianto '%s': %d elementi creati\n", config_path, elementi_cella );
    log_evento( log, -1, LOG_INFO, "Configurazione impianto '%s': %d elementi creati", config_path, elementi_cella );
    if ( elementi_cella == 0 ) {
        log_evento( log, -1, LOG_ERROR, "Nessun elemento creato dal file di configurazione '%s'", config_path );
        cell_destroy( cell );
        log_destroy( log );
        registry_clear();
        return ERR_NOT_FOUND;
    }

    ctrl = controllore_create( cell, sim.soglia_buffer, &err );
    if ( ctrl == NULL ) {
        fprintf( stderr, "Errore creazione controllore: %d\n", err );
        log_evento( log, -1, LOG_ERROR, "Errore creazione controllore (codice %d)", err );
        cell_destroy( cell );
        log_destroy( log );
        registry_clear();
        return err;
    }
    controllore_collegaLog( ctrl, log );

    /* Strategia di controllo per QUESTA esecuzione (vedi
     * strategia_controllo_t in Controllore.h): impostata subito dopo
     * controllore_create, PRIMA di collegare sensori/attuatori e di
     * inserire il primo oggetto, cosi' che valga fin dal primo
     * inserimento (vedi doc di controllore_impostaStrategia). */
    controllore_impostaStrategia( ctrl, strategia );
    printf( "Strategia di controllo: %s\n",
            ( strategia == STRATEGIA_FCFS ) ? "Strategia 2 (FCFS)" : "Strategia 1 (priorita' + buffer-aware)" );
    log_evento( log, -1, LOG_INFO, "Strategia di controllo: %s",
                ( strategia == STRATEGIA_FCFS ) ? "Strategia 2 (FCFS)" : "Strategia 1 (priorita' + buffer-aware)" );

    stats = statistiche_create( &err );
    statistiche_monitoraBuffer( stats, "B1" );
    statistiche_monitoraBuffer( stats, "B2" );
    statistiche_monitoraBuffer( stats, "B_rame" );
    statistiche_monitoraBuffer( stats, "B_Alacciaio" );
    statistiche_monitoraBuffer( stats, "B_riqualifica" );
    statistiche_monitoraBuffer( stats, "B_TRASH" );
    statistiche_monitoraMotore( stats, "M" );
    statistiche_monitoraMotore( stats, "N1" );
    statistiche_monitoraISP( stats, "ISP1" );
    statistiche_monitoraISP( stats, "ISP2" );
    statistiche_monitoraSensorePresenza( stats, "B1" );

    /* 2. Motore/Deviatore dal file di configurazione. */
    attuatori_collegati = parser_collegaAttuatori( ctrl, config_path, &err );
    int sensori_qualita = parser_collegaSensoriQualita( ctrl, config_path, &err );
    int sensori_buffer = parser_collegaSensoriBuffer( ctrl, config_path, &err );
    printf( "Sensori di buffer collegati: %d\n", sensori_buffer );
    int sensori_presenza = parser_collegaSensoriPresenza( ctrl, config_path, &err );
    printf( "Sensori di presenza collegati: %d\n", sensori_presenza );
    printf( "Sensori di qualita' collegati: %d\n", sensori_qualita );
    printf( "Attuatori collegati: %d\n", attuatori_collegati );
    log_evento( log, -1, LOG_INFO,
                "Sensori/attuatori collegati: buffer=%d presenza=%d qualita'=%d attuatori=%d",
                sensori_buffer, sensori_presenza, sensori_qualita, attuatori_collegati );
    if ( attuatori_collegati == 0 ) {
        log_evento( log, -1, LOG_ERROR, "Nessun attuatore collegato: impianto non valido" );
        statistiche_destroy( stats );
        controllore_destroy( ctrl );
        cell_destroy( cell );
        log_destroy( log );
        registry_clear();
        return ERR_NOT_FOUND;
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
            /* scenario.guasto_isp_id e' un elenco (una o piu' ISP, vedi
             * FAULT_ISP in parser.h): costruito qui un'unica stringa
             * "ISP1, ISP2" per stampa/log, invece di stampare un array
             * direttamente (non stampabile con %s). */
            char elencoISP[128] = "";
            int idx;
            for ( idx = 0; idx < scenario.n_guasto_isp; idx++ ) {
                if ( idx > 0 ) { strncat( elencoISP, ", ", sizeof( elencoISP ) - strlen( elencoISP ) - 1 ); }
                strncat( elencoISP, scenario.guasto_isp_id[idx], sizeof( elencoISP ) - strlen( elencoISP ) - 1 );
            }
            if ( scenario.n_guasto_isp == 0 ) { strncpy( elencoISP, "(nessuna)", sizeof( elencoISP ) - 1 ); }

            printf( "Scenario '%s' applicato: guasto %s su %s (err=%d)\n",
                    scenario.nome, scenario.guasto_abilitato ? "ABILITATO" : "disabilitato",
                    elencoISP, err_scenario );
            log_evento( log, -1, ( err_scenario == OP_SUCCESS ) ? LOG_INFO : LOG_ERROR,
                        "Scenario '%s' (%s) applicato: guasto %s su '%s' (err=%d)",
                        scenario_path, scenario.nome,
                        scenario.guasto_abilitato ? "ABILITATO" : "disabilitato",
                        elencoISP, err_scenario );
        } else {
            fprintf( stderr, "Scenario '%s' non caricato, si prosegue senza guasto configurato\n", scenario_path );
            log_evento( log, -1, LOG_WARNING,
                        "Scenario '%s' non caricato, si prosegue senza guasto configurato", scenario_path );
        }
    }

    /* Backlog iniziale (sez. 2.2/6/8 del progetto): se e' stato passato
     * un file oggetti (oggetti_path != NULL), i pezzi arrivano da li',
     * ciascuno al proprio ARRIVAL_STEP (vedi parser_caricaOggetti,
     * schedulazione reale nel tempo). Altrimenti si ricade sul
     * generatore casuale storico: tutti i pezzi di prova arrivano in un
     * unico burst allo STEP REALE 0 - non prima del ciclo con uno
     * pseudo-step come nella versione con il bug (il loro indice di
     * generazione, che non corrispondeva a nessun istante reale), ma
     * come primo evento del ciclo di simulazione vero e proprio. Questo
     * fa partire B1 gia' pieno, cosi' che buffer_removeObject scelga
     * davvero tra piu' candidati in base alla priorita' (Strategia 1,
     * sez. 4.1) invece di trovare quasi sempre un solo pezzo in coda, e
     * le statistiche di occupazione buffer partono da un livello
     * realistico invece che da zero. stepCreation=0 per tutti resta
     * corretto in questo caso (non un compromesso): rappresenta
     * fedelmente il fatto che sono tutti presenti fin dal primo istante
     * della simulazione, quindi stepOut - stepCreation misura il vero
     * tempo totale in sistema (coda + attraversamento). */
    if ( oggetti_path != NULL ) {
        int oggetti_caricati = parser_caricaOggetti( cell, ctrl, oggetti_path, "B1", &err );
        printf( "File oggetti '%s': %d oggetti schedulati\n", oggetti_path, oggetti_caricati );
        log_evento( log, -1, LOG_INFO, "File oggetti '%s': %d oggetti schedulati", oggetti_path, oggetti_caricati );
        if ( oggetti_caricati == 0 ) {
            fprintf( stderr, "Attenzione: nessun oggetto caricato da '%s', la simulazione partira' senza arrivi\n",
                     oggetti_path );
            log_evento( log, -1, LOG_WARNING, "Nessun oggetto caricato da '%s'", oggetti_path );
        }
        /* SIM_PEZZI (config impianto) e' letto SEMPRE da
         * parser_caricaSimulazione (sim.n_pezzi_prova), ma usato SOLO
         * nel ramo "else" sotto (generatore casuale) - con un file
         * oggetti esplicito viene semplicemente ignorato. Avviso
         * esplicito qui, altrimenti chi cambia SIM_PEZZI aspettandosi un
         * effetto (avendo pero' anche passato un file oggetti) resta
         * confuso nel non vederne nessuno. */
        if ( sim.n_pezzi_prova > 0 ) {
            printf( "Nota: SIM_PEZZI=%d nel file di configurazione viene ignorato (in uso il file oggetti)\n",
                    sim.n_pezzi_prova );
            log_evento( log, -1, LOG_INFO, "SIM_PEZZI=%d ignorato: in uso il file oggetti '%s'",
                        sim.n_pezzi_prova, oggetti_path );
        }
    } else {
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
            genera_arrivi_esempio( cell, stats, ctrl, log, "B1", 0, id, tipo, dimensionX, raggio );
        }
    }

    /* Pre-caricamento opzionale di B2: due modi, mutuamente esclusivi
     * (stessa logica di B1 sopra) - se e' stato passato un file oggetti
     * per B2 (oggetti_b2_path != NULL), i pezzi arrivano da li' (ciascuno
     * al proprio ARRIVAL_STEP, stesso meccanismo generico di
     * parser_caricaOggetti usato per B1 - la funzione accetta gia'
     * QUALUNQUE buffer di destinazione, non solo "B1"); altrimenti si
     * ricade sul generatore casuale storico (SIM_PEZZI_B2). Applicato
     * SEMPRE, indipendentemente da come e' stato riempito B1 sopra: i
     * due backlog (B1/B2) sono completamente indipendenti tra loro. */
    if ( oggetti_b2_path != NULL ) {
        int oggetti_b2_caricati = parser_caricaOggetti( cell, ctrl, oggetti_b2_path, "B2", &err );
        printf( "File oggetti B2 '%s': %d oggetti schedulati\n", oggetti_b2_path, oggetti_b2_caricati );
        log_evento( log, -1, LOG_INFO, "File oggetti B2 '%s': %d oggetti schedulati",
                    oggetti_b2_path, oggetti_b2_caricati );
        if ( oggetti_b2_caricati == 0 ) {
            fprintf( stderr, "Attenzione: nessun oggetto caricato da '%s' per B2\n", oggetti_b2_path );
            log_evento( log, -1, LOG_WARNING, "Nessun oggetto caricato da '%s' per B2", oggetti_b2_path );
        }
        if ( sim.n_pezzi_prova_b2 > 0 ) {
            printf( "Nota: SIM_PEZZI_B2=%d nel file di configurazione viene ignorato (in uso il file oggetti per B2)\n",
                    sim.n_pezzi_prova_b2 );
            log_evento( log, -1, LOG_INFO, "SIM_PEZZI_B2=%d ignorato: in uso il file oggetti B2 '%s'",
                        sim.n_pezzi_prova_b2, oggetti_b2_path );
        }
    } else if ( sim.n_pezzi_prova_b2 > 0 ) {
        /* Generatore casuale storico: stessa logica di generazione usata
         * per B1 (stesso target/errore percentuale), dato che un pezzo
         * pre-caricato in B2 rappresenta un pezzo che ha gia'
         * virtualmente attraversato ISP1/N1/M prima dell'inizio della
         * simulazione, quindi non ha senso dargli caratteristiche
         * diverse da quelle generate per l'ingresso in B1. */
        char id[16];
        int idx;
        printf( "Pre-caricamento B2: %d pezzi (SIM_PEZZI_B2)\n", sim.n_pezzi_prova_b2 );
        log_evento( log, -1, LOG_INFO, "Pre-caricamento B2: %d pezzi (SIM_PEZZI_B2)", sim.n_pezzi_prova_b2 );
        for ( idx = 0; idx < sim.n_pezzi_prova_b2; idx++ ) {
            char tipo;
            if ( rand() % 2 == 0 ) {
                tipo = 'A';
            } else {
                tipo = 'B';
            }
            double scarto_pct = (double) ( rand() % ( 2 * sim.gen_errore_pct + 1 ) - sim.gen_errore_pct );
            double dimensionX = sim.gen_target_dimensionX + ( sim.gen_target_dimensionX * scarto_pct / 100.0 );
            double raggio     = sim.gen_target_raggio     + ( sim.gen_target_raggio     * scarto_pct / 100.0 );

            snprintf( id, sizeof( id ), "PB2_%d", idx + 1 );
            genera_arrivi_esempio( cell, stats, ctrl, log, "B2", 0, id, tipo, dimensionX, raggio );
        }
    }

    if ( stampa_stato_console ) {
        printf( "=== Stato iniziale (dopo il backlog di ingresso, prima di far girare la simulazione) ===\n" );
        controllore_print( ctrl );
        printf( "\n" );
    }

    log_evento( log, -1, LOG_INFO, "Avvio simulazione: %d passi", sim.n_step_simulazione );

    /* Contatori "precedenti" di anomalie/blocchi-per-guasto del sensore
     * di qualita', per registrare nel log solo il MOMENTO in cui un
     * nuovo evento viene rilevato (non il totale cumulativo ad ogni
     * passo). Se un'ISP non ha nessun sensore agganciato, il getter
     * restituisce un codice ERR_* negativo: il confronto "> prev" resta
     * sempre falso, quindi non serve nessun controllo aggiuntivo qui. */
    long anomalieISP1_prev = 0;
    long anomalieISP2_prev = 0;
    long bloccoGuastoISP1_prev = 0;
    long bloccoGuastoISP2_prev = 0;

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

        /* Guasto = stazione indisponibile (vedi processISP in
         * Controllore.c): il contatore cresce di 1 per ogni passo
         * bloccato, quindi durante un episodio di guasto lungo compare
         * una riga di log per ogni passo (stesso schema gia' usato sopra
         * per le anomalie) - utile per vedere a schermo per quanti passi
         * di fila una stazione e' rimasta ferma, non solo che lo e'
         * stata. */
        long bloccoGuastoISP1 = controllore_getStepBloccoGuasto( ctrl, "ISP1" );
        if ( bloccoGuastoISP1 > bloccoGuastoISP1_prev ) {
            log_evento( log, step, LOG_WARNING,
                        "ISP1 indisponibile per guasto: pezzo trattenuto (episodi totali=%ld)", bloccoGuastoISP1 );
        }
        bloccoGuastoISP1_prev = bloccoGuastoISP1;
        long bloccoGuastoISP2 = controllore_getStepBloccoGuasto( ctrl, "ISP2" );
        if ( bloccoGuastoISP2 > bloccoGuastoISP2_prev ) {
            log_evento( log, step, LOG_WARNING,
                        "ISP2 indisponibile per guasto: pezzo trattenuto (episodi totali=%ld)", bloccoGuastoISP2 );
        }
        bloccoGuastoISP2_prev = bloccoGuastoISP2;
    }

    if ( stampa_stato_console ) {
        printf( "=== Stato finale (dopo %d passi) ===\n", sim.n_step_simulazione );
        controllore_print( ctrl );
        printf( "Completati: %ld, ancora in coda (pending): %d, arrivi non ancora entrati (schedulati): %d\n",
                controllore_getCompletati( ctrl ), controllore_getPendingCount( ctrl ),
                controllore_getArriviSchedulatiCount( ctrl ) );
    }
    log_evento( log, sim.n_step_simulazione, LOG_INFO,
                "Fine simulazione: completati=%ld, ancora in coda (pending)=%d, arrivi schedulati non entrati=%d",
                controllore_getCompletati( ctrl ), controllore_getPendingCount( ctrl ),
                controllore_getArriviSchedulatiCount( ctrl ) );

    // STATISTICHE //
    {
        const char *buffer_terminali[] = { "B_Alacciaio", "B_riqualifica", "B_TRASH", "B_rame" };
        int nb = (int) ( sizeof( buffer_terminali ) / sizeof( buffer_terminali[0] ) );
        int bi;
        for ( bi = 0; bi < nb; bi++ ) {
            buffer_t *b = cell_getBuffer( cell, buffer_terminali[bi] );
            if ( b == NULL ) { continue; }
            bufferObj_t *cur = b->head;
            while ( cur != NULL ) {
                statistiche_registraCompletamento( stats, cur->dato, 40 ); /* 40 = scadenza in passi, a piacere/ soglia di completamento */
                cur = cur->next;
            }
        }
    }

    statistiche_stampa( stats, ctrl, sim.n_step_simulazione, statistiche_path );

    if ( out_riepilogo != NULL ) {
        statistiche_getRiepilogo( stats, ctrl, out_riepilogo );
    }

    statistiche_destroy( stats );

    log_stampaRiepilogo( log );
    log_destroy( log );

    /* Libera tutti gli object_t rimasti nella cella (buffer, nastri,
     * macchine/ISP eventualmente ancora occupate): va fatto DOPO aver
     * letto i buffer terminali per le statistiche appena sopra (altrimenti
     * statistiche_registraCompletamento leggerebbe oggetti già liberati),
     * e PRIMA di cell_destroy - vedi doc di libera_tutti_gli_oggetti. Gli
     * object_t ancora in coda pending sono liberati da controllore_destroy
     * (coda privata del Controllore, main non puo' raggiungerla). */
    libera_tutti_gli_oggetti( cell );

    controllore_destroy( ctrl );
    cell_destroy( cell );

    /* Svuota il registro globale (vedi registry.h: "una sola cella per
     * esecuzione") prima di un'eventuale prossima chiamata a questa
     * funzione nello STESSO processo (modalita' "--confronto"): senza
     * questo, la seconda esecuzione troverebbe ID già presenti nel
     * registro (es. "B1_SB") e ogni registry_add fallirebbe con
     * ERR_DUPLICATE, lasciando i sensori della seconda run scollegati
     * dal registro (letture/tracciabilità inconsistenti). I puntatori
     * liberati sopra (cell_destroy/controllore_destroy) sono l'unica
     * cosa a cui il registro puntava: registry_clear libera solo i nodi
     * del registro stesso, mai le entita' a cui puntavano (già libere). */
    registry_clear();

    return OP_SUCCESS;
}

/**
 * @brief Stampa (su stdout e, se path_output != NULL, anche su file) un
 *        confronto affiancato tra due riepiloghi (sez. "confronto tra
 *        strategie di controllo").
 */
static void stampaConfronto( const statistiche_riepilogo_t *r1, const statistiche_riepilogo_t *r2,
                              const char *scenario_path, int n_step, const char *path_output )
{
    FILE *f = NULL;

    if ( path_output != NULL ) {
        f = fopen( path_output, "w" );
        if ( f == NULL ) {
            fprintf( stderr, "stampaConfronto: impossibile aprire '%s', si procede solo su stdout\n", path_output );
        }
    }

#define RIGA_CONFRONTO( etichetta, fmt, v1, v2 ) \
    do { \
        printf( "  %-38s " fmt "    " fmt "\n", etichetta, v1, v2 ); \
        if ( f != NULL ) { fprintf( f, "  %-38s " fmt "    " fmt "\n", etichetta, v1, v2 ); } \
    } while ( 0 )

    printf( "\n=== CONFRONTO STRATEGIE DI CONTROLLO ===\n" );
    printf( "Scenario: %s   Passi di simulazione: %d\n", scenario_path, n_step );
    printf( "  %-38s %-18s  %-18s\n", "Metrica", "Strategia 1 (prio)", "Strategia 2 (FCFS)" );
    if ( f != NULL ) {
        fprintf( f, "=== CONFRONTO STRATEGIE DI CONTROLLO ===\n" );
        fprintf( f, "Scenario: %s   Passi di simulazione: %d\n", scenario_path, n_step );
        fprintf( f, "  %-38s %-18s  %-18s\n", "Metrica", "Strategia 1 (prio)", "Strategia 2 (FCFS)" );
    }

    RIGA_CONFRONTO( "Oggetti completati (totale)", "%-18ld", r1->totale_completati, r2->totale_completati );
    RIGA_CONFRONTO( "Compl. entro scad. SISTEMA (coda+processo) %", "%-18.1f", r1->perc_entro_scadenza_tempo_sistema, r2->perc_entro_scadenza_tempo_sistema );
    RIGA_CONFRONTO( "Tempo medio SISTEMA (coda+processo)", "%-18.1f", r1->tempo_medio_sistema, r2->tempo_medio_sistema );
    RIGA_CONFRONTO( "Compl. entro scad. PROCESSO (solo pipeline) %", "%-18.1f", r1->perc_entro_scadenza_tempo_processo, r2->perc_entro_scadenza_tempo_processo );
    RIGA_CONFRONTO( "Tempo medio PROCESSO (solo pipeline, no coda)", "%-18.1f", r1->tempo_medio_processo, r2->tempo_medio_processo );
    RIGA_CONFRONTO( "Occupazione media buffer (%)", "%-18.1f", r1->occupazione_media_buffer, r2->occupazione_media_buffer );
    RIGA_CONFRONTO( "Occupazione massima buffer (%)", "%-18d", r1->occupazione_massima_buffer, r2->occupazione_massima_buffer );
    RIGA_CONFRONTO( "Blocchi (buffer pieno)", "%-18ld", r1->totale_blocchi, r2->totale_blocchi );
    RIGA_CONFRONTO( "Anomalie sensore qualita'", "%-18ld", r1->totale_anomalie_qualita, r2->totale_anomalie_qualita );
    RIGA_CONFRONTO( "Ancora in coda a fine sim. (pending)", "%-18d", r1->pending_finale, r2->pending_finale );
    RIGA_CONFRONTO( "Entro scad. SISTEMA, priorita' alta (>=7) %", "%-18.1f", r1->perc_entro_scadenza_alta_priorita, r2->perc_entro_scadenza_alta_priorita );
    RIGA_CONFRONTO( "Entro scad. SISTEMA, priorita' bassa (<=3) %", "%-18.1f", r1->perc_entro_scadenza_bassa_priorita, r2->perc_entro_scadenza_bassa_priorita );

#undef RIGA_CONFRONTO

    printf( "\nNota: Strategia 1 = priorita' + ammissione buffer-aware (sez. 4.1); "
            "Strategia 2 = First-Come-First-Served, nessuna garanzia su priorita' o buffer.\n" );
    printf( "Le due righe 'entro scadenza per fascia di priorita'' mostrano il compromesso discusso in "
            "sez. 2.1 del progetto preliminare: le medie aggregate possono restare simili tra le due "
            "strategie (proprieta' nota delle code a singolo servitore: il tempo di attesa totale medio "
            "non dipende dall'ordine di servizio), ma CHI aspetta di piu' cambia radicalmente.\n" );
    printf( "Stesso seed (%u) e stesso scenario in entrambe le run: gli arrivi in ingresso sono identici, "
            "le differenze sopra riflettono solo la strategia di controllo.\n", SEED_ESECUZIONE );
    if ( f != NULL ) {
        fprintf( f, "\nNota: Strategia 1 = priorita' + ammissione buffer-aware (sez. 4.1); "
                 "Strategia 2 = First-Come-First-Served, nessuna garanzia su priorita' o buffer.\n" );
        fprintf( f, "Le due righe 'entro scadenza per fascia di priorita'' mostrano il compromesso discusso in "
                 "sez. 2.1 del progetto preliminare: le medie aggregate possono restare simili tra le due "
                 "strategie (proprieta' nota delle code a singolo servitore: il tempo di attesa totale medio "
                 "non dipende dall'ordine di servizio), ma CHI aspetta di piu' cambia radicalmente.\n" );
        fprintf( f, "Stesso seed (%u) e stesso scenario in entrambe le run: gli arrivi in ingresso sono identici, "
                 "le differenze sopra riflettono solo la strategia di controllo.\n", SEED_ESECUZIONE );
        fclose( f );
    }
}

int main( int argc, char *argv[] )
{
    /* CLI (sez. 8/10 della traccia: "poter essere eseguito da linea di
     * comando indicando almeno i file di configurazione, oggetti e
     * scenario"):
     *   ./main.exe [config_path] [oggetti_path] [scenario_path] [oggetti_b2_path]
     * Tutti e quattro opzionali, con default ragionevoli se omessi (vedi
     * CONFIG_PATH_DEFAULT/SCENARIO_PATH_DEFAULT sopra) - "./main.exe"
     * senza argomenti continua a funzionare come sempre.
     *
     * oggetti_path e oggetti_b2_path sono casi speciali: se omessi (o
     * passati come "-"), NESSUN file oggetti viene aperto e si ricade
     * sul generatore casuale storico (SIM_PEZZI/SIM_PEZZI_B2, vedi
     * "Backlog iniziale" in esegui_simulazione) - non sono file "di
     * default" come gli altri due, perche' introdurre un file vero
     * cambia il comportamento della simulazione (arrivi scaglionati nel
     * tempo invece che tutti a step 0): vanno scelti esplicitamente. Per
     * provare il caricamento da file senza scriverne uno nuovo, passare
     * OGGETTI_PATH_DEFAULT ("lib/parser/oggetti_esempio.txt") come
     * secondo argomento, o OGGETTI_B2_PATH_DEFAULT
     * ("lib/parser/oggetti_b2_esempio.txt") come quarto.
     *
     * oggetti_b2_path e' stato aggiunto DOPO scenario_path (non subito
     * dopo oggetti_path) per non spostare la posizione degli argomenti
     * gia' esistenti: chi lanciava gia' "./main.exe config oggetti
     * scenario" continua a funzionare identico, il quarto argomento e'
     * puramente additivo.
     *
     * OGNI esecuzione fa SEMPRE due cose, in automatico, senza bisogno
     * di flag aggiuntivi:
     *   1. Simula con la Strategia 1 (priorità + buffer-aware) e ne
     *      stampa il resoconto completo su console (comportamento
     *      storico di questo main, invariato) più LOG_PATH/STATISTICHE_PATH.
     *   2. Rilancia LA STESSA identica configurazione (stessi file,
     *      stesso seed) con la Strategia 2 (FCFS) in silenzio (report
     *      completo comunque disponibile in STATISTICHE_PATH_STRATEGIA2),
     *      e stampa/scrive un confronto affiancato tra le due (sez.
     *      "confronto tra due esecuzioni o due strategie" della traccia). */
    const char *config_path = ( argc > 1 ) ? argv[1] : CONFIG_PATH_DEFAULT;
    const char *oggetti_path = NULL;
    if ( argc > 2 && argv[2][0] != '\0' && strcmp( argv[2], "-" ) != 0 ) {
        oggetti_path = argv[2];
    }
    const char *scenario_path = ( argc > 3 ) ? argv[3] : SCENARIO_PATH_DEFAULT;
    const char *oggetti_b2_path = NULL;
    if ( argc > 4 && argv[4][0] != '\0' && strcmp( argv[4], "-" ) != 0 ) {
        oggetti_b2_path = argv[4];
    }

    SimulationConfig sim_per_report;
    short int err_sim, esito1, esito2;
    statistiche_riepilogo_t r1, r2;

    parser_caricaSimulazione( config_path, &sim_per_report, &err_sim );

    /* Seed 1u: stesso comportamento di rand() usato (implicitamente,
     * senza mai chiamare srand) dalle versioni precedenti di questo main
     * per la Strategia 1 - il C standard garantisce che rand() senza una
     * srand() precedente si comporti come se srand(1) fosse stata
     * chiamata. La Strategia 2 riusa lo STESSO seed (non uno diverso):
     * cosi' i due run vedono esattamente gli stessi arrivi in ingresso
     * (rilevante solo per il generatore casuale: con un file oggetti
     * esplicito gli arrivi sono comunque identici tra le due run, dato
     * che vengono letti dallo stesso file), e le differenze nei
     * risultati riflettono solo la strategia. */
    esito1 = esegui_simulazione( config_path, oggetti_path, scenario_path, oggetti_b2_path, STRATEGIA_PRIORITA_BUFFER_AWARE, SEED_ESECUZIONE,
                                  LOG_PATH, STATISTICHE_PATH, true, &r1 );

    esito2 = esegui_simulazione( config_path, oggetti_path, scenario_path, oggetti_b2_path, STRATEGIA_FCFS, SEED_ESECUZIONE,
                                  LOG_PATH_STRATEGIA2, STATISTICHE_PATH_STRATEGIA2, false, &r2 );

    if ( esito1 != OP_SUCCESS ) {
        fprintf( stderr, "Simulazione principale non completata (codice %d)\n", esito1 );
        return OP_SUCCESS;
    }
    if ( esito2 != OP_SUCCESS ) {
        /* La run principale e' comunque andata a buon fine (statistiche.txt
         * e' gia' stato scritto sopra): solo il confronto non e' disponibile. */
        fprintf( stderr, "Confronto automatico non completato (codice %d): resoconto principale comunque disponibile in '%s'\n",
                 esito2, STATISTICHE_PATH );
        return 0;
    }

    stampaConfronto( &r1, &r2, scenario_path, sim_per_report.n_step_simulazione, CONFRONTO_PATH );
    return 0;
}
