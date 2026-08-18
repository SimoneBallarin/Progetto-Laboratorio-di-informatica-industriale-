/**
 * @file Controllore.h
 * @brief Libreria di controllo: orchestra il flusso degli oggetti tra
 *        le entità della cella ad ogni passo di simulazione, includendo
 *        ora il comando di Motore/Deviatore e la lettura/sincronizzazione
 *        dei sensori S_Buffer/S_Presenza.
 *
 * COSA FA OGNI PASSO (controllore_step), in quest'ordine:
 *   1. Riprova a instradare gli oggetti rimasti bloccati al passo
 *      precedente (destinazione piena, o Deviatore non ancora in
 *      posizione), invece di perderli silenziosamente.
 *   2. ISP pronte: calcola l'esito (CONFORME/RIVALUTAZIONE/SCARTO). Se
 *      alla ISP è collegato un Deviatore (controllore_collegaDeviatore),
 *      lo comanda verso la posizione corrispondente all'esito e aspetta
 *      che sia fisicamente arrivato prima di instradare l'oggetto;
 *      altrimenti instrada subito in base al solo indice di uscita
 *      (vedi convenzione in cell.h).
 *   3. Macchine pronte: rilascia l'oggetto lavorato verso il suo output.
 *   4. Nastri pronti: se collegato un Motore (controllore_collegaMotore),
 *      lo accende/spegne in base al contenuto del nastro e lo aggiorna
 *      (rampa, temperatura); poi rilascia l'oggetto trasportato.
 *   5. Buffer non vuoti: preleva l'oggetto a priorità più alta e prova
 *      ad ammetterlo nella destinazione collegata, con ammissione
 *      "buffer-aware" (Strategia 1, sez. 4.1) basata sulla lettura del
 *      SensoreBuffer associato al buffer a valle di una macchina.
 *
 * SENSORI: alla creazione, il controllore istanzia automaticamente un
 * SensoreBuffer per ogni buffer già presente nella cella, e lo aggiorna
 * lui stesso (aggiornamento_status) ad ogni inserimento/rimozione che fa
 * passare attraverso genericInsert/processBuffer — quindi resta sempre
 * sincronizzato col contenuto reale del buffer, senza bisogno che
 * nessun altro modulo se ne ricordi. Vedi controllore_getPercentualeBuffer
 * / controllore_getStatoBuffer per leggerlo dall'esterno (es. per log).
 *
 * S_Presenza non è (ancora) usato internamente dal controllore: la sua
 * unica applicazione prevista dal progetto è rilevare l'arrivo di un
 * NUOVO oggetto al buffer di ingresso (sez. 5.1), ma qui non esiste
 * ancora nulla che generi arrivi dall'esterno (sarà compito del
 * parser/main). Viene comunque esposto con controllore_segnalaArrivo,
 * pronto per essere chiamato dal main una volta scritto.
 */

#ifndef CONTROLLORE_H
#define CONTROLLORE_H

#include "object.h"
#include "errors.h"
#include "cell.h"
#include "S_Buffer.h"
#include "S_Presenza.h"

typedef struct controllore controllore_t;

/**
 * @brief Crea un controllore agganciato a una cella già costruita.
 *
 * Il controllore NON diventa proprietario della cella: cell_destroy va
 * chiamata a parte da chi ha creato la cella. Crea automaticamente un
 * SensoreBuffer per ogni buffer già presente nella cella in questo
 * momento (i buffer aggiunti alla cella DOPO questa chiamata non hanno
 * un sensore associato: da tenere presente se si costruisce la cella in
 * più fasi).
 * @param cell Puntatore alla cella da orchestrare (deve esistere già).
 * @param soglia_buffer Soglia di occupazione (0.0-1.0, es. 0.8) oltre la
 *        quale l'ammissione in una macchina viene ritardata se il
 *        buffer a valle della macchina è troppo pieno (Strategia 1).
 * @param errCode puntatore opzionale (può essere NULL) in cui viene
 *        scritto OP_SUCCESS oppure un codice ERR_* (vedi errors.h).
 * @return Puntatore al controllore allocato, o NULL in caso di errore.
 */
controllore_t *controllore_create( cell_t *cell, double soglia_buffer, short int *errCode );

/**
 * @brief Distrugge il controllore: libera le associazioni interne
 *        (sensori, Motore, Deviatore, coda pending), non la cella né
 *        gli oggetti eventualmente ancora in transito.
 * @param c Puntatore al controllore.
 */
void controllore_destroy( controllore_t *c );

/**
 * @brief Collega un Motore a un nastro già presente nella cella.
 *
 * Da qui in poi controllore_step accenderà/spegnerà questo motore in
 * base al contenuto del nastro e lo aggiornerà ogni passo (rampa,
 * temperatura). Un nastro senza motore collegato continua a funzionare
 * come prima (nessun obbligo di collegarne uno).
 * @param c Puntatore al controllore.
 * @param nastroID ID del nastro (deve già esistere nella cella).
 * @param velocita_target Velocità target del motore (vedi Motore.h).
 * @return OP_SUCCESS se collegato, un codice ERR_* (vedi errors.h) altrimenti,
 *         incluso ERR_NOT_FOUND se nastroID non esiste nella cella.
 */
