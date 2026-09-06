/**
 * @file object.h
 * @brief Definizione degli oggetti.
 *
 */

#ifndef OBJECT_H
#define OBJECT_H

#include <stdbool.h>
#include "errors.h"

#define IDLENGTH 20

/** @brief Valore usato per indicare "nessun ID" (es. nessuna destinazione/provenienza). */
#define ID_NONE "NULL"

/** @brief Priorità minima ammessa per un oggetto. */
#define PRIORITY_MIN 0

/** @brief Priorità massima ammessa per un oggetto. */
#define PRIORITY_MAX 10

/** @brief Valore di stepOut quando l'oggetto non è ancora uscito dalla linea. */
#define STEP_OUT_NONE -1

/* ------------------------------------------------------------------ */
/*  STRUCT                                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Descrizione della struttura oggetto.
 *
 *
 */
typedef struct {
    char ID[IDLENGTH];     /**< ID dell'oggetto. */
    short int priority;     /**< priorità dell'oggetto, vedi PRIORITY_MIN/PRIORITY_MAX. */
    char type;      /**< tipologia: A = acciaio, B = rame. */
    char ID_LOCATION[IDLENGTH];  /**< ID della locazione corrente. */
    int stepCreation;           /**< Step temporale all'entrata della linea (in un buffer di ingresso). */
    int stepPartial;     /**< Step in cui l'oggetto occupa per la prima volta una stazione di
                                   *   lavorazione/controllo (non un buffer): distingue il tempo di
                                   *   ATTESA in coda (stepPartial - stepCreation) dal tempo di
                                   *   PROCESSO vero e proprio (stepOut - stepPartial). Vale
                                   *   STEP_OUT_NONE finché l'oggetto non ha ancora lasciato il buffer
                                   *   di ingresso. */
    int stepOut;           /**< step temporale all'uscita della linea, STEP_OUT_NONE se non ancora uscito. */
    double dimensionX;   /**< Dimensione oggetto fittizia. */
    double raggio;        /**< Raggio dell'oggetto (per il calcolo del materiale in S_Qualita). */
} object_t;

/* ------------------------------------------------------------------ */
/*  FUNZIONI                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief creazione oggetto.
 * @param Obj_ID id dell'oggetto creato.
 * @param Obj_priority priorità dell'oggetto creato, deve essere compresa tra PRIORITY_MIN e PRIORITY_MAX (10 = massima priorità).
 * @param Obj_type tipologia oggetto: A = acciaio, B = rame.
 * @param step step temporale in cui l'oggetto viene creato.
 * @param Obj_dimensionX dimensione fittizia alla creazione (deve essere >= 0).
 * @param Obj_raggio raggio dell'oggetto alla creazione (deve essere >= 0).
 * @param errCode puntatore opzionale (può essere NULL) in cui viene scritto OP_SUCCESS
 *        oppure un codice ERR_* (vedi errors.h) che spiega perché la creazione è fallita
 *        (ERR_NULL_PTR, ERR_ID_INVALID, ERR_OUT_OF_RANGE, ERR_ALLOC).
 * @return puntatore all'oggetto creato, o NULL in caso di errore (vedi *errCode per il motivo).
 */
object_t *object_create( const char *Obj_ID, short int Obj_priority, char Obj_type, int step, double Obj_dimensionX, double Obj_raggio, short int *errCode );

/**
 * @brief cambia la posizione dell'oggetto.
 * @param Obj puntatore all'oggetto.
 * @param newID ID della nuova location.
 * @return OP_SUCCESS se l'oggetto è stato spostato, un codice ERR_* (vedi errors.h) in caso di errore.
 */
short int object_setLocation( object_t *Obj, const char *newID );

/**
 * @brief imposta lo step temporale di uscita dalla linea.
 * @param Obj puntatore all'oggetto.
 * @param step step temporale di uscita.
 * @return OP_SUCCESS se l'operazione è andata a buon fine, un codice ERR_* (vedi errors.h) in caso di errore.
 */
short int object_setStepOut( object_t *Obj, int step );

/**
 * @brief imposta lo step in cui l'oggetto occupa per la prima volta una
 *        stazione di lavorazione/controllo (non un buffer di attesa).
 *
 * Va chiamata UNA SOLA VOLTA per oggetto, la prima volta che lascia il
 * buffer di ingresso: chiamate successive (es. quando l'oggetto entra
 * nella stazione successiva della pipeline) non devono sovrascrivere il
 * valore già impostato. Serve a separare, a fine simulazione, il tempo
 * di ATTESA in coda (stepPartial - stepCreation) dal tempo di
 * PROCESSO vero e proprio (stepOut - stepPartial) — vedi
 * object_getStepPartial e statistiche_registraCompletamento.
 * @param Obj puntatore all'oggetto.
 * @param step step temporale di inizio processo.
 * @return OP_SUCCESS se l'operazione è andata a buon fine, un codice
 *         ERR_* (vedi errors.h) in caso di errore (incluso
 *         ERR_DUPLICATE se era già stato impostato in precedenza).
 */
