/**
 * @file isp.h
 * @brief Stazione di controllo qualita' ISP.
 *
 * Stessa struttura di machine.h (lavora un oggetto alla volta), ma
 * compone al suo interno il sensore di qualita' (S_Qualita.h) invece di
 * limitarsi a un tempo fisso: il "rilascio" dell'oggetto include anche
 * l'esito della classificazione (CONFORME/RIVALUTAZIONE/SCARTO), che il
 * controllore usera' per decidere verso quale uscita instradarlo (in
 * genere tramite un Deviatore collegato subito dopo).
 */

#ifndef ISP_H
#define ISP_H

#include <stdbool.h>
#include "object.h"
#include "errors.h"
#include "idlist.h"
#include "S_Qualita.h"

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
    SensoreQualita sensore;
    MalfunzionamentoSensore guasto;
    idNode_t *inputList;
    idNode_t *outputList;              /**< Tipicamente un solo elemento: il deviatore a valle. */
    idNode_t *sensorList;
    idNode_t *actuatorList;
} isp_t;

/**
 * @brief Crea una ISP libera, senza collegamenti, con guasto disabilitato.
 * @param ID Identificativo (es. "ISP1"), non vuoto e < IDLENGTH.
 * @param tempo_controllo Passi di simulazione per controllare un pezzo (deve essere > 0).
 * @param dimensionX_target Valore di riferimento per la dimensione, vedi S_Qualita.h.
 * @param raggio_target Valore di riferimento per il raggio, vedi S_Qualita.h.
 * @param errCode puntatore opzionale (puo' essere NULL) in cui viene scritto
 *        OP_SUCCESS oppure un codice ERR_* (vedi errors.h).
 * @return Puntatore alla ISP allocata, o NULL in caso di errore.
 */
isp_t *isp_create( const char *ID, int tempo_controllo, int dimensionX_target, int raggio_target, short int *errCode );

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
 * @brief Abilita/disabilita e configura il guasto simulato del sensore
 *        di qualita' (sez. 5.3 del progetto).
 * @param i Puntatore alla ISP.
 * @param abilitato true per abilitare il guasto.
 * @param time_error Passi di funzionamento OK prima del guasto (deve essere > 0).
 * @param time_ok Passi di guasto prima di tornare OK (deve essere > 0).
 * @return OP_SUCCESS se impostato, un codice ERR_* (vedi errors.h) altrimenti.
 */
short int isp_impostaGuasto( isp_t *i, bool abilitato, int time_error, int time_ok );

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
 * @brief Se il tempo di controllo e' trascorso, libera la ISP, esegue
 *        la lettura del sensore di qualita' e restituisce l'oggetto.
 * @param i Puntatore alla ISP.
 * @param step_corrente Step di simulazione corrente.
 * @param outEsito puntatore in cui viene scritto l'esito (CONFORME/
 *        RIVALUTAZIONE/SCARTO); scritto SOLO se la funzione non
 *        restituisce NULL. Puo' essere NULL se l'esito non serve.
 * @return Puntatore all'oggetto controllato, o NULL se la ISP e' libera
 *         o se il controllo non e' ancora completato.
 */
object_t *isp_tryRelease( isp_t *i, int step_corrente, TipoQualita *outEsito );

/**
 * @brief Stampa lo stato della ISP.
 * @param i Puntatore alla ISP.
 */
void isp_print( const isp_t *i );

#endif /* ISP_H */
