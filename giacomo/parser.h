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
 * @brief Legge il file oggetti e li inserisce nel buffer di ingresso
 *        indicato, segnalando ogni arrivo al controllore.
 *
 * Formato CSV atteso, con header opzionale:
 *   ID,PRIORITY,TYPE,ARRIVAL_STEP,DIMENSIONX,RAGGIO
 *   P001,2,A,0,101.0,10.5
 *
 * Per ogni riga valida: object_create -> buffer_insertObject (nel
 * buffer bufferIngressoID, con ordinamento per priorita') ->
 * object_setLocation -> controllore_segnalaArrivo.
 *
 * @param cell Cella gia' costruita (deve gia' contenere bufferIngressoID).
 * @param ctrl Controllore gia' creato.
 * @param path Percorso del file oggetti.
 * @param bufferIngressoID ID del buffer di ingresso (es. "B1") in cui
 *        inserire tutti gli oggetti letti.
 * @param errCode puntatore opzionale (puo' essere NULL), vedi
 *        parser_costruisciCella.
 * @return Numero di oggetti caricati con successo.
 */
int parser_caricaOggetti( cell_t *cell, controllore_t *ctrl, const char *path,
                           const char *bufferIngressoID, short int *errCode );

/**
 * @brief Configurazione di scenario: cosa varia tra "scenario nominale"
 *        e "scenario difficile" senza dover ricompilare (sez. 7 della
 *        traccia).
 */
typedef struct {
    char   nome[64];
    double moltiplicatore_carico;   /* riservato per usi futuri (es. generazione arrivi) */
    bool   guasto_abilitato;
    char   guasto_isp_id[IDLENGTH]; /* su quale ISP applicare il guasto, es. "ISP2" */
    int    guasto_tempo_errore;     /* passi di funzionamento OK prima del guasto */
    int    guasto_tempo_ok;         /* passi di guasto prima di tornare OK */
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
 * @brief Applica una ScenarioConfig gia' letta alla cella: se
 *        guasto_abilitato e' vero, chiama isp_impostaGuasto sull'ISP
 *        indicata da guasto_isp_id; se e' falso, disattiva
 *        esplicitamente il guasto su quella ISP (utile per passare da
 *        uno scenario difficile a uno nominale senza ricreare la cella).
 * @param cell Cella gia' costruita (deve contenere guasto_isp_id).
 * @param scenario Scenario gia' letto con parser_caricaScenario.
 * @return OP_SUCCESS se applicato, un codice ERR_* (vedi errors.h)
 *         altrimenti (es. ERR_NOT_FOUND se guasto_isp_id non esiste).
 */
short int parser_applicaScenario( cell_t *cell, const ScenarioConfig *scenario );

#endif /* PARSER_H */
