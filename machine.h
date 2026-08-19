/**
 * @file machine.h
 * @brief Stazione di lavorazione M.
 *
 * Struttura del modulo identica a buffer.h/nastro.h: inputList/outputList
 * per i collegamenti nella topologia della cella, sensorList/actuatorList
 * per sensori/attuatori agganciati (tipicamente nessun sensore per M,
 * la traccia mette il sensore di qualita' solo su ISP - vedi sez. 1.2 e 5.1).
 *
 * Modello scelto: la macchina lavora UN oggetto alla volta (sez. 1.2:
 * "M e ISP: possono lavorare un solo oggetto alla volta; se occupate,
 * non accettano nuovi oggetti"). Il tempo di lavorazione e' una proprieta'
 * fissa della macchina (non dell'oggetto: object_t non ha un campo
 * "tempo di lavorazione" - vedi object.h), impostata alla creazione.
 */

#ifndef MACHINE_H
#define MACHINE_H

#include <stdbool.h>
#include "object.h"
#include "errors.h"
#include "idlist.h"

typedef enum {
    MACCHINA_LIBERA    = 0,
    MACCHINA_OCCUPATA  = 1
} StatoMacchina;

typedef struct {
    char ID[IDLENGTH];
    int  tempo_lavorazione;            /**< Passi di simulazione per lavorare un pezzo. */
    StatoMacchina stato;
    object_t *oggetto_in_lavorazione;  /**< NULL se la macchina e' libera. */
    int  step_inizio_lavorazione;      /**< Step in cui e' iniziata la lavorazione corrente. */
    double tolleranza_lavorazione;     /**< Rumore massimo (frazione, es. 0.11 = 11%) applicato a
                                         *   dimensionX/raggio al rilascio (vedi machine_tryRelease). */
    idNode_t *inputList;
    idNode_t *outputList;
    idNode_t *sensorList;
    idNode_t *actuatorList;
} machine_t;

/**
 * @brief Crea una macchina libera, senza collegamenti.
 * @param ID Identificativo (es. "M1"), non vuoto e < IDLENGTH.
 * @param tempo_lavorazione Passi di simulazione per lavorare un pezzo (deve essere > 0).
 * @param errCode puntatore opzionale (puo' essere NULL) in cui viene scritto
 *        OP_SUCCESS oppure un codice ERR_* (vedi errors.h).
 * @return Puntatore alla macchina allocata, o NULL in caso di errore.
 */
machine_t *machine_create( const char *ID, int tempo_lavorazione, short int *errCode );

/**
 * @brief Elimina la macchina (nodi delle liste interne), non l'oggetto
 *        eventualmente in lavorazione (resta di proprieta' di chi l'ha creato).
 * @param m Puntatore alla macchina.
 */
void machine_delete( machine_t *m );

/**
 * @brief Controlla se la macchina e' occupata.
 * @param m Puntatore alla macchina.
 * @return true se occupata, false se libera (anche se m e' NULL).
 */
bool machine_isBusy( const machine_t *m );

/**
 * @brief Controlla se la macchina ha completato la lavorazione corrente,
 *        SENZA rilasciare l'oggetto (a differenza di machine_tryRelease).
 *
 * Serve al controllore per decidere se c'e' spazio a valle PRIMA di
 * rilasciare l'oggetto: senza questa funzione, l'unico modo per sapere
 * se la macchina e' pronta sarebbe chiamare machine_tryRelease, che pero'
 * rilascia comunque l'oggetto, rischiando di perderlo se a valle non
 * c'e' posto.
 * @param m Puntatore alla macchina.
 * @param step_corrente Step di simulazione corrente.
 * @return true se occupata e la lavorazione e' completata, false altrimenti
 *         (anche se m e' NULL).
 */
bool machine_isReady( const machine_t *m, int step_corrente );

/**
 * @brief ID della macchina.
 * @param m Puntatore alla macchina.
 * @return Puntatore costante alla stringa ID, oppure NULL se m e' NULL.
 */
const char *machine_getID( const machine_t *m );

/**
 * @brief Tempo di lavorazione configurato per questa macchina.
 * @param m Puntatore alla macchina.
 * @return Tempo di lavorazione, oppure ERR_NULL_PTR se m e' NULL.
 */
