/**
 * @file nastro.h
 * @brief Prototipo del nastro trasportatore.
 *
 * PROTOTIPO — da discutere e rifinire insieme, non ancora agganciato
 * a registry.h/cell.h.
 *
 * Modello scelto (coerente con l'assunzione della traccia, sez. 2.2:
 * "il tempo di percorrenza dei nastri è modellato in modo aggregato
 * tramite velocità e capacità, senza simulare la posizione continua
 * di ogni oggetto sul nastro"): ogni oggetto che entra nel nastro
 * viene marcato con lo step di simulazione in cui è entrato; è
 * "pronto per uscire" solo quando sono trascorsi almeno `velocita`
 * passi da quel momento. Il nastro è una coda FIFO (a differenza del
 * buffer non ha inserimento per priorità: è puro trasporto, non
 * gestisce quale oggetto è più urgente).
 *
 * Struttura del modulo identica a buffer.h: inputList/outputList per
 * i collegamenti nella topologia della cella, sensorList/actuatorList
 * per sensori/attuatori agganciati (es. un Motore che lo aziona).
 */

#ifndef NASTRO_H
#define NASTRO_H

#include <stdbool.h>
#include "object.h"
#include "errors.h"
#include "idlist.h"

/* ------------------------------------------------------------------ */
/*  STRUCT                                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Descrizione del nastro trasportatore.
 */
typedef struct {
    char ID[IDLENGTH];        /**< ID del nastro. */
    int capacity;              /**< Numero massimo di oggetti trasportabili contemporaneamente. */
    int counter;                /**< Quanti oggetti sono attualmente sul nastro. */
    int velocita;               /**< Passi di simulazione necessari per attraversare il nastro. */
    struct nastroObj *head;    /**< Oggetto più vicino all'uscita (il primo entrato). */
    struct nastroObj *tail;    /**< Ultimo oggetto entrato (il più vicino all'ingresso). */
    idNode_t *inputList;       /**< ID dei macchinari collegati in ingresso. */
    idNode_t *outputList;      /**< ID dei macchinari collegati in uscita. */
    idNode_t *sensorList;      /**< ID dei sensori collegati (es. presenza in ingresso/uscita). */
    idNode_t *actuatorList;    /**< ID degli attuatori collegati (es. il motore che lo aziona). */
} nastro_t;

/**
 * @brief Nodo della coda interna del nastro.
 *
 * A differenza di bufferObj_t, tiene anche lo step di ingresso: serve
 * per calcolare quando l'oggetto ha completato l'attraversamento.
 */
typedef struct nastroObj {
    object_t *dato;                  /**< Payload del nodo. */
    int step_ingresso;                /**< Step di simulazione in cui l'oggetto è entrato sul nastro. */
    struct nastroObj *next;          /**< Nodo successivo, NULL se ultimo. */
} nastroObj_t;

/* ------------------------------------------------------------------ */
/*  FUNZIONI                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Crea un nastro vuoto, senza collegamenti.
 * @param ID Identificativo del nastro, non vuoto e < IDLENGTH.
 * @param capacity Numero massimo di oggetti trasportabili insieme (deve essere > 0).
 * @param velocita Passi di simulazione per attraversare il nastro (deve essere > 0).
 * @param errCode puntatore opzionale (può essere NULL) in cui viene scritto
 *        OP_SUCCESS oppure un codice ERR_* (vedi errors.h).
 * @return Puntatore al nastro allocato, o NULL in caso di errore.
 */
nastro_t *nastro_create( const char *ID, int capacity, int velocita, short int *errCode );

/**
 * @brief Elimina il nastro (nodi di coda + liste interne), non gli oggetti contenuti.
 * @param n Puntatore al nastro.
 */
void nastro_delete( nastro_t *n );

/**
 * @brief Controlla se il nastro è pieno.
 * @param n Puntatore al nastro.
 * @return true se pieno, false se non pieno.
 */
bool nastro_isFull( const nastro_t *n );

/**
 * @brief Controlla se il nastro è vuoto.
 * @param n Puntatore al nastro.
 * @return true se vuoto, false se non vuoto.
 */
bool nastro_isEmpty( const nastro_t *n );

/**
 * @brief Restituisce il numero di oggetti attualmente sul nastro.
 *
 * Il numero di oggetti è sempre >= 0, quindi ERR_NULL_PTR (-1) è un
 * valore di ritorno inequivocabile per il caso di errore.
 * @param n Puntatore al nastro.
 * @return Numero di oggetti sul nastro, oppure ERR_NULL_PTR se n è NULL.
 */
int nastro_getCount( const nastro_t *n );

/**
 * @brief ID del nastro.
 * @param n Puntatore al nastro.
 * @return Puntatore costante alla stringa ID, oppure NULL se n è NULL.
 */
const char *nastro_getID( const nastro_t *n );

/**
 * @brief Capacità massima del nastro.
 * @param n Puntatore al nastro.
 * @return Capacità del nastro, oppure ERR_NULL_PTR se n è NULL.
 */
