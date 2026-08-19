/**
 * @file cell.h
 * @brief Modulo "cella": punto d'ingresso unico per costruire la cella
 *        a partire dal file di configurazione.
 *
 * Pensato per essere usato come black-box da chi scrive il parser
 * (es. una riga BUFFER nel file di configurazione -> una chiamata a
 * cell_addBuffer; una riga che collega due elementi -> una chiamata
 * a cell_connect). Chi usa questo modulo non deve mai chiamare
 * direttamente buffer_create, machine_create, isp_create, nastro_create,
 * registry_add o le altre funzioni di basso livello: cell.c fa da
 * collante e nasconde i dettagli.
 *
 * Supporta buffer_t, machine_t, isp_t e nastro_t, tutti sullo stesso
 * schema: una lista di proprietà dedicata per tipo (bufferListNode_t,
 * machineListNode_t, ispListNode_t, nastroListNode_t), e funzioni
 * cell_addX/cell_getX/cell_removeX/cell_getXCount/cell_hasX per
 * ciascuno. cell_connect/cell_attachSensor/cell_attachActuator sono
 * generiche: risolvono il tipo tramite il registro e fanno dispatch
 * verso la funzione giusta, qualunque combinazione di tipi.
 */

#ifndef CELL_H
#define CELL_H

#include <stdbool.h>
#include "object.h"
#include "errors.h"
#include "buffer.h"
#include "machine.h"
#include "isp.h"
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
 * @brief Crea una macchina (stazione M), la registra nel registro
 *        globale e la aggiunge alla cella (che ne diventa proprietaria).
 * @param cell Puntatore alla cella.
 * @param ID Identificativo (es. "M1"), non vuoto e < IDLENGTH.
 * @param tempo_lavorazione Passi di simulazione per lavorare un pezzo (deve essere > 0).
 * @param errCode puntatore opzionale (può essere NULL), vedi cell_addBuffer.
 * @return Puntatore alla macchina creata, o NULL in caso di errore.
 */
machine_t *cell_addMachine( cell_t *cell, const char *ID, int tempo_lavorazione, short int *errCode );

/**
 * @brief Crea una ISP, la registra nel registro globale e la aggiunge
 *        alla cella (che ne diventa proprietaria).
 * @param cell Puntatore alla cella.
 * @param ID Identificativo (es. "ISP1"), non vuoto e < IDLENGTH.
 * @param tempo_controllo Passi di simulazione per controllare un pezzo (deve essere > 0).
 * @param dimensionX_target Valore di riferimento per la dimensione, vedi S_Qualita.h.
 * @param raggio_target Valore di riferimento per il raggio, vedi S_Qualita.h.
 * @param errCode puntatore opzionale (può essere NULL), vedi cell_addBuffer.
 * @return Puntatore alla ISP creata, o NULL in caso di errore.
 */
isp_t *cell_addISP( cell_t *cell, const char *ID, int tempo_controllo, int dimensionX_target, int raggio_target, short int *errCode );

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
 * combinazione, un buffer, una macchina, una ISP o un nastro.
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
 * Stato attuale: supporta bersagli ENTITY_BUFFER, ENTITY_MACHINE,
 * ENTITY_ISP ed ENTITY_NASTRO.
 * @param cell Puntatore alla cella.
 * @param targetID ID dell'entità a cui collegare il sensore (buffer,
 *        macchina, ISP o nastro).
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
 * @param targetID ID dell'entità a cui collegare l'attuatore (buffer,
 *        macchina, ISP o nastro).
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
 * @brief Recupera la macchina con l'ID indicato tra quelle della cella.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo cercato.
 * @return Puntatore al machine_t, o NULL se non trovata o se cell è NULL.
 */
machine_t *cell_getMachine( const cell_t *cell, const char *ID );

