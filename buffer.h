/**
 * @file buffer.h
 * @brief Definizione dei buffer.
 *
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <stdbool.h>
#include "object.h"
#include "errors.h"

/* ------------------------------------------------------------------ */
/*  STRUCT                                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Descrizione della struttura buffer.
 *
 *
 */
typedef struct {
    char ID[IDLENGTH];     /**< ID del buffer. */
    int capacity;     /**< Capacità del buffer. */
    int counter;      /**< Quanti oggetti sono contenuti nel buffer. */
    struct bufferObj *head;           /**< Puntatore al primo oggetto nel buffer. */
    struct bufferObj *tail;           /**< Puntatore all'ultimo oggetto nel buffer. */
    char ID_Next[IDLENGTH];     /**< ID della destinazione buffer, ID_NONE ("NULL") se ultimo. */
    char ID_Previous[IDLENGTH];     /**< ID dell'ingresso buffer, ID_NONE ("NULL") se primo. */
} buffer_t;

/**
 * @brief Descrizione di un oggetto nel buffer.
 *
 * Utilizzato per creare una coda ordinata nel buffer.
 */
typedef struct bufferObj {
    object_t *dato;             /**< Payload del nodo. */
    struct bufferObj *next;          /**< Puntatore al nodo successivo, NULL se ultimo. */
} bufferObj_t;

/* ------------------------------------------------------------------ */
/*  FUNZIONI                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Crea un buffer vuoto con la capacità indicata.
 * @param ID Identificativo del buffer (es. "B1"), copiato internamente.
 * @param capacity Capacità del buffer (deve essere > 0).
 * @param ID_Next ID della destinazione buffer, passare ID_NONE se ultimo.
 * @param ID_Previous ID dell'ingresso buffer, passare ID_NONE se primo.
 * @return Puntatore al buffer allocato, o NULL se malloc fallisce.
 */
buffer_t *buffer_create( const char *ID, const int capacity, const char *ID_Next, const char *ID_Previous );

/**
 * @brief Controlla se il buffer è pieno.
 * @param buffer Puntatore al buffer.
 * @return true se pieno, false se non pieno.
 */
bool buffer_isFull( const buffer_t *buffer );

/**
 * @brief Controlla se il buffer è vuoto.
 * @param buffer Puntatore al buffer.
 * @return true se vuoto, false se non vuoto.
 */
bool buffer_isEmpty( const buffer_t *buffer );

/**
 * @brief Restituisce il numero di oggetti contenuti nel buffer.
 * @param buffer Puntatore al buffer.
 * @return Numero di oggetti contenuti nel buffer.
 */
int buffer_getCount( const buffer_t *buffer );

/**
 * @brief ID del buffer.
 * @param buffer Puntatore al buffer.
 * @return Puntatore costante alla stringa ID.
 */
const char *buffer_getID( const buffer_t *buffer );

/**
 * @brief Capacità massima del buffer.
 * @param buffer Puntatore al buffer.
 * @return Capacità del buffer.
 */
int buffer_getCapacity( const buffer_t *buffer );

/**
 * @brief ID del buffer successivo nel flusso.
 * @param buffer Puntatore al buffer.
 * @return Puntatore costante alla stringa ID_Next (ID_NONE se non presente).
 */
const char *buffer_getNext( const buffer_t *buffer );

/**
 * @brief ID del buffer precedente nel flusso.
 * @param buffer Puntatore al buffer.
 * @return Puntatore costante alla stringa ID_Previous (ID_NONE se non presente).
 */
const char *buffer_getPrevious( const buffer_t *buffer );

/**
 * @brief Inserisce un oggetto nel buffer.
 *
 * La funzione non modifica l'oggetto puntato da object; ne salva solo il
 * puntatore all'interno del nodo di coda (nessuna copia).
 * @param buffer Puntatore al buffer.
 * @param object Puntatore all'oggetto da inserire.
 * @param priority Se true, inserimento ordinato per priorità; se false, in coda.
 * @return OP_SUCCESS se l'inserimento va a buon fine, un codice ERR_* (vedi errors.h) in caso di errore.
 */
int buffer_insertObject( buffer_t *buffer, object_t *object, const bool priority );

/**
 * @brief Rimuove un oggetto dal buffer.
 * @param buffer Puntatore al buffer.
 * @param priority Se true, rimozione dell'oggetto con priorità più alta; se false, rimozione del primo oggetto in coda.
 * @return Puntatore all'oggetto rimosso, o NULL se il buffer è vuoto.
 */
object_t *buffer_removeObject( buffer_t *buffer, const bool priority );

/**
 * @brief Elimina un buffer.
 * @param buffer Puntatore al buffer.
 */
void buffer_delete ( buffer_t *buffer );

/**
 * @brief Stampa le informazioni del buffer.
 * @param buffer Puntatore al buffer.
 */
void buffer_print( const buffer_t *buffer );

#endif /* BUFFER_H */