int nastro_getCapacity( const nastro_t *n );

/**
 * @brief Passi di simulazione necessari per attraversare il nastro.
 * @param n Puntatore al nastro.
 * @return Velocità del nastro, oppure ERR_NULL_PTR se n è NULL.
 */
int nastro_getVelocita( const nastro_t *n );

/**
 * @brief Imposta una nuova velocità (passi di simulazione per attraversare il nastro).
 *
 * Nota: la velocità è usata da nastro_removeReadyObject al momento
 * della chiamata, non memorizzata per singolo oggetto — quindi un
 * cambio di velocità si applica SUBITO anche agli oggetti già in
 * transito (accelera/rallenta tutto il nastro, non solo i nuovi
 * ingressi). Se invece serve che ogni oggetto mantenga il tempo di
 * attraversamento calcolato al momento del suo ingresso, va discusso
 * insieme: richiede di salvare la velocità anche in nastroObj_t.
 * @param n Puntatore al nastro.
 * @param velocita Nuova velocità (deve essere > 0).
 * @return OP_SUCCESS se impostata, un codice ERR_* (vedi errors.h) altrimenti.
 */
short int nastro_setVelocita( nastro_t *n, int velocita );

/**
 * @brief Aggiunge un macchinario collegato in ingresso al nastro.
 * @param n Puntatore al nastro.
 * @param ID ID del macchinario da aggiungere come ingresso.
 * @return OP_SUCCESS se aggiunto, ERR_DUPLICATE se l'ID è già presente,
 *         un altro codice ERR_* (vedi errors.h) per altri errori.
 */
short int nastro_addInput( nastro_t *n, const char *ID );

/**
 * @brief Aggiunge un macchinario collegato in uscita al nastro.
 * @param n Puntatore al nastro.
 * @param ID ID del macchinario da aggiungere come uscita.
 * @return OP_SUCCESS se aggiunto, ERR_DUPLICATE se l'ID è già presente,
 *         un altro codice ERR_* (vedi errors.h) per altri errori.
 */
short int nastro_addOutput( nastro_t *n, const char *ID );

/**
 * @brief Numero di macchinari collegati in ingresso al nastro.
 * @param n Puntatore al nastro.
 * @return Numero di ingressi (sempre >= 0), oppure ERR_NULL_PTR se n è NULL.
 */
int nastro_getInputCount( const nastro_t *n );

/**
 * @brief Legge l'ID dell'ingresso all'indice indicato (0-based), copiandolo in outID.
 * @param n Puntatore al nastro.
 * @param index Indice dell'ingresso richiesto (0 = primo).
 * @param outID Buffer di dimensione almeno IDLENGTH in cui viene copiato l'ID.
 * @return OP_SUCCESS se trovato, un codice ERR_* (vedi errors.h) altrimenti.
 */
short int nastro_getInputAt( const nastro_t *n, int index, char outID[IDLENGTH] );

/**
 * @brief Controlla se un dato ID è tra gli ingressi collegati al nastro.
 * @param n Puntatore al nastro.
 * @param ID ID cercato.
 * @return true se presente, false altrimenti (anche se n è NULL).
 */
bool nastro_hasInput( const nastro_t *n, const char *ID );

/**
 * @brief Numero di macchinari collegati in uscita al nastro.
 * @param n Puntatore al nastro.
 * @return Numero di uscite (sempre >= 0), oppure ERR_NULL_PTR se n è NULL.
 */
int nastro_getOutputCount( const nastro_t *n );

/**
 * @brief Legge l'ID dell'uscita all'indice indicato (0-based), copiandolo in outID.
 * @param n Puntatore al nastro.
 * @param index Indice dell'uscita richiesta (0 = prima).
 * @param outID Buffer di dimensione almeno IDLENGTH in cui viene copiato l'ID.
 * @return OP_SUCCESS se trovato, un codice ERR_* (vedi errors.h) altrimenti.
 */
short int nastro_getOutputAt( const nastro_t *n, int index, char outID[IDLENGTH] );

/**
 * @brief Controlla se un dato ID è tra le uscite collegate al nastro.
 * @param n Puntatore al nastro.
 * @param ID ID cercato.
 * @return true se presente, false altrimenti (anche se n è NULL).
 */
bool nastro_hasOutput( const nastro_t *n, const char *ID );

/**
 * @brief Aggiunge un sensore collegato a questo nastro.
 * @param n Puntatore al nastro.
 * @param ID ID del sensore da aggiungere.
 * @return OP_SUCCESS se aggiunto, ERR_DUPLICATE se l'ID è già presente,
 *         un altro codice ERR_* (vedi errors.h) per altri errori.
 */
short int nastro_addSensor( nastro_t *n, const char *ID );

/**
 * @brief Numero di sensori collegati al nastro.
 * @param n Puntatore al nastro.
 * @return Numero di sensori (sempre >= 0), oppure ERR_NULL_PTR se n è NULL.
 */
int nastro_getSensorCount( const nastro_t *n );