short int controllore_collegaMotore( controllore_t *c, const char *nastroID, int velocita_target, int accelerazione_target );

/**
 * @brief Collega un Deviatore a una ISP già presente nella cella.
 *
 * Da qui in poi, quando questa ISP rilascia un oggetto, il controllore
 * comanda il Deviatore verso la posizione corrispondente all'esito e
 * aspetta che sia fisicamente in posizione (rispettando il tempo minimo
 * tra commutazioni) prima di instradare l'oggetto. Una ISP senza
 * Deviatore collegato instrada subito, come nella versione precedente.
 * @param c Puntatore al controllore.
 * @param ispID ID della ISP (deve già esistere nella cella).
 * @param tempo_minimo_commutazioni Tempo minimo (passi di simulazione)
 *        tra due commutazioni consecutive del Deviatore (sez. 5.2).
 * @return OP_SUCCESS se collegato, un codice ERR_* (vedi errors.h) altrimenti,
 *         incluso ERR_NOT_FOUND se ispID non esiste nella cella.
 */
short int controllore_collegaDeviatore( controllore_t *c, const char *ispID, int tempo_minimo_commutazioni );

/**
 * @brief Esegue un passo di controllo: scandisce tutte le entità della
 *        cella e fa avanzare gli oggetti pronti verso la destinazione
 *        successiva (vedi ordine dettagliato in testa al file).
 * @param c Puntatore al controllore.
 * @param step_corrente Step di simulazione corrente (tempo globale).
 * @return OP_SUCCESS se il passo è stato eseguito, ERR_NULL_PTR se c è NULL.
 */
short int controllore_step( controllore_t *c, int step_corrente );

/**
 * @brief Percentuale di occupazione (0-100) del buffer, letta dal
 *        SensoreBuffer associato (creato automaticamente da
 *        controllore_create).
 * @param c Puntatore al controllore.
 * @param bufferID ID del buffer.
 * @return Percentuale di occupazione, oppure ERR_NULL_PTR/ERR_NOT_FOUND
 *         (vedi errors.h) se non trovato.
 */
int controllore_getPercentualeBuffer( const controllore_t *c, const char *bufferID );

/**
 * @brief Stato sintetico (BUFFER_EMPTY/BUFFER_FULL, vedi S_Buffer.h) del
 *        buffer, letto dal SensoreBuffer associato.
 * @param c Puntatore al controllore.
 * @param bufferID ID del buffer.
 * @return Lo stato, oppure ERR_NULL_PTR/ERR_NOT_FOUND se non trovato.
 */
int controllore_getStatoBuffer( const controllore_t *c, const char *bufferID );

/**
 * @brief Inoltra una lettura al sensore di presenza associato a
 *        bufferIngressoID (creato al primo utilizzo per quell'ID).
 *
 * Pensata per essere chiamata dal main/parser una volta scritta la
 * generazione degli arrivi: rileva se time_on/presenza corrispondono a
 * un nuovo oggetto arrivato (fronte di salita), coerentemente con
 * S_Presenza.h.
 * @param c Puntatore al controllore.
 * @param bufferIngressoID ID del buffer di ingresso a cui è associato
 *        il sensore (non deve necessariamente esistere già nella cella:
 *        il sensore è indipendente dal buffer vero e proprio).
 * @param time_on Vedi get_status_presenza in S_Presenza.h.
 * @param presenza Vedi get_status_presenza in S_Presenza.h.
 * @return Uno tra ENTRATA/USCITA/PRESENZA_PROLUNGATA/ASSENZA_PROLUNGATA
 *         (vedi S_Presenza.h), oppure un codice ERR_* in caso di errore.
 */
int controllore_segnalaArrivo( controllore_t *c, const char *bufferIngressoID, int time_on, int presenza );

/**
 * @brief Numero di oggetti usciti dalla linea (arrivati a un'entità
 *        senza nessuna uscita collegata) da quando il controllore esiste.
 * @param c Puntatore al controllore.
 * @return Numero di oggetti completati, oppure ERR_NULL_PTR se c è NULL.
 */
long controllore_getCompletati( const controllore_t *c );

/**
 * @brief Numero di oggetti attualmente bloccati in coda "pending"
 *        (pronti per essere instradati ma con la destinazione piena, o
 *        in attesa che un Deviatore raggiunga la posizione richiesta).
 * @param c Puntatore al controllore.
 * @return Numero di oggetti in coda, oppure ERR_NULL_PTR se c è NULL.
 */
int controllore_getPendingCount( const controllore_t *c );

/**
 * @brief Stampa lo stato della cella orchestrata e le statistiche del
 *        controllore (completati, pending).
 * @param c Puntatore al controllore.
 */
void controllore_print( const controllore_t *c );

#endif /* CONTROLLORE_H */
