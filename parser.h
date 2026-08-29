/**
 * @file parser.h
 * @brief Modulo "parser": legge i file di configurazione impianto,
 *        oggetti e scenario, e costruisce/popola la cella usando SOLO
 *        le funzioni pubbliche di cell.h, object.h e Controllore.h (mai
 *        buffer_create/machine_create/... direttamente, come da
 *        convenzione descritta in cell.h).
 *
 * Filosofia di gestione errori (coerente con la traccia, sez. 8): una
 * riga malformata viene segnalata su stderr con il numero di riga e
 * SCARTATA, ma il parsing dell'intero file continua. Ogni funzione
 * restituisce quante entita' ha creato con successo, cosi' il chiamante
 * (main) puo' decidere se la configurazione caricata e' sufficiente per
 * avviare la simulazione.
 */

#ifndef PARSER_H
#define PARSER_H

#include "object.h"
#include "errors.h"
#include "cell.h"
#include "Controllore.h"

/**
 * @brief Legge il file di configurazione impianto e costruisce la cella:
 *        crea buffer/nastri/macchine/ISP (righe BUFFER/NASTRO/MACCHINA/ISP)
 *        e li collega (righe CONNECT), chiamando esclusivamente
 *        cell_addBuffer/cell_addNastro/cell_addMachine/cell_addISP/
 *        cell_connect.
 *
 * Formato di riga atteso (chiave-valore separate da virgola):
 *   BUFFER,ID=B1,CAPACITY=10
 *   NASTRO,ID=N1,CAPACITY=2,VELOCITA=2
 *   MACCHINA,ID=M,TEMPO=3
 *   ISP,ID=ISP1,TEMPO=1,DIMX_TARGET=100,RAGGIO_TARGET=10
 *   CONNECT,FROM=B1,TO=ISP1
 *
 * L'ordine delle righe conta: un CONNECT richiede che FROM e TO siano
 * gia' stati creati da una riga precedente.
 *
 * @param cell Cella gia' creata (cell_create), in cui costruire il layout.
 * @param path Percorso del file di configurazione impianto.
 * @param errCode puntatore opzionale (puo' essere NULL) in cui viene
 *        scritto OP_SUCCESS se il file e' stato aperto e almeno un
 *        elemento e' stato creato, o un codice ERR_* (vedi errors.h)
 *        altrimenti (es. ERR_NOT_FOUND se il file non si apre).
 * @return Numero di elementi (buffer + nastri + macchine + ISP +
 *         collegamenti) creati con successo.
 */
int parser_costruisciCella( cell_t *cell, const char *path, short int *errCode );

/**
 * @brief Legge il file di configurazione impianto e collega Motori/
 *        Deviatori gia' esistenti nel controllore alle entita' della
 *        cella, chiamando esclusivamente controllore_collegaMotore /
 *        controllore_collegaDeviatore.
 *
 * Va chiamata DOPO parser_costruisciCella (servono gia' i nastri/ISP a
 * cui agganciare Motore/Deviatore) e DOPO controllore_create (serve gia'
 * il controllore a cui agganciarli).
 *
 * Formato di riga atteso, nello stesso file di configurazione impianto:
 *   MOTORE,NASTRO=N1,VELOCITA=5000,ACCEL=2000
 *   DEVIATORE,ISP=ISP2,TEMPO_MIN_COMMUT=3
 *
 * @param ctrl Controllore gia' creato (controllore_create).
 * @param path Percorso del file di configurazione impianto (lo stesso
 *        passato a parser_costruisciCella: le righe MOTORE/DEVIATORE
 *        possono convivere con BUFFER/NASTRO/MACCHINA/ISP/CONNECT).
 * @param errCode puntatore opzionale (puo' essere NULL), vedi
 *        parser_costruisciCella.
 * @return Numero di attuatori collegati con successo.
 */
int parser_collegaAttuatori( controllore_t *ctrl, const char *path, short int *errCode );

/**
 * @brief Legge il file di configurazione impianto e aggancia un
 *        SensoreBuffer (tramite controllore_collegaSensoreBuffer) a
 *        ogni buffer trovato in una riga BUFFER. Necessario: da questa
 *        versione di Controllore.c i sensori di buffer NON vengono piu'
 *        creati automaticamente da controllore_create.
 * @return Numero di sensori di buffer collegati con successo.
 */
