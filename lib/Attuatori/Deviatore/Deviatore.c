/**
 * @file Deviatore.c
 * @brief Implementazione dell'attuatore deviatore.
 */
#include <string.h>
#include "Deviatore.h"
#include "errors.h"

int deviatore_init(Deviatore *d, const char *ID)
{
    if (d == NULL || ID == NULL) return ERR_NULL_PTR;
    if (strlen(ID) > sizeof(d->ID) - 1) return ERR_ID_INVALID;

    strncpy(d->ID, ID, sizeof(d->ID) - 1);
    d->ID[sizeof(d->ID) - 1] = '\0';

    d->posizione_target = 0;
    d->posizione_attuale = 0;
    d->status = DEVIATORE_OFF;
    d->status_precedente = DEVIATORE_OFF;
    d->inPosizione = POSIZIONE_OK;

    return 1;
}

int deviatoretime_init(DeviatoreTime *dt, int tempo_minimo_tra_commutazioni)
{
    if (dt == NULL) return ERR_NULL_PTR;
    if (tempo_minimo_tra_commutazioni < 0) return ERR_OUT_OF_RANGE;

    dt->time_last_commutazione = 0;
    dt->tempo_minimo_tra_commutazioni = tempo_minimo_tra_commutazioni;
    dt->prima_commutazione_fatta = false;

    return 1;
}

int set_deviatore(Deviatore *d, StatoDeviatore stato)
{
    if (d == NULL) return ERR_NULL_PTR;

    d->status_precedente = d->status;
    d->status = stato;

    return 1;
}

int deviatore_imposta_target(Deviatore *d, DeviatoreTime *dt,
                              int nuova_posizione_target, int time_globale)
{
    if (d == NULL || dt == NULL) return ERR_NULL_PTR;

    if (nuova_posizione_target == d->posizione_target) {
        return 1; /* nessuna commutazione richiesta */
    }

    if (dt->prima_commutazione_fatta &&
        (time_globale - dt->time_last_commutazione) < dt->tempo_minimo_tra_commutazioni) {
        return ERR_NOT_SUPPORTED; /* vincolo attuatore: commutazione troppo ravvicinata */
    }

    d->posizione_target = nuova_posizione_target;
    d->inPosizione = POSIZIONE_NOT_OK;
    dt->time_last_commutazione = time_globale;
    dt->prima_commutazione_fatta = true;

    return 1;
}

int posizionamento_deviatore(Deviatore *d)
{
    if (d == NULL) return ERR_NULL_PTR;

    if (d->status != DEVIATORE_ON) {
        return 1; /* deviatore spento: nessun movimento */
    }


    if (d->posizione_attuale < d->posizione_target) {

        d->posizione_attuale++;
        d->inPosizione = POSIZIONE_NOT_OK;
    } else if (d->posizione_attuale > d->posizione_target) {
        d->posizione_attuale--;
        d->inPosizione = POSIZIONE_NOT_OK;
    }

    if (d->posizione_attuale == d->posizione_target) {
        d->inPosizione = POSIZIONE_OK;
    }

    return 1;
}

bool get_status_deviatore(const Deviatore *d)
{
    if (d == NULL) return false;
    return d->status == DEVIATORE_ON;
}

int get_posizione(const Deviatore *d)
{
    if (d == NULL) return ERR_NULL_PTR;
    return d->posizione_attuale;
}

bool get_inPosizione(const Deviatore *d)
{
    if (d == NULL) return false;
    return d->inPosizione == POSIZIONE_OK;
}
