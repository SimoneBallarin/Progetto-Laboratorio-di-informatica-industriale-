/**
 * @file isp.h
 * @brief Stazione di controllo qualita' ISP.
 *
 * Stessa struttura di machine.h e nastro.h: una entita' "vuota",
 * responsabile solo del proprio ciclo occupata/libera e del tempo di
 * attraversamento. NON possiede piu' nessun sensore/attuatore incorporato:
 * il SensoreQualita (e l'eventuale Deviatore per lo smistamento) sono
 * creati ed agganciati da fuori (dal Controllore, tramite
 * controllore_collegaSensoreQualita), esattamente con lo stesso schema
 * gia' usato per SensoreBuffer/SensorePresenza sui buffer e per
 * Motore/Deviatore su nastri/ISP.
 *
 * PRIMA di questa versione, isp_create chiamava internamente
 * sensore_qualita_init, creando il sensore in automatico alla nascita
 * dell'ISP - incoerente con machine_t/nastro_t (che non creano mai nulla
 * in automatico) e con SensoreBuffer/Motore/Deviatore (sempre creati
 * dopo, esplicitamente, dal Controllore). Ora isp_tryRelease restituisce
 * solo l'oggetto (come machine_tryRelease): il calcolo dell'esito
 * qualita' (get_qualita) e la scelta del materiale (get_Material)
 * avvengono nel Controllore, usando il sensore che gli e' stato
 * agganciato - se nessun sensore e' stato agganciato a una ISP (es. una
 * ISP "passacarte" con una sola uscita), quella ISP semplicemente non fa
 * nessun controllo qualita', senza bisogno di nessun target fittizio.
 */

#ifndef ISP_H
#define ISP_H

#include <stdbool.h>
#include "object.h"
#include "errors.h"
#include "idlist.h"

typedef enum {
    ISP_LIBERA    = 0,
    ISP_OCCUPATA  = 1
} StatoISP;

typedef struct {
    char ID[IDLENGTH];
    int  tempo_controllo;              /**< Passi di simulazione per controllare un pezzo. */
    StatoISP stato;
    object_t *oggetto_in_controllo;    /**< NULL se la ISP e' libera. */
    int  step_inizio_controllo;        /**< Step in cui e' iniziato il controllo corrente. */
    idNode_t *inputList;
    idNode_t *outputList;              /**< Tipicamente il deviatore/le uscite finali a valle. */
    idNode_t *sensorList;
    idNode_t *actuatorList;
} isp_t;

/**
 * @brief Crea una ISP libera, senza collegamenti e senza nessun sensore.
 * @param ID Identificativo (es. "ISP1"), non vuoto e < IDLENGTH.
 * @param tempo_controllo Passi di simulazione per controllare un pezzo (deve essere > 0).
 * @param errCode puntatore opzionale (puo' essere NULL) in cui viene scritto
 *        OP_SUCCESS oppure un codice ERR_* (vedi errors.h).
 * @return Puntatore alla ISP allocata, o NULL in caso di errore.
 */
isp_t *isp_create( const char *ID, int tempo_controllo, short int *errCode );

/**
 * @brief Elimina la ISP, non l'oggetto eventualmente in controllo.
 * @param i Puntatore alla ISP.
 */
void isp_delete( isp_t *i );

bool isp_isBusy( const isp_t *i );

/**
 * @brief Controlla se la ISP ha completato il controllo corrente, SENZA
 *        rilasciare l'oggetto (a differenza di isp_tryRelease). Stesso
 *        motivo di machine_isReady: evita di perdere l'oggetto se a
 *        valle non c'e' posto.
 * @param i Puntatore alla ISP.
 * @param step_corrente Step di simulazione corrente.
 * @return true se occupata e il controllo e' completato, false altrimenti
 *         (anche se i e' NULL).
 */
bool isp_isReady( const isp_t *i, int step_corrente );
const char *isp_getID( const isp_t *i );
int isp_getTempoControllo( const isp_t *i );

short int isp_addInput( isp_t *i, const char *ID );
short int isp_addOutput( isp_t *i, const char *ID );
int isp_getInputCount( const isp_t *i );
short int isp_getInputAt( const isp_t *i, int index, char outID[IDLENGTH] );
bool isp_hasInput( const isp_t *i, const char *ID );
int isp_getOutputCount( const isp_t *i );
short int isp_getOutputAt( const isp_t *i, int index, char outID[IDLENGTH] );
bool isp_hasOutput( const isp_t *i, const char *ID );

short int isp_addSensor( isp_t *i, const char *ID );
int isp_getSensorCount( const isp_t *i );
short int isp_getSensorAt( const isp_t *i, int index, char outID[IDLENGTH] );
bool isp_hasSensor( const isp_t *i, const char *ID );

short int isp_addActuator( isp_t *i, const char *ID );
int isp_getActuatorCount( const isp_t *i );
short int isp_getActuatorAt( const isp_t *i, int index, char outID[IDLENGTH] );
bool isp_hasActuator( const isp_t *i, const char *ID );

/**
 * @brief Avvia il controllo di un oggetto, se la ISP e' libera.
 * @param i Puntatore alla ISP.
 * @param object Puntatore all'oggetto da controllare.
 * @param step_corrente Step di simulazione corrente.
 * @return OP_SUCCESS se ammesso, ERR_FULL se la ISP e' gia' occupata,
 *         un altro codice ERR_* (vedi errors.h) per altri errori.
 */
short int isp_admit( isp_t *i, object_t *object, int step_corrente );

/**
 * @brief Se il tempo di controllo e' trascorso, libera la ISP e
 *        restituisce l'oggetto - SENZA calcolare nessun esito qualita'
 *        (a differenza della versione precedente): la ISP di per se'
 *        non sa piu' nulla di qualita', e' un puro timer come machine_t.
 *        Il calcolo dell'esito (se serve) spetta al Controllore, che ha
 *        il sensore agganciato tramite controllore_collegaSensoreQualita.
 * @param i Puntatore alla ISP.
 * @param step_corrente Step di simulazione corrente.
 * @return Puntatore all'oggetto controllato, o NULL se la ISP e' libera
 *         o se il controllo non e' ancora completato.
 */
object_t *isp_tryRelease( isp_t *i, int step_corrente );

/**
 * @brief Stampa lo stato della ISP.
 * @param i Puntatore alla ISP.
 */
void isp_print( const isp_t *i );

#endif /* ISP_H */