int parser_collegaSensoriBuffer( controllore_t *ctrl, const char *path, short int *errCode );

/**
 * @brief Legge il file di configurazione impianto e aggancia un
 *        SensorePresenza (tramite controllore_collegaSensorePresenza) a
 *        ogni ID trovato in una riga INGRESSO,ID=... . Necessario: da
 *        questa versione di Controllore.c, controllore_segnalaArrivo
 *        fallisce con ERR_NOT_FOUND se nessuno ha prima agganciato un
 *        sensore di presenza a quell'ID.
 * @return Numero di sensori di presenza collegati con successo.
 */
int parser_collegaSensoriPresenza( controllore_t *ctrl, const char *path, short int *errCode );

/**
 * @brief Legge il file di configurazione impianto e collega, tramite
 *        controllore_collegaSensoreQualita, un sensore di qualita' a
 *        ogni ISP la cui riga specifica target > 0 (le ISP "passacarte"
 *        con DIMX_TARGET=0 e RAGGIO_TARGET=0 vengono saltate: non fanno
 *        nessun controllo qualita', per design - vedi isp.h).
 * @param ctrl Controllore gia' creato (controllore_create).
 * @param path Percorso del file di configurazione impianto.
 * @param errCode puntatore opzionale (puo' essere NULL).
 * @return Numero di sensori di qualita' collegati con successo.
 */
int parser_collegaSensoriQualita( controllore_t *ctrl, const char *path, short int *errCode );

/**
 * @brief Legge il file oggetti e SCHEDULA (vedi controllore_schedulaArrivo)
 *        l'ingresso di ciascuno nel buffer indicato, al proprio
 *        ARRIVAL_STEP - non li inserisce subito: ogni oggetto entra
 *        davvero nella cella solo quando controllore_step raggiunge lo
 *        step indicato dal file, con arrivi realisticamente distribuiti
 *        nel tempo invece che tutti in un unico blocco al caricamento.
 *
 * Formato CSV atteso, con header opzionale:
 *   ID,PRIORITY,TYPE,ARRIVAL_STEP,DIMENSIONX,RAGGIO
 *   P001,2,A,0,101.0,10.5
 *
 * Per ogni riga valida: object_create (stepCreation = ARRIVAL_STEP) ->
 * controllore_schedulaArrivo (SensoreBuffer/SensorePresenza aggiornati
 * correttamente al momento dell'ammissione effettiva, non qui).
 *
 * @param cell Cella gia' costruita (deve gia' contenere bufferIngressoID:
 *        usata solo per verificare che esista, l'inserimento vero e
 *        proprio passa da ctrl).
 * @param ctrl Controllore gia' creato.
 * @param path Percorso del file oggetti.
 * @param bufferIngressoID ID del buffer di ingresso (es. "B1") in cui
 *        far entrare tutti gli oggetti letti, ciascuno al proprio
 *        ARRIVAL_STEP.
 * @param errCode puntatore opzionale (puo' essere NULL), vedi
 *        parser_costruisciCella.
 * @return Numero di oggetti schedulati con successo (non ancora
 *         necessariamente entrati nella cella: vedi
 *         controllore_getArriviSchedulatiCount per quanti sono ancora
 *         in attesa in un dato momento).
 */
int parser_caricaOggetti( cell_t *cell, controllore_t *ctrl, const char *path,
                           const char *bufferIngressoID, short int *errCode );

/** Numero massimo di ISP guaste contemporaneamente in uno scenario (vedi
 * FAULT_ISP in parser_caricaScenario: elenco separato da virgole, es.
 * "FAULT_ISP=ISP1,ISP2"). Più che sufficiente per questo impianto (solo
 * ISP1/ISP2 esistono), tenuto basso apposta: uno scenario con 10 ISP
 * guaste contemporaneamente non avrebbe senso da leggere/discutere. */
#define MAX_GUASTO_ISP 4

/**
 * @brief Configurazione di scenario: cosa varia tra "scenario nominale"
 *        e "scenario difficile" senza dover ricompilare (sez. 7 della
 *        traccia).
 */