short int object_setStepPartial( object_t *Obj, int step );

/**
 * @brief imposta/aggiorna la dimensione fittizia dell'oggetto.
 *
 * Utile per simulare letture o lavorazioni che alterano la dimensione,
 * ad esempio per generare scarti per dimensione fuori tolleranza in ISP.
 * @param Obj puntatore all'oggetto.
 * @param newDimension nuovo valore della dimensione.
 * @return OP_SUCCESS se l'operazione è andata a buon fine, un codice ERR_* (vedi errors.h) in caso di errore.
 */
short int object_setDimensionX( object_t *Obj, double newDimension );

/**
 * @brief imposta/aggiorna il raggio dell'oggetto.
 *
 * Stesso utilizzo di object_setDimensionX (letture/lavorazioni che
 * alterano il pezzo), ma sul raggio invece della dimensione.
 * @param Obj puntatore all'oggetto.
 * @param newRaggio nuovo valore del raggio (deve essere >= 0).
 * @return OP_SUCCESS se l'operazione è andata a buon fine, un codice ERR_* (vedi errors.h) in caso di errore.
 */
short int object_setRaggio( object_t *Obj, double newRaggio );

/**
 * @brief Restituisce la priorità dell'oggetto.
 *
 * La priorità di un oggetto valido è sempre compresa tra PRIORITY_MIN e
 * PRIORITY_MAX (quindi >= 0): questo rende ERR_NULL_PTR (-1) un valore
 * di ritorno inequivocabile per il caso di errore.
 * @param Obj puntatore all'oggetto.
 * @return Priorità dell'oggetto, oppure ERR_NULL_PTR se Obj è NULL.
 */
short int object_getPriority( const object_t *Obj );

/**
 * @brief Restituisce l'ID della location corrente dell'oggetto.
 * @param Obj puntatore all'oggetto.
 * @return Puntatore costante alla stringa ID_LOCATION, oppure NULL se Obj è NULL.
 *         Se all'oggetto non è ancora stata assegnata una locazione, la stringa
 *         restituita contiene il valore sentinella ID_NONE ("NULL") — da non
 *         confondere con il puntatore NULL restituito quando Obj stesso è NULL.
 */
const char *object_getLocation( const object_t *Obj );

/**
 * @brief Restituisce il tipo dell'oggetto.
 *
 * Il tipo è un char, non un puntatore: non può quindi restituire il
 * puntatore NULL. Il valore di errore usato quando Obj è NULL è il
 * carattere nullo '\0', che non è mai un tipo valido (i tipi validi
 * sono 'A' e 'B').
 * @param Obj puntatore all'oggetto.
 * @return Tipo dell'oggetto ('A' o 'B'), oppure '\0' se Obj è NULL.
 */
char object_getType( const object_t *Obj );

/**
 * @brief Restituisce l'ID dell'oggetto.
 * @param Obj puntatore all'oggetto.
 * @return Puntatore costante alla stringa ID, oppure NULL se Obj è NULL.
 */
const char *object_getID( const object_t *Obj );

/**
 * @brief Step temporale in cui l'oggetto è entrato nella linea.
 * @param Obj puntatore all'oggetto.
 * @return step di creazione (sempre >= 0), oppure ERR_NULL_PTR se Obj è NULL.
 */
int object_getStepCreation( const object_t *Obj );

/**
 * @brief Step temporale in cui l'oggetto è uscito dalla linea.
 * @param Obj puntatore all'oggetto.
 * @return step di uscita (>= 0 se l'oggetto è uscito, STEP_OUT_NONE se non
 *         ancora uscito), oppure ERR_NULL_PTR se Obj è NULL.
 */
int object_getStepOut( const object_t *Obj );

/**
 * @brief Step temporale in cui l'oggetto ha occupato per la prima volta
 *        una stazione di lavorazione/controllo (non un buffer).
 * @param Obj puntatore all'oggetto.
 * @return step di inizio processo (>= 0 se già avvenuto), STEP_OUT_NONE
 *         se l'oggetto è ancora nel buffer di ingresso, oppure
 *         ERR_NULL_PTR se Obj è NULL.
 */
int object_getStepPartial( const object_t *Obj );

/**
 * @brief Dimensione fittizia corrente dell'oggetto.
 * @param Obj puntatore all'oggetto.
 * @return valore di dimensionX (sempre >= 0), oppure ERR_NULL_PTR se Obj è NULL.
 */
double object_getDimensionX( const object_t *Obj );

/**
 * @brief Raggio corrente dell'oggetto.
 * @param Obj puntatore all'oggetto.
 * @return valore di raggio (sempre >= 0), oppure ERR_NULL_PTR se Obj è NULL.
 */
double object_getRaggio( const object_t *Obj );

/**
 * @brief Elimina un oggetto.
 * @param Obj puntatore all'oggetto.
 */
void object_delete( object_t *Obj );

/**
 * @brief Stampa le informazioni dell'oggetto.
 * @param Obj puntatore all'oggetto.
 */
void object_print( const object_t *Obj );

#endif /* OBJECT_H */
