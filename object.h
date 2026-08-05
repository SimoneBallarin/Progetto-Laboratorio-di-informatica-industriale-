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
    short int priority;     /**< priorità dell'oggetto. */
    char type;      /**< tipologia: A = acciaio, B = rame. */
    char ID_LOCATION[IDLENGTH];  /**< ID della locazione corrente. */
    int stepCreation;           /**< Step temporale all'entrata della linea. */
    int stepOut;           /**< step temporale all'uscita della linea. */
    double dimensionX;   /**< Dimensione oggetto fittizia. */
} object_t;

/* ------------------------------------------------------------------ */
/*  FUNZIONI                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief creazione oggetto.
 * @param Obj_ID id dell'oggetto creato.
 * @param Obj_priority priorità dell'oggetto creato, 10 massima priorità.
 * @param Obj_type tipologia oggetto: A = acciaio, B = rame.
 * @param step step temporale in cui l'oggetto viene creato.
 * @param Obj_dimensionX dimensione fittizia alla creazione.
 * @return puntatore all'oggetto creato, NULL se malloc fallisce.
 */
object_t *object_create( const char *Obj_ID, short int Obj_priority, char Obj_type, int step, double Obj_dimensionX );

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
 * @brief Restituisce la priorità dell'oggetto.
 * @param Obj puntatore all'oggetto.
 * @return Priorità dell'oggetto.
 */
short int object_getPriority( const object_t *Obj );

/**
 * @brief Restituisce l'ID della location corrente dell'oggetto.
 * @param Obj puntatore all'oggetto.
 * @return Puntatore costante alla stringa ID.
 */
const char *object_getLocation( const object_t *Obj );

/**
 * @brief Restituisce il tipo dell'oggetto.
 * @param Obj puntatore all'oggetto.
 * @return Tipo dell'oggetto.
 */
char object_getType( const object_t *Obj );

/**
 * @brief Restituisce l'ID dell'oggetto.
 * @param Obj puntatore all'oggetto.
 * @return Puntatore costante alla stringa ID.
 */
const char *object_getID( const object_t *Obj );

/**
 * @brief Step temporale in cui l'oggetto è entrato nella linea.
 * @param Obj puntatore all'oggetto.
 * @return step di creazione.
 */
int object_getStepCreation( const object_t *Obj );

/**
 * @brief Step temporale in cui l'oggetto è uscito dalla linea.
 * @param Obj puntatore all'oggetto.
 * @return step di uscita (valore non ancora significativo se l'oggetto non è uscito).
 */
int object_getStepOut( const object_t *Obj );

/**
 * @brief Dimensione fittizia corrente dell'oggetto.
 * @param Obj puntatore all'oggetto.
 * @return valore di dimensionX.
 */
double object_getDimensionX( const object_t *Obj );

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
