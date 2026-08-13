#ifndef MOTORE_H
#define MOTORE_H

#include <stdbool.h>

/* Rinominati da 'ON'/'OFF' generici (che collidevano con l'enum omonimo
 * in Deviatore.h) a 'MOTORE_ON'/'MOTORE_OFF'. */
typedef enum {
    MOTORE_ON  = 1,
    MOTORE_OFF = 0
} MotorState;

typedef enum {
    MOTORE_OK     = 1,
    MOTORE_NOT_OK = 0
} MotoreStatus;

typedef struct { /* struttura per rappresentare un motore */
    char ID[20];
    int  velocita_target;
    int  velocita_attuale;
    int  acc;                  /* accelerazione del motore (unita' di velocita' al secondo) */
    MotorState status;         /* stato del motore, acceso o spento */
    MotorState status_precedente;

    int temperatura_motore;
    int temperatura_start;
} Motore;

/* Struttura per tenere traccia del tempo in cui il motore e' stato
 * acceso o spento. NB: qui vive SOLO la parte temporale; lo stato
 * (acceso/spento, attuale e precedente) vive nella struct Motore, per
 * evitare di dover mantenere due copie sincronizzate dello stesso dato
 * (nella versione originale il codice usava mt->status e
 * mt->status_precedente, campi che pero' non esistevano in questa
 * struct: non compilava). */
typedef struct {
    int time_on;
    int time_off;
    int time_start;
    int time_stop;
} MotorTime;

typedef struct { /* struttura per tenere traccia degli errori del motore */
    char error_message[100];
    bool error_status;
    int  error;
} MotorError;

/* Tutte le funzioni che modificano stato ritornano 0 in caso di successo
 * o un codice ERR_* (vedi errors.h) in caso di puntatore NULL. Le
 * funzioni "getter" che restituiscono bool non possono restituire un
 * codice di errore distinguibile: su puntatore NULL restituiscono false. */

int  motore_init(Motore *m, const char *ID, int velocita_target);

/* Avanza lo stato del motore di un passo di simulazione: aggiorna
 * accensione/spegnimento, i tempi in MotorTime, la rampa di velocita'
 * e la temperatura. Va chiamata una volta per passo (time_globale e'
 * il tempo assoluto di simulazione corrente, non un delta). */
int  motore_update(Motore *m, MotorTime *mt, MotorState stato, int time_globale);

/* Aggiorna time_on/time_off/time_start/time_stop in base alla
 * transizione di stato (status rispetto a status_precedente). */
int  Motortime_update(MotorTime *mt, int time_globale, MotorState status, MotorState status_precedente);

/* x=1 -> time_on, x=2 -> time_off, x=3 -> time_on+time_off. Ritorna 0
 * anche per x non valido (comportamento invariato rispetto all'originale). */
int  get_MotorTime(const MotorTime *mt, int x);

/* Fa avanzare velocita_attuale verso velocita_target in base al tempo
 * di accensione corrente (mt->time_on). Non tocca velocita_target: per
 * quello vedi motore_imposta_velocita_target(). */
int  motore_aggiorna_velocita(Motore *m, const MotorTime *mt);

/* Imposta una nuova velocita' target (es. su comando del controllore).
 * Rinominata da 'motor_set_velocita' per non confondersi con
 * motore_aggiorna_velocita: nella versione originale le due funzioni
 * avevano lo stesso nome ma firme diverse (ridefinizione, non compila). */
int  motore_imposta_velocita_target(Motore *m, int velocita_target);

bool motore_get_status(const Motore *m);
int  motore_get_velocita(const Motore *m);
int  motore_get_temperatura(const Motore *m);

/* status_precedente e' lo stato del motore PRIMA dell'update corrente
 * (serve per capire se c'e' stata una transizione ON<->OFF). */
int  motore_set_temperatura(Motore *m, const MotorTime *mt, MotorState status_precedente);

int  error_motor_init(MotorError *me);
int  error_motor_set(MotorError *me, int error);

#endif