/**
 * @brief Rimuove una macchina dalla cella: la toglie dal registro
 *        globale e la libera (machine_delete).
 *
 * Stesso comportamento di cell_removeBuffer: non ricollega
 * automaticamente i vicini.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo della macchina da rimuovere.
 * @return OP_SUCCESS se rimossa, ERR_NOT_FOUND se nessuna macchina con
 *         quell'ID appartiene alla cella, ERR_NULL_PTR se cell o ID
 *         sono NULL.
 */
short int cell_removeMachine( cell_t *cell, const char *ID );

/**
 * @brief Numero di macchine attualmente possedute dalla cella.
 * @param cell Puntatore alla cella.
 * @return Numero di macchine (sempre >= 0), oppure ERR_NULL_PTR se cell è NULL.
 */
int cell_getMachineCount( const cell_t *cell );

/**
 * @brief Controlla se la cella possiede una macchina con l'ID indicato.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo cercato.
 * @return true se presente, false altrimenti (anche se cell o ID sono NULL).
 */
bool cell_hasMachine( const cell_t *cell, const char *ID );

/**
 * @brief Recupera la ISP con l'ID indicato tra quelle della cella.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo cercato.
 * @return Puntatore all'isp_t, o NULL se non trovata o se cell è NULL.
 */
isp_t *cell_getISP( const cell_t *cell, const char *ID );

/**
 * @brief Rimuove una ISP dalla cella: la toglie dal registro globale
 *        e la libera (isp_delete).
 *
 * Stesso comportamento di cell_removeBuffer: non ricollega
 * automaticamente i vicini.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo della ISP da rimuovere.
 * @return OP_SUCCESS se rimossa, ERR_NOT_FOUND se nessuna ISP con
 *         quell'ID appartiene alla cella, ERR_NULL_PTR se cell o ID
 *         sono NULL.
 */
short int cell_removeISP( cell_t *cell, const char *ID );

/**
 * @brief Numero di ISP attualmente possedute dalla cella.
 * @param cell Puntatore alla cella.
 * @return Numero di ISP (sempre >= 0), oppure ERR_NULL_PTR se cell è NULL.
 */
int cell_getISPCount( const cell_t *cell );

/**
 * @brief Controlla se la cella possiede una ISP con l'ID indicato.
 * @param cell Puntatore alla cella.
 * @param ID Identificativo cercato.
 * @return true se presente, false altrimenti (anche se cell o ID sono NULL).
 */
bool cell_hasISP( const cell_t *cell, const char *ID );

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
 * @brief Legge l'ID del buffer all'indice indicato (0-based) tra quelli
 *        posseduti dalla cella, copiandolo in outID.
 *
 * Permette di enumerare tutte le entità di un tipo senza conoscerne gli
 * ID in anticipo — usato dal controllore per scandire l'intera cella a
 * ogni passo di simulazione. Le equivalenti per machine/isp/nastro
 * seguono lo stesso schema.
 * @param cell Puntatore alla cella.
 * @param index Indice richiesto (0 = primo).
 * @param outID Buffer di dimensione almeno IDLENGTH in cui viene copiato l'ID.
 * @return OP_SUCCESS se trovato, un codice ERR_* (vedi errors.h) altrimenti.
 */
short int cell_getBufferIDAt( const cell_t *cell, int index, char outID[IDLENGTH] );
short int cell_getMachineIDAt( const cell_t *cell, int index, char outID[IDLENGTH] );
short int cell_getISPIDAt( const cell_t *cell, int index, char outID[IDLENGTH] );
short int cell_getNastroIDAt( const cell_t *cell, int index, char outID[IDLENGTH] );

/**
 * @brief Stampa lo stato di tutte le entità della cella.
 * @param cell Puntatore alla cella.
 */
void cell_print( const cell_t *cell );

/**
 * @brief Distrugge la cella: elimina dal registro e libera tutte le
 *        entità che possiede (buffer, macchine, ISP, nastri), poi
 *        libera la cella stessa.
 * @param cell Puntatore alla cella.
 */
void cell_destroy( cell_t *cell );

#endif /* CELL_H */