int machine_getTempoLavorazione( const machine_t *m );

/**
 * @brief Imposta la tolleranza di lavorazione: la percentuale massima di
 *        rumore casuale applicata a dimensionX/raggio dell'oggetto al
 *        momento del rilascio (vedi machine_tryRelease).
 *
 * Non obbligatorio: machine_create imposta già un valore di default
 * (0.11, cioè 11%). Va chiamata solo se serve una tolleranza diversa
 * per una specifica macchina.
 * @param m Puntatore alla macchina.
 * @param tolleranza Frazione (es. 0.11 per 11%), deve essere >= 0.
 * @return OP_SUCCESS se impostata, un codice ERR_* (vedi errors.h) altrimenti.
 */
short int machine_setTolleranzaLavorazione( machine_t *m, double tolleranza );

/**
 * @brief Legge la tolleranza di lavorazione attualmente impostata.
 * @param m Puntatore alla macchina.
 * @return La tolleranza (frazione, es. 0.11 = 11%), oppure -1.0 se m è NULL.
 */
double machine_getTolleranzaLavorazione( const machine_t *m );

short int machine_addInput( machine_t *m, const char *ID );
short int machine_addOutput( machine_t *m, const char *ID );
int machine_getInputCount( const machine_t *m );
short int machine_getInputAt( const machine_t *m, int index, char outID[IDLENGTH] );
bool machine_hasInput( const machine_t *m, const char *ID );
int machine_getOutputCount( const machine_t *m );
short int machine_getOutputAt( const machine_t *m, int index, char outID[IDLENGTH] );
bool machine_hasOutput( const machine_t *m, const char *ID );

short int machine_addSensor( machine_t *m, const char *ID );
int machine_getSensorCount( const machine_t *m );
short int machine_getSensorAt( const machine_t *m, int index, char outID[IDLENGTH] );
bool machine_hasSensor( const machine_t *m, const char *ID );

short int machine_addActuator( machine_t *m, const char *ID );
int machine_getActuatorCount( const machine_t *m );
short int machine_getActuatorAt( const machine_t *m, int index, char outID[IDLENGTH] );
bool machine_hasActuator( const machine_t *m, const char *ID );

/**
 * @brief Avvia la lavorazione di un oggetto, se la macchina e' libera.
 *
 * Non modifica l'oggetto puntato da object; ne salva solo il puntatore.
 * @param m Puntatore alla macchina.
 * @param object Puntatore all'oggetto da lavorare.
 * @param step_corrente Step di simulazione corrente.
 * @return OP_SUCCESS se ammesso, ERR_FULL se la macchina e' gia' occupata,
 *         un altro codice ERR_* (vedi errors.h) per altri errori.
 */
short int machine_admit( machine_t *m, object_t *object, int step_corrente );

/**
 * @brief Se il tempo di lavorazione e' trascorso, libera la macchina e
 *        restituisce l'oggetto lavorato.
 *
 * PRIMA di restituirlo, altera dimensionX e raggio dell'oggetto con un
 * rumore casuale indipendente per ciascuno dei due valori, entro +/- la
 * tolleranza di lavorazione (vedi machine_setTolleranzaLavorazione,
 * default 11%): simula una lavorazione reale mai perfettamente precisa,
 * cosi' l'ISP a valle puo' effettivamente rilevare pezzi fuori
 * tolleranza. Usa rand(): il chiamante deve aver seminato il generatore
 * una sola volta all'avvio del programma (stessa convenzione di
 * S_Qualita.h e Motore.h).
 * @param m Puntatore alla macchina.
 * @param step_corrente Step di simulazione corrente.
 * @return Puntatore all'oggetto lavorato (con dimensionX/raggio alterati),
 *         o NULL se la macchina e' libera o se la lavorazione non e'
 *         ancora completata.
 */
object_t *machine_tryRelease( machine_t *m, int step_corrente );

/**
 * @brief Stampa lo stato della macchina.
 * @param m Puntatore alla macchina.
 */
void machine_print( const machine_t *m );

#endif /* MACHINE_H */
