#include <stdio.h>
#include <string.h>
#include "Motore.h"
#include "errors.h"

int motore_init(Motore *m, const char *ID, int velocita_target , int accelerazione_desiderata)
{
    if (m == NULL || ID == NULL) return ERR_NULL_PTR;
    if (strlen(ID) > sizeof(m->ID) - 1) return ERR_ID_INVALID;

    strncpy(m->ID, ID, sizeof(m->ID) - 1);
    m->ID[sizeof(m->ID) - 1] = '\0';

    m->velocita_target = velocita_target;
    m->velocita_attuale = 0;
    m->status = MOTORE_OFF;
    m->status_precedente = MOTORE_OFF;
    m->acc = accelerazione_desiderata; /* accelerazione del motore */

    /* la versione originale inizializzava solo temperatura_start,
     * lasciando temperatura_motore non inizializzata */
    m->temperatura_motore = 25;
    m->temperatura_start = 25;

    return 0;
}

int Motortime_update(MotorTime *mt, int time_globale, MotorState status, MotorState status_precedente)
{
    if (mt == NULL) return ERR_NULL_PTR;

    /* time_start/time_stop vengono aggiornati SOLO sul fronte di
     * transizione, non ad ogni chiamata: era questo il bug che
     * impediva al motore di accelerare (vedi motore_update). */
    if (status == MOTORE_ON && status_precedente == MOTORE_OFF) {
        mt->time_start = time_globale;
    } else if (status == MOTORE_OFF && status_precedente == MOTORE_ON) {
        mt->time_stop = time_globale;
    }

    if (status == MOTORE_ON) {
        mt->time_on = time_globale - mt->time_start;
        mt->time_off = 0;
    } else {
        mt->time_off = time_globale - mt->time_stop;
        mt->time_on = 0;
    }

    return 0;
}

int motore_aggiorna_velocita(Motore *m, const MotorTime *mt)
{
    if (m == NULL || mt == NULL) return ERR_NULL_PTR;
    if (m->status != MOTORE_ON) return 0; /* a motore spento la velocita' non si aggiorna qui */

    if (m->velocita_attuale < m->velocita_target) {
        m->velocita_attuale = mt->time_on * m->acc; /* ogni secondo la velocita' aumenta di 'acc' */
    }
    if (m->velocita_attuale >= m->velocita_target) {
        m->velocita_attuale = m->velocita_target; /* la velocita' non puo' superare il target */
    }

    return 0;
}

int motore_imposta_velocita_target(Motore *m, int velocita_target)
{
    if (m == NULL) return ERR_NULL_PTR;
    if (velocita_target < 0) return ERR_OUT_OF_RANGE;

    m->velocita_target = velocita_target;
    return 0;
}

int motore_set_temperatura(Motore *m, const MotorTime *mt, MotorState status_precedente)
{
    if (m == NULL || mt == NULL) return ERR_NULL_PTR;

    /* alla transizione di stato, il punto di partenza per il nuovo
     * calcolo e' la temperatura attuale (nella versione originale i due
     * rami if facevano gia' la stessa identica assegnazione: unificati
     * in un solo controllo sul cambio di stato) */
    if (m->status != status_precedente) {
        m->temperatura_start = m->temperatura_motore;
    }

    if (m->status == MOTORE_ON) {
        m->temperatura_motore = m->temperatura_start + mt->time_on * 5; /* +5 gradi al secondo acceso */
    } else {
        m->temperatura_motore = m->temperatura_start - mt->time_off * 2; /* -2 gradi al secondo spento */
    }

    if (m->temperatura_motore < 25) {
        m->temperatura_motore = 25; /* la temperatura non scende sotto l'ambiente */
    }

    return 0;
}

int motore_update(Motore *m, MotorTime *mt, MotorState stato, int time_globale)
{
    MotorState status_precedente;

    if (m == NULL || mt == NULL) return ERR_NULL_PTR;

    status_precedente = m->status; /* stato PRIMA di questo passo */
    m->status = stato;

    Motortime_update(mt, time_globale, m->status, status_precedente);

    if (m->status == MOTORE_ON) {
        motore_aggiorna_velocita(m, mt);
    } else {
        m->velocita_attuale = 0; /* a motore spento la velocita' e' sempre 0 */
    }

    motore_set_temperatura(m, mt, status_precedente);

    m->status_precedente = status_precedente;

    return 0;
}

int get_MotorTime(const MotorTime *mt, int x)
{
    if (mt == NULL) return ERR_NULL_PTR;

    if (x == 1) {
        return mt->time_on;
    }
    if (x == 2) {
        return mt->time_off;
    }
    if (x == 3) {
        return mt->time_on + mt->time_off;
    }
    return 0; /* x non riconosciuto: comportamento invariato rispetto all'originale */
}

bool motore_get_status(const Motore *m)
{
    if (m == NULL) return false; /* un bool non puo' distinguere un errore da OFF */
    return m->status == MOTORE_ON;
}

int motore_get_velocita(const Motore *m)
{
    if (m == NULL) return ERR_NULL_PTR;
    return m->velocita_attuale;
}

int motore_get_temperatura(const Motore *m)
{
    if (m == NULL) return ERR_NULL_PTR;
    return m->temperatura_motore;
}

int error_motor_init(MotorError *me)
{
    if (me == NULL) return ERR_NULL_PTR;

    me->error_status = false;
    me->error = 0;
    snprintf(me->error_message, sizeof(me->error_message), "Nessun errore");

    return 0;
}

int error_motor_set(MotorError *me, int error)
{
    if (me == NULL) return ERR_NULL_PTR;

    me->error = error;
    me->error_status = true;

    switch (error) {
        case 1:
            snprintf(me->error_message, sizeof(me->error_message), "Errore: Motore surriscaldato");
            break;
        case 2:
            snprintf(me->error_message, sizeof(me->error_message), "Errore: Motore non risponde");
            break;
        default:
            snprintf(me->error_message, sizeof(me->error_message), "Errore sconosciuto");
            break;
    }

    return 0;
}
