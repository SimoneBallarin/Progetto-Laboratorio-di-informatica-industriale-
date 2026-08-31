/**
 * @file S_Buffer.h
 * @brief Sensore di livello buffer: tiene traccia di quanti oggetti sono
 *        presenti in un buffer rispetto alla sua capacità massima, usato
 *        sia per le statistiche di occupazione sia dall'ammissione
 *        "buffer-aware" della Strategia 1 (sez. 4.1 del progetto
 *        preliminare).
 */
#ifndef S_BUFFER_H
#define S_BUFFER_H
#include "object.h"
#include <stdbool.h>

/**
 * @brief Stato del buffer secondo il sensore.
 *
 * Con solo due stati, BUFFER_EMPTY va interpretato come "non pieno"
 * (comprende sia il buffer davvero vuoto sia livelli intermedi), non
 * letteralmente "vuoto". Se serve distinguere anche il caso intermedio,
 * va aggiunto un terzo valore (es. BUFFER_PARZIALE) e gestito di
 * conseguenza in aggiornamento_status().
 */
typedef enum {
    BUFFER_EMPTY = 0,
    BUFFER_FULL  = 1
} StatoBuffer;

/** @brief Stato di un sensore di livello buffer. */
typedef struct {
    char ID[IDLENGTH];
    long livello_attuale;
    long livello_massimo;
    StatoBuffer status;
} SensoreBuffer;

/**
 * @brief Inizializza un sensore di livello buffer, a livello 0.
 * @param s Puntatore al sensore.
 * @param ID Identificativo del sensore (max 19 caratteri + terminatore),
 *        copiato internamente.
 * @param livello_massimo Capacità massima del buffer monitorato (> 0).
 * @return OP_SUCCESS se inizializzato, un codice ERR_* (vedi errors.h) altrimenti.
 */
int  sensore_Buffer_init(SensoreBuffer *s, const char *ID, long livello_massimo);

/**
 * @brief Aggiorna il livello e lo stato del sensore in base a una
 *        variazione del buffer monitorato.
 * @param s Puntatore al sensore.
 * @param new_object 1 se e' appena entrato un oggetto, -1 se ne e' appena
 *        uscito uno, 0 per un aggiornamento senza variazione di livello.
 * @return OP_SUCCESS se aggiornato, ERR_NULL_PTR se s e' NULL.
 */
int  aggiornamento_status(SensoreBuffer *s, int new_object);

/**
 * @brief Stato attuale del buffer secondo il sensore.
 * @param s Puntatore al sensore.
 * @return BUFFER_EMPTY o BUFFER_FULL, oppure ERR_NULL_PTR se s e' NULL.
 */
int  get_status_buffer(const SensoreBuffer *s);

/**
 * @brief Livello attuale (numero di oggetti) rilevato dal sensore.
 * @param s Puntatore al sensore.
 * @return Livello attuale, oppure ERR_NULL_PTR se s e' NULL.
 */
long get_livello_attuale(const SensoreBuffer *s);

/**
 * @brief Percentuale di riempimento del buffer (livello_attuale / livello_massimo).
 * @param s Puntatore al sensore.
 * @return Percentuale (0-100), oppure ERR_NULL_PTR se s e' NULL.
 */
int  get_percentuale_livello(const SensoreBuffer *s);

#endif