/**
 * @brief Legge l'ID del sensore all'indice indicato (0-based), copiandolo in outID.
 * @param n Puntatore al nastro.
 * @param index Indice del sensore richiesto (0 = primo).
 * @param outID Buffer di dimensione almeno IDLENGTH in cui viene copiato l'ID.
 * @return OP_SUCCESS se trovato, un codice ERR_* (vedi errors.h) altrimenti.
 */
short int nastro_getSensorAt( const nastro_t *n, int index, char outID[IDLENGTH] );

/**
 * @brief Controlla se un dato ID è tra i sensori collegati al nastro.
 * @param n Puntatore al nastro.
 * @param ID ID cercato.
 * @return true se presente, false altrimenti (anche se n è NULL).
 */
bool nastro_hasSensor( const nastro_t *n, const char *ID );

/**
 * @brief Aggiunge un attuatore collegato a questo nastro.
 * @param n Puntatore al nastro.
 * @param ID ID dell'attuatore da aggiungere.
 * @return OP_SUCCESS se aggiunto, ERR_DUPLICATE se l'ID è già presente,
 *         un altro codice ERR_* (vedi errors.h) per altri errori.
 */
short int nastro_addActuator( nastro_t *n, const char *ID );

/**
 * @brief Numero di attuatori collegati al nastro.
 * @param n Puntatore al nastro.
 * @return Numero di attuatori (sempre >= 0), oppure ERR_NULL_PTR se n è NULL.
 */
int nastro_getActuatorCount( const nastro_t *n );

/**
 * @brief Legge l'ID dell'attuatore all'indice indicato (0-based), copiandolo in outID.
 * @param n Puntatore al nastro.
 * @param index Indice dell'attuatore richiesto (0 = primo).
 * @param outID Buffer di dimensione almeno IDLENGTH in cui viene copiato l'ID.
 * @return OP_SUCCESS se trovato, un codice ERR_* (vedi errors.h) altrimenti.
 */
short int nastro_getActuatorAt( const nastro_t *n, int index, char outID[IDLENGTH] );

/**
 * @brief Controlla se un dato ID è tra gli attuatori collegati al nastro.
 * @param n Puntatore al nastro.
 * @param ID ID cercato.
 * @return true se presente, false altrimenti (anche se n è NULL).
 */
bool nastro_hasActuator( const nastro_t *n, const char *ID );

/**
 * @brief Inserisce un oggetto in coda al nastro (lato ingresso).
 *
 * Non modifica l'oggetto puntato da object; ne salva solo il puntatore.
 * @param n Puntatore al nastro.
 * @param object Puntatore all'oggetto da inserire.
 * @param step_corrente Step di simulazione corrente: viene marcato
 *        sull'oggetto per calcolare in seguito quando ha completato
 *        l'attraversamento.
 * @return OP_SUCCESS se inserito, ERR_FULL se il nastro è pieno, un
 *         altro codice ERR_* (vedi errors.h) per altri errori.
 */
int nastro_insertObject( nastro_t *n, object_t *object, int step_corrente );

/**
 * @brief Toglie dal nastro l'oggetto in testa, SOLO se ha completato
 *        l'attraversamento (step_corrente - step_ingresso >= velocita).
 *
 * Se l'oggetto in testa non è ancora pronto, non lo rimuove (a
 * differenza di buffer_removeObject, che rimuove sempre se la coda
 * non è vuota) e ritorna NULL: chi chiama deve quindi trattare NULL
 * come "non c'è nulla di pronto ora", non come errore.
 * @param n Puntatore al nastro.
 * @param step_corrente Step di simulazione corrente.
 * @return Puntatore all'oggetto pronto per uscire, o NULL se il nastro
 *         è vuoto o l'oggetto in testa non ha ancora finito l'attraversamento.
 */
/**
 * @brief Controlla se l'oggetto in testa ha completato l'attraversamento,
 *        SENZA rimuoverlo (a differenza di nastro_removeReadyObject).
 *
 * Serve al controllore per verificare che ci sia posto a valle PRIMA di
 * rimuovere l'oggetto dal nastro — stesso motivo di machine_isReady/
 * isp_isReady: senza questa funzione, l'unico modo per sapere se il
 * nastro è pronto sarebbe chiamare nastro_removeReadyObject, che però
 * rilascia comunque l'oggetto, rischiando di perderlo se a valle non
 * c'è posto.
 * @param n Puntatore al nastro.
 * @param step_corrente Step di simulazione corrente.
 * @return true se il nastro non è vuoto e l'oggetto in testa è pronto
 *         per uscire, false altrimenti (anche se n è NULL).
 */
bool nastro_isReady( const nastro_t *n, int step_corrente );

object_t *nastro_removeReadyObject( nastro_t *n, int step_corrente );

/**
 * @brief Stampa lo stato del nastro (collegamenti e oggetti contenuti).
 * @param n Puntatore al nastro.
 */
void nastro_print( const nastro_t *n );

#endif /* NASTRO_H */
