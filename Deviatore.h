/**
 * @file Deviatore.h
 * @brief Attuatore "deviatore": smista fisicamente un oggetto verso una
 *        delle uscite di una ISP (o di un buffer/nastro), muovendosi di
 *        una posizione alla volta e rispettando un tempo minimo tra due
 *        commutazioni consecutive (sez. 5.2 del progetto preliminare).
 */
#ifndef DEVIATORE_H
#define DEVIATORE_H
#include "object.h"
#include <stdbool.h>

/** @brief Stato di accensione del deviatore: se spento non si muove (vedi posizionamento_deviatore). */
typedef enum {
    DEVIATORE_ON  = 1,
    DEVIATORE_OFF = 0
} StatoDeviatore;

/** @brief Indica se il deviatore ha raggiunto la posizione richiesta (posizione_attuale == posizione_target). */
typedef enum {
    POSIZIONE_OK     = 1,
    POSIZIONE_NOT_OK = 0
} PosizioneDeviatore;

/** @brief Stato del deviatore. */
typedef struct {
    char ID[IDLENGTH];
    int  posizione_target;
    int  posizione_attuale;
    StatoDeviatore status;
    StatoDeviatore status_precedente;
    PosizioneDeviatore inPosizione;
} Deviatore;

/**
 * @brief Vincolo del "tempo minimo tra due commutazioni" (sez. 5.2):
 *        tenuto separato da Deviatore per non mescolare stato fisico
 *        (posizione) e temporizzazione del vincolo.
 */
typedef struct {
    int  time_last_commutazione;      /**< Istante dell'ultimo cambio di posizione_target accettato. */
    int  tempo_minimo_tra_commutazioni;
    bool prima_commutazione_fatta;    /**< False finche' non e' mai avvenuta una commutazione. */
} DeviatoreTime;

/* Tutte le funzioni ritornano OP_SUCCESS (vedi errors.h, vale 1) in
 * successo o un codice ERR_* su puntatore NULL. I getter che
 * restituiscono bool restituiscono false su puntatore NULL (un bool non
 * puo' distinguere un errore da un valore legittimo). */

/**
 * @brief Inizializza un deviatore in posizione 0, spento.
 * @param d Puntatore al deviatore.
 * @param ID Identificativo del deviatore.
 * @return OP_SUCCESS se inizializzato, un codice ERR_* altrimenti.
 */
int  deviatore_init(Deviatore *d, const char *ID);

/**
 * @brief Inizializza il timer del vincolo "tempo minimo tra commutazioni".
 * @param dt Puntatore al timer.
 * @param tempo_minimo_tra_commutazioni Passi minimi tra due commutazioni (>= 0).
 * @return OP_SUCCESS se inizializzato, un codice ERR_* altrimenti.
 */
int  deviatoretime_init(DeviatoreTime *dt, int tempo_minimo_tra_commutazioni);

/**
 * @brief Accende/spegne il deviatore.
 * @param d Puntatore al deviatore.
 * @param stato Nuovo stato (DEVIATORE_ON/DEVIATORE_OFF).
 * @return OP_SUCCESS se impostato, un codice ERR_* altrimenti.
 */
int  set_deviatore(Deviatore *d, StatoDeviatore stato);

/**
 * @brief Richiede una nuova posizione target per il deviatore.
 *
 * Applica il vincolo "tempo minimo tra due commutazioni" (sez. 5.2): se
 * non e' ancora trascorso dt->tempo_minimo_tra_commutazioni dall'ultima
 * commutazione accettata, la richiesta viene ignorata e si ritorna
 * ERR_NOT_SUPPORTED (il deviatore resta sulla posizione target
 * precedente, va richiamata ai passi successivi finche' non e'
 * accettata). Nessun effetto (ritorna subito OP_SUCCESS) se
 * nuova_posizione_target coincide gia' con quella corrente.
 * @param d Puntatore al deviatore.
 * @param dt Puntatore al timer del vincolo.
 * @param nuova_posizione_target Nuova posizione richiesta.
 * @param time_globale Step di simulazione corrente.
 * @return OP_SUCCESS se accettata (o gia' su quella posizione),
 *         ERR_NOT_SUPPORTED se il vincolo temporale non e' rispettato,
 *         un altro codice ERR_* per puntatori NULL.
 */
int  deviatore_imposta_target(Deviatore *d, DeviatoreTime *dt,
                               int nuova_posizione_target, int time_globale);

/**
 * @brief Fa avanzare posizione_attuale verso posizione_target di una
 *        unita' per chiamata (coerente con la simulazione a passi
 *        discreti, sez. 6). No-op se il deviatore e' spento.
 * @param d Puntatore al deviatore.
 * @return OP_SUCCESS se aggiornato, un codice ERR_* altrimenti.
 */
int  posizionamento_deviatore(Deviatore *d);

/**
 * @brief Controlla se il deviatore e' acceso.
 * @param d Puntatore al deviatore.
 * @return true se acceso, false se spento (anche se d e' NULL).
 */
bool get_status_deviatore(const Deviatore *d);

/**
 * @brief Posizione attuale del deviatore.
 * @param d Puntatore al deviatore.
 * @return Posizione attuale, oppure ERR_NULL_PTR se d e' NULL.
 */
int  get_posizione(const Deviatore *d);

/**
 * @brief Controlla se il deviatore ha raggiunto la posizione target.
 * @param d Puntatore al deviatore.
 * @return true se in posizione, false altrimenti (anche se d e' NULL).
 */
bool get_inPosizione(const Deviatore *d);

#endif
