/**
 * @file cell.h
 * @brief Modulo "cella": punto d'ingresso unico per costruire la cella
 *        a partire dal file di configurazione.
 *
 * Pensato per essere usato come black-box da chi scrive il parser
 * (es. una riga BUFFER nel file di configurazione -> una chiamata a
 * cell_addBuffer; una riga che collega due elementi -> una chiamata
 * a cell_connect). Chi usa questo modulo non deve mai chiamare
 * direttamente buffer_create, nastro_create, registry_add o le altre
 * funzioni di basso livello: cell.c fa da collante e nasconde i dettagli.
 *
 * Stato attuale: supporta buffer_t e nastro_t (machine_t/isp_t non
 * esistono ancora). Quando saranno pronti, andranno aggiunte
 * cell_addMachine/cell_addISP sullo stesso modello di cell_addBuffer/
 * cell_addNastro, e cell_connect/cell_attachSensor/cell_attachActuator
 * andranno estese per gestire anche quei tipi (vedi i commenti nel .c).
 */

#ifndef CELL_H
#define CELL_H

#include <stdbool.h>
#include "object.h"
#include "errors.h"
#include "buffer.h"
#include "nastro.h"

/**
 * @brief La cella: possiede tutte le entità create al suo interno.
 *
 * I campi interni non sono pensati per essere letti direttamente da
 * fuori: usare le funzioni cell_* sottostanti.
 */
typedef struct cell cell_t;

/**
 * @brief Crea una cella vuota, senza nessuna entità al suo interno.
 * @return Puntatore alla cella allocata, o NULL se malloc fallisce.
 */
cell_t *cell_create( void );

/**
 * @brief Crea un buffer, lo registra nel registro globale e lo
 *        aggiunge alla cella (che ne diventa proprietaria).
 * @param cell Puntatore alla cella.
 * @param ID Identificativo del buffer (es. "B1"), non vuoto e < IDLENGTH.
 * @param capacity Capacità del buffer (deve essere > 0).
 * @param errCode puntatore opzionale (può essere NULL) in cui viene scritto
 *        OP_SUCCESS oppure un codice ERR_* (vedi errors.h) che spiega
 *        perché la creazione è fallita (incluso ERR_DUPLICATE se un
 *        buffer con lo stesso ID esiste già nella cella).
 * @return Puntatore al buffer creato, o NULL in caso di errore.
 */
buffer_t *cell_addBuffer( cell_t *cell, const char *ID, int capacity, short int *errCode );

/**
 * @brief Crea un nastro trasportatore, lo registra nel registro globale
 *        e lo aggiunge alla cella (che ne diventa proprietaria).
 * @param cell Puntatore alla cella.
 * @param ID Identificativo del nastro (es. "C1"), non vuoto e < IDLENGTH.
 * @param capacity Numero massimo di oggetti trasportabili insieme (deve essere > 0).
 * @param velocita Passi di simulazione per attraversare il nastro (deve essere > 0).
 * @param errCode puntatore opzionale (può essere NULL) in cui viene scritto
 *        OP_SUCCESS oppure un codice ERR_* (vedi errors.h) che spiega
 *        perché la creazione è fallita (incluso ERR_DUPLICATE se un
 *        nastro con lo stesso ID esiste già nella cella).
 * @return Puntatore al nastro creato, o NULL in caso di errore.
 */
nastro_t *cell_addNastro( cell_t *cell, const char *ID, int capacity, int velocita, short int *errCode );

/**
 * @brief Collega due entità già presenti nella cella: aggiunge toID
 *        agli output di fromID e fromID agli input di toID.
 *
 * Supporta naturalmente branch (più cell_connect con lo stesso fromID)
 * e merge (più cell_connect con lo stesso toID): ogni chiamata aggiunge
 * un collegamento senza rimuovere quelli già presenti.
 *
 * Stato attuale: fromID e toID possono essere, in qualsiasi
 * combinazione, un buffer o un nastro (ENTITY_BUFFER/ENTITY_NASTRO).
 * Quando machine_t/isp_t esisteranno, andranno aggiunti gli altri casi
 * (vedi cell.c).
 * @param cell Puntatore alla cella.
 * @param fromID ID dell'entità di partenza (deve già esistere nella cella).
 * @param toID ID dell'entità di arrivo (deve già esistere nella cella).
 * @return OP_SUCCESS se collegate, un codice ERR_* (vedi errors.h) altrimenti:
 *         ERR_NOT_FOUND se uno dei due ID non esiste, ERR_NOT_SUPPORTED se
 *         uno dei due non è (ancora) un tipo di entità gestito da cell_connect.
 */
short int cell_connect( cell_t *cell, const char *fromID, const char *toID );

/**
 * @brief Collega un sensore già registrato a un'entità della cella
 *        (buffer, e in futuro machine/isp/nastro).
 *
 * Il sensore deve essere già stato creato e registrato (con
 * registry_add e il tipo ENTITY_SENSOR_*) da chi possiede quel modulo:
 * questa funzione si limita a verificarne il tipo, risolvere il tipo
 * del bersaglio, e aggiungere il sensore alla sua sensorList.
 *
 * Stato attuale: supporta bersagli ENTITY_BUFFER ed ENTITY_NASTRO.
 * Quando machine_t/isp_t avranno le proprie sensorList/addSensor
 * (stesso schema di buffer.h), andranno aggiunti gli altri casi nel
 * corpo della funzione (vedi cell.c).
 * @param cell Puntatore alla cella.
 * @param targetID ID dell'entità a cui collegare il sensore (oggi un
 *        buffer o un nastro; in futuro anche machine/isp).
 * @param sensorID ID del sensore, già presente nel registro con un
 *        tipo ENTITY_SENSOR_*.
 * @return OP_SUCCESS se collegato, ERR_NOT_FOUND se targetID o sensorID
 *         non esistono nel registro, ERR_NOT_SUPPORTED se sensorID non è
 *         di un tipo ENTITY_SENSOR_*, o se targetID è di un tipo per cui
 *         l'aggancio non è (ancora) implementato, un altro codice ERR_*
 *         (vedi errors.h) per altri errori.
 */
