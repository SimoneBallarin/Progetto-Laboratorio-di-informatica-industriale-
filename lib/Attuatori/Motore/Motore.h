/**
 * @file Motore.h
 * @brief Attuatore "motore": aziona il trasporto di un nastro o la
 *        lavorazione di una macchina, con una rampa di accelerazione e
 *        una temperatura simulata (sez. 5.2 del progetto preliminare).
 */
#ifndef MOTORE_H
#define MOTORE_H
#include "object.h"
#include <stdbool.h>

/** @brief Stato di accensione del motore. */
typedef enum {
    MOTORE_ON  = 1,
    MOTORE_OFF = 0
} MotorState;

/** @brief Riservato per usi futuri (es. un guasto del motore stesso, da
 *  distinguere da MotorError sotto): non ancora prodotto da nessuna
 *  funzione di questo modulo. */
typedef enum {
    MOTORE_OK     = 1,
    MOTORE_NOT_OK = 0
} MotoreStatus;

/** @brief Stato di un motore. */
typedef struct {
    char ID[IDLENGTH];
    int  velocita_target;
    int  velocita_attuale;
    int  acc;                  /**< Accelerazione del motore (unita' di velocita' al secondo). */
    MotorState status;         /**< Stato del motore: acceso o spento. */
    MotorState status_precedente;

    int temperatura_motore;
    int temperatura_start;
} Motore;

/**
 * @brief Tiene traccia del tempo in cui il motore e' acceso/spento in
 *        modo continuativo, e degli istanti dell'ultima transizione.
 *
 * Tenuta separata da Motore per non mescolare stato fisico (velocita',
 * temperatura) e temporizzazione: il valore di time_on/time_off si
 * azzera ad ogni transizione ON<->OFF (vedi Motortime_update), mentre
 * lo stato acceso/spento (attuale e precedente) resta nella struct
 * Motore.
 */
typedef struct {
    int time_on;
    int time_off;
    int time_start;
    int time_stop;
} MotorTime;

/** @brief Errore del motore (es. surriscaldamento), gestito a parte
 *  dallo stato normale acceso/spento. */
typedef struct {
    char error_message[100];
    bool error_status;
    int  error;
} MotorError;

/* Tutte le funzioni che modificano stato ritornano OP_SUCCESS (vedi
 * errors.h, vale 1) in caso di successo o un codice ERR_* in caso di
 * puntatore NULL. Le funzioni "getter" che restituiscono bool non
 * possono restituire un codice di errore distinguibile: su puntatore
 * NULL restituiscono false. */

/**
 * @brief Inizializza un motore spento, a velocita' e temperatura di riposo.
 * @param m Puntatore al motore.
 * @param ID Identificativo del motore.
 * @param velocita_target Velocita' target da raggiungere quando acceso.
 * @param accelerazione_desiderata Incremento di velocita' per passo di accensione.
 * @return OP_SUCCESS se inizializzato, un codice ERR_* altrimenti.
 */
int  motore_init(Motore *m, const char *ID, int velocita_target, int accelerazione_desiderata);

/**
 * @brief Avanza lo stato del motore di un passo di simulazione:
 *        aggiorna accensione/spegnimento, i tempi in MotorTime, la
 *        rampa di velocita' e la temperatura.
 *
 * Va chiamata una volta per passo (time_globale e' il tempo assoluto
 * di simulazione corrente, non un delta).
 * @param m Puntatore al motore.
 * @param mt Puntatore al timer del motore.
 * @param stato Stato richiesto per questo passo (ON/OFF).
 * @param time_globale Step di simulazione corrente.
 * @return OP_SUCCESS se aggiornato, un codice ERR_* altrimenti.
 */
int  motore_update(Motore *m, MotorTime *mt, MotorState stato, int time_globale);

/**
 * @brief Aggiorna time_on/time_off/time_start/time_stop in base alla
 *        transizione di stato (status rispetto a status_precedente).
 * @param mt Puntatore al timer del motore.
 * @param time_globale Step di simulazione corrente.
 * @param status Stato corrente del motore.
 * @param status_precedente Stato del motore prima di questo passo.
 * @return OP_SUCCESS se aggiornato, un codice ERR_* altrimenti.
 */
int  Motortime_update(MotorTime *mt, int time_globale, MotorState status, MotorState status_precedente);

/**
 * @brief Legge un componente del timer del motore.
 * @param mt Puntatore al timer del motore.
 * @param x 1 = time_on, 2 = time_off, 3 = time_on + time_off.
 * @return Il valore richiesto, oppure 0 se mt e' NULL o x non e' 1/2/3.
 */
int  get_MotorTime(const MotorTime *mt, int x);

/**
 * @brief Fa avanzare velocita_attuale verso velocita_target in base al
 *        tempo di accensione corrente (mt->time_on). Non tocca
 *        velocita_target: per quello vedi motore_imposta_velocita_target().
 * @param m Puntatore al motore.
 * @param mt Puntatore al timer del motore.
 * @return OP_SUCCESS se aggiornato, un codice ERR_* altrimenti.
 */
int  motore_aggiorna_velocita(Motore *m, const MotorTime *mt);

/**
 * @brief Imposta una nuova velocita' target (es. su comando del controllore).
 * @param m Puntatore al motore.
 * @param velocita_target Nuova velocita' target (>= 0).
 * @return OP_SUCCESS se impostata, ERR_OUT_OF_RANGE se negativa, un
 *         altro codice ERR_* altrimenti.
 */
int  motore_imposta_velocita_target(Motore *m, int velocita_target);

/**
 * @brief Controlla se il motore e' acceso.
 * @param m Puntatore al motore.
 * @return true se acceso, false se spento (anche se m e' NULL).
 */
bool motore_get_status(const Motore *m);

/**
 * @brief Velocita' attuale del motore.
 * @param m Puntatore al motore.
 * @return Velocita' attuale, oppure ERR_NULL_PTR se m e' NULL.
 */
int  motore_get_velocita(const Motore *m);

/**
 * @brief Temperatura attuale del motore.
 * @param m Puntatore al motore.
 * @return Temperatura attuale, oppure ERR_NULL_PTR se m e' NULL.
 */
int  motore_get_temperatura(const Motore *m);

/**
 * @brief Aggiorna la temperatura del motore in base al tempo trascorso
 *        acceso/spento (+5 gradi/passo acceso, -2 gradi/passo spento,
 *        mai sotto la temperatura ambiente).
 * @param m Puntatore al motore.
 * @param mt Puntatore al timer del motore.
 * @param status_precedente Stato del motore PRIMA dell'update corrente
 *        (serve per capire se c'e' stata una transizione ON<->OFF).
 * @return OP_SUCCESS se aggiornata, un codice ERR_* altrimenti.
 */
int  motore_set_temperatura(Motore *m, const MotorTime *mt, MotorState status_precedente);

/**
 * @brief Inizializza una struct di errore motore, senza nessun errore attivo.
 * @param me Puntatore alla struct di errore.
 * @return OP_SUCCESS se inizializzata, ERR_NULL_PTR se me e' NULL.
 */
int  error_motor_init(MotorError *me);

/**
 * @brief Segnala un errore sul motore, con un messaggio descrittivo
 *        associato al codice (1 = surriscaldamento, 2 = non risponde,
 *        altro = errore sconosciuto).
 * @param me Puntatore alla struct di errore.
 * @param error Codice di errore.
 * @return OP_SUCCESS se registrato, ERR_NULL_PTR se me e' NULL.
 */
int  error_motor_set(MotorError *me, int error);

#endif
