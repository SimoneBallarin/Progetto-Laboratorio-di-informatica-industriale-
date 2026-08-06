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
#include "idlist.h"

/* ------------------------------------------------------------------ */
/*  STRUCT                                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Descrizione della struttura buffer.
 *
 * Gli ID dei macchinari collegati in ingresso/uscita sono rappresentati
 * come liste (idlist.h) invece che come singoli campi: questo permette
 * di modellare branch (più uscite possibili, es. smistamento) e merge
 * (più macchinari collegati allo stesso buffer) senza casi speciali.
 * Per un buffer "in linea" (un solo ingresso, una sola uscita), le
 * liste conterranno semplicemente un elemento.
 */
typedef struct {
    char ID[IDLENGTH];     /**< ID del buffer. */
    int capacity;     /**< Capacità del buffer. */
    int counter;      /**< Quanti oggetti sono contenuti nel buffer. */
    struct bufferObj *head;           /**< Puntatore al primo oggetto nel buffer. */
    struct bufferObj *tail;           /**< Puntatore all'ultimo oggetto nel buffer. */
    idNode_t *inputList;     /**< ID dei macchinari collegati in ingresso (vuota se nessuno). */
    idNode_t *outputList;    /**< ID dei macchinari collegati in uscita (vuota se nessuno). */
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
 * @brief Crea un buffer vuoto con la capacità indicata, senza collegamenti.
 *
 * I collegamenti in ingresso/uscita si aggiungono dopo con
 * buffer_addInput / buffer_addOutput.
 * @param ID Identificativo del buffer (es. "B1"), copiato internamente, non vuoto.
 * @param capacity Capacità del buffer (deve essere > 0).
 * @param errCode puntatore opzionale (può essere NULL) in cui viene scritto OP_SUCCESS
 *        oppure un codice ERR_* (vedi errors.h) che spiega perché la creazione è fallita
 *        (ERR_NULL_PTR, ERR_ID_INVALID, ERR_OUT_OF_RANGE, ERR_ALLOC).
 * @return Puntatore al buffer allocato, o NULL in caso di errore (vedi *errCode per il motivo).
 */
buffer_t *buffer_create( const char *ID, const int capacity, short int *errCode );

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
 *
 * Il numero di oggetti è sempre >= 0, quindi ERR_NULL_PTR (-1) è un
 * valore di ritorno inequivocabile per il caso di errore.
 * @param buffer Puntatore al buffer.
 * @return Numero di oggetti contenuti nel buffer, oppure ERR_NULL_PTR se buffer è NULL.
 */
int buffer_getCount( const buffer_t *buffer );

/**
 * @brief ID del buffer.
 *
 * L'ID di un buffer viene fissato alla creazione e non cambia mai,
 * quindi l'unico caso in cui questa funzione restituisce NULL è
 * quando il parametro buffer stesso è NULL.
 * @param buffer Puntatore al buffer.
 * @return Puntatore costante alla stringa ID, oppure NULL se buffer è NULL.
 */
const char *buffer_getID( const buffer_t *buffer );

/**
 * @brief Capacità massima del buffer.
 *
 * La capacità di un buffer valido è sempre > 0, quindi ERR_NULL_PTR (-1)
 * è un valore di ritorno inequivocabile per il caso di errore.
 * @param buffer Puntatore al buffer.
 * @return Capacità del buffer, oppure ERR_NULL_PTR se buffer è NULL.
 */
int buffer_getCapacity( const buffer_t *buffer );

/**
 * @brief Aggiunge un macchinario collegato in ingresso al buffer.
 * @param buffer Puntatore al buffer.
 * @param ID ID del macchinario da aggiungere come ingresso.
 * @return OP_SUCCESS se aggiunto, ERR_DUPLICATE se l'ID è già presente,
 *         un altro codice ERR_* (vedi errors.h) per altri errori.
 */
short int buffer_addInput( buffer_t *buffer, const char *ID );

/**
 * @brief Aggiunge un macchinario collegato in uscita al buffer.
 * @param buffer Puntatore al buffer.
 * @param ID ID del macchinario da aggiungere come uscita.
 * @return OP_SUCCESS se aggiunto, ERR_DUPLICATE se l'ID è già presente,
 *         un altro codice ERR_* (vedi errors.h) per altri errori.
 */
short int buffer_addOutput( buffer_t *buffer, const char *ID );

/**
 * @brief Numero di macchinari collegati in ingresso al buffer.
 * @param buffer Puntatore al buffer.
 * @return Numero di ingressi (sempre >= 0), oppure ERR_NULL_PTR se buffer è NULL.
 */
int buffer_getInputCount( const buffer_t *buffer );

/**
 * @brief Legge l'ID dell'ingresso all'indice indicato (0-based), copiandolo in outID.
 *
 * Da usare come un array: per i da 0 a buffer_getInputCount(buffer)-1,
 * buffer_getInputAt(buffer, i, id) restituisce l'i-esimo ingresso.
 * @param buffer Puntatore al buffer.
 * @param index Indice dell'ingresso richiesto (0 = primo).
 * @param outID Buffer di dimensione almeno IDLENGTH in cui viene copiato l'ID.
 * @return OP_SUCCESS se trovato, un codice ERR_* (vedi errors.h) altrimenti.
 */
short int buffer_getInputAt( const buffer_t *buffer, int index, char outID[IDLENGTH] );

/**
 * @brief Controlla se un dato ID è tra gli ingressi collegati al buffer.
 * @param buffer Puntatore al buffer.
 * @param ID ID cercato.
 * @return true se presente, false altrimenti (anche se buffer è NULL).
 */
bool buffer_hasInput( const buffer_t *buffer, const char *ID );

/**
 * @brief Numero di macchinari collegati in uscita al buffer.
 * @param buffer Puntatore al buffer.
 * @return Numero di uscite (sempre >= 0), oppure ERR_NULL_PTR se buffer è NULL.
 */
int buffer_getOutputCount( const buffer_t *buffer );

/**
 * @brief Legge l'ID dell'uscita all'indice indicato (0-based), copiandolo in outID.
 *
 * Da usare come un array: per i da 0 a buffer_getOutputCount(buffer)-1,
 * buffer_getOutputAt(buffer, i, id) restituisce l'i-esima uscita.
 * @param buffer Puntatore al buffer.
 * @param index Indice dell'uscita richiesta (0 = prima).
 * @param outID Buffer di dimensione almeno IDLENGTH in cui viene copiato l'ID.
 * @return OP_SUCCESS se trovato, un codice ERR_* (vedi errors.h) altrimenti.
 */
short int buffer_getOutputAt( const buffer_t *buffer, int index, char outID[IDLENGTH] );

/**
 * @brief Controlla se un dato ID è tra le uscite collegate al buffer.
 * @param buffer Puntatore al buffer.
 * @param ID ID cercato.
 * @return true se presente, false altrimenti (anche se buffer è NULL).
 */
bool buffer_hasOutput( const buffer_t *buffer, const char *ID );

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
 * @return Puntatore all'oggetto rimosso, o NULL se il buffer è NULL o vuoto.
 */
object_t *buffer_removeObject( buffer_t *buffer, const bool priority );

/**
 * @brief Elimina un buffer, inclusi i nodi delle liste input/output.
 * @param buffer Puntatore al buffer.
 */
void buffer_delete( buffer_t *buffer );

/**
 * @brief Stampa le informazioni del buffer.
 * @param buffer Puntatore al buffer.
 */
void buffer_print( const buffer_t *buffer );

#endif /* BUFFER_H */