typedef struct {
    char   nome[64];
    double moltiplicatore_carico;   /* riservato per usi futuri (es. generazione arrivi) */
    bool   guasto_abilitato;
    char   guasto_isp_id[MAX_GUASTO_ISP][IDLENGTH]; /* una o piu' ISP: "FAULT_ISP=ISP1,ISP2" nel file (virgola, senza spazi) */
    int    n_guasto_isp;            /* quante entrate valide ci sono in guasto_isp_id (0 = nessun guasto da applicare) */
    int    guasto_tempo_errore;     /* passi di funzionamento OK prima del guasto, STESSO valore per tutte le ISP elencate */
    int    guasto_tempo_ok;         /* passi di guasto prima di tornare OK, STESSO valore per tutte le ISP elencate */
} ScenarioConfig;

/**
 * @brief Legge il file di scenario in una ScenarioConfig.
 * @param path Percorso del file di scenario.
 * @param out Struct da riempire (viene azzerata e reinizializzata coi default).
 * @param errCode puntatore opzionale (puo' essere NULL), vedi
 *        parser_costruisciCella.
 * @return 1 se il file e' stato letto correttamente (anche con qualche
 *         riga scartata), 0 se il file non si apre.
 */
int parser_caricaScenario( const char *path, ScenarioConfig *out, short int *errCode );

/**
 * @brief Applica una ScenarioConfig gia' letta: chiama
 *        controllore_impostaGuastoQualita su OGNI ISP elencata in
 *        guasto_isp_id (una o piu', vedi FAULT_ISP nel file di
 *        scenario), con lo stesso guasto_abilitato/guasto_tempo_errore/
 *        guasto_tempo_ok per tutte. Richiede che un SensoreQualita sia
 *        GIA' stato agganciato a ciascuna di quelle ISP con
 *        parser_collegaSensoriQualita, altrimenti quella specifica ISP
 *        viene saltata con un avviso su stderr (le altre vengono
 *        comunque applicate) e la funzione restituisce ERR_NOT_FOUND
 *        alla fine.
 * @param ctrl Controllore gia' creato.
 * @param scenario Scenario gia' letto con parser_caricaScenario.
 * @return OP_SUCCESS se applicato a tutte le ISP elencate (o se
 *         n_guasto_isp è 0, nessun guasto da configurare), ERR_NOT_FOUND
 *         se almeno una non e' stata trovata/non aveva un sensore
 *         agganciato, ERR_NULL_PTR se ctrl o scenario sono NULL.
 */
short int parser_applicaScenario( controllore_t *ctrl, const ScenarioConfig *scenario );

/**
 * @brief Valori di simulazione e target di generazione, letti da file
 *        invece che da #define fissi nel main: dimensioni dei buffer
 *        (indirettamente, tramite il file di impianto), numero di passi,
 *        numero di pezzi di prova generati, soglia dei buffer, e le
 *        misure "nominali" di ingresso attorno a cui viene applicato
 *        l'errore casuale (quella logica resta nel main, e' gia' scritta:
 *        qui forniamo solo i valori attorno a cui viene applicata).
 */
typedef struct {
    int    n_step_simulazione;
    int    n_pezzi_prova;
    int    n_pezzi_prova_b2;       /* pezzi pre-caricati in B2 all'avvio (default 0, vedi SIM_PEZZI_B2) */
    double soglia_buffer;
    double gen_target_dimensionX;  /* misura nominale di ingresso, es. 100 */
    double gen_target_raggio;      /* misura nominale di ingresso, es. 10 */
    int    gen_errore_pct;         /* ampiezza massima dell'errore casuale, in punti percentuali (es. 2 = +/-2%) */
} SimulationConfig;

/**
 * @brief Legge il file di configurazione impianto (lo stesso passato a
 *        parser_costruisciCella) ed estrae i parametri globali di
 *        simulazione (righe SIM_STEPS=, SIM_PEZZI=, SIM_PEZZI_B2=,
 *        SOGLIA_BUFFER=, GEN_TARGET_DIMENSIONX=, GEN_TARGET_RAGGIO=,
 *        GEN_ERRORE_PCT=).
 * @param path Percorso del file di configurazione impianto.
 * @param out Struct da riempire (viene azzerata e reinizializzata con
 *        dei default ragionevoli per ogni chiave assente dal file).
 * @param errCode puntatore opzionale (puo' essere NULL).
 * @return 1 se il file e' stato letto correttamente, 0 se non si apre.
 */
int parser_caricaSimulazione( const char *path, SimulationConfig *out, short int *errCode );

#endif /* PARSER_H */