short int cell_attachSensor( cell_t *cell, const char *targetID, const char *sensorID );

/**
 * @brief Collega un attuatore già registrato a un'entità della cella
 *        (buffer, e in futuro machine/isp/nastro).
 *
 * Stesso schema di cell_attachSensor, ma per un tipo ENTITY_ACTUATOR_*.
 * @param cell Puntatore alla cella.
 * @param targetID ID dell'entità a cui collegare l'attuatore (oggi un
 *        buffer o un nastro; in futuro anche machine/isp).
 * @param actuatorID ID dell'attuatore, già presente nel registro con un
 *        tipo ENTITY_ACTUATOR_*.
 * @return OP_SUCCESS se collegato, ERR_NOT_FOUND se targetID o actuatorID
 *         non esistono nel registro, ERR_NOT_SUPPORTED se actuatorID non è
 *         di un tipo ENTITY_ACTUATOR_*, o se targetID è di un tipo per cui
 *         l'aggancio non è (ancora) implementato, un altro codice ERR_*
 *         (vedi errors.h) per altri errori.
 */
short int cell_attachActuator( cell_t *cell, const char *targetID, const char *actuatorID );

/**
 * @brief Verifica che la cella rispetti tutti i vincoli di coerenza.
 *
 * Da chiamare dopo aver finito di costruire e collegare la cella (non
 * impone un ordine tra cell_connect/cell_attachActuator): controlla,
 * tra le altre cose, che ogni buffer con 2 o più uscite abbia almeno
 * un deviatore tra i suoi attuatori collegati.
 * @param cell Puntatore alla cella.
 * @return OP_SUCCESS se tutti i controlli passano, il primo codice
 *         ERR_* (vedi errors.h) del primo controllo fallito altrimenti.
 */
short int cell_validateAll( const cell_t *cell );

/**
 * @brief Recupera il buffer con l'ID indicato tra quelli della cella.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo cercato.
 * @return Puntatore al buffer_t, o NULL se non trovato o se cell è NULL.
 */
buffer_t *cell_getBuffer( const cell_t *cell, const char *ID );

/**
 * @brief Rimuove un buffer dalla cella: lo toglie dal registro globale
 *        e lo libera (buffer_delete).
 *
 * Non ricollega automaticamente gli input/output dei vicini (stessa
 * scelta di design già discussa per buffer_delete: nessuna
 * riconfigurazione dinamica della topologia a runtime).
 * @param cell Puntatore alla cella.
 * @param ID Identificativo del buffer da rimuovere.
 * @return OP_SUCCESS se rimosso, ERR_NOT_FOUND se nessun buffer con
 *         quell'ID appartiene alla cella, ERR_NULL_PTR se cell o ID
 *         sono NULL.
 */
short int cell_removeBuffer( cell_t *cell, const char *ID );

/**
 * @brief Numero di buffer attualmente posseduti dalla cella.
 * @param cell Puntatore alla cella.
 * @return Numero di buffer (sempre >= 0), oppure ERR_NULL_PTR se cell è NULL.
 */
int cell_getBufferCount( const cell_t *cell );

/**
 * @brief Controlla se la cella possiede un buffer con l'ID indicato.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo cercato.
 * @return true se presente, false altrimenti (anche se cell o ID sono NULL).
 */
bool cell_hasBuffer( const cell_t *cell, const char *ID );

/**
 * @brief Recupera il nastro con l'ID indicato tra quelli della cella.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo cercato.
 * @return Puntatore al nastro_t, o NULL se non trovato o se cell è NULL.
 */
nastro_t *cell_getNastro( const cell_t *cell, const char *ID );

/**
 * @brief Rimuove un nastro dalla cella: lo toglie dal registro globale
 *        e lo libera (nastro_delete).
 *
 * Stesso comportamento di cell_removeBuffer: non ricollega
 * automaticamente i vicini.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo del nastro da rimuovere.
 * @return OP_SUCCESS se rimosso, ERR_NOT_FOUND se nessun nastro con
 *         quell'ID appartiene alla cella, ERR_NULL_PTR se cell o ID
 *         sono NULL.
 */
short int cell_removeNastro( cell_t *cell, const char *ID );

/**
 * @brief Numero di nastri attualmente posseduti dalla cella.
 * @param cell Puntatore alla cella.
 * @return Numero di nastri (sempre >= 0), oppure ERR_NULL_PTR se cell è NULL.
 */
int cell_getNastroCount( const cell_t *cell );

/**
 * @brief Controlla se la cella possiede un nastro con l'ID indicato.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo cercato.
 * @return true se presente, false altrimenti (anche se cell o ID sono NULL).
 */
bool cell_hasNastro( const cell_t *cell, const char *ID );

/**
 * @brief Stampa lo stato di tutte le entità della cella.
 * @param cell Puntatore alla cella.
 */
void cell_print( const cell_t *cell );

/**
 * @brief Distrugge la cella: elimina dal registro e libera tutte le
 *        entità che possiede (buffer, nastri, e in futuro machine/isp),
 *        poi libera la cella stessa.
 * @param cell Puntatore alla cella.
 */
void cell_destroy( cell_t *cell );

#endif /* CELL_H */
