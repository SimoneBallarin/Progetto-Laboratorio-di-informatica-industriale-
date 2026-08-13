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

    return 0;
}

int deviatoretime_init(DeviatoreTime *dt, int tempo_minimo_tra_commutazioni)
{
    if (dt == NULL) return ERR_NULL_PTR;
    if (tempo_minimo_tra_commutazioni < 0) return ERR_OUT_OF_RANGE;

    dt->time_last_commutazione = 0;
    dt->tempo_minimo_tra_commutazioni = tempo_minimo_tra_commutazioni;
    dt->prima_commutazione_fatta = false;

    return 0;
}

int set_deviatore(Deviatore *d, StatoDeviatore stato)
{
    if (d == NULL) return ERR_NULL_PTR;

    d->status_precedente = d->status;
    d->status = stato;

    return 0;
}

int deviatore_imposta_target(Deviatore *d, DeviatoreTime *dt,
                              int nuova_posizione_target, int time_globale)
{
    if (d == NULL || dt == NULL) return ERR_NULL_PTR;

    if (nuova_posizione_target == d->posizione_target) {
        return 0; /* nessuna commutazione richiesta */
    }

    if (dt->prima_commutazione_fatta &&
        (time_globale - dt->time_last_commutazione) < dt->tempo_minimo_tra_commutazioni) {
        return ERR_NOT_SUPPORTED; /* vincolo attuatore: commutazione troppo ravvicinata */
    }

    d->posizione_target = nuova_posizione_target;
    d->inPosizione = POSIZIONE_NOT_OK;
    dt->time_last_commutazione = time_globale;
    dt->prima_commutazione_fatta = true;

    return 0;
}

int posizionamento_deviatore(Deviatore *d)
{
    if (d == NULL) return ERR_NULL_PTR;

    if (d->status != DEVIATORE_ON) {
        return 0; /* deviatore spento: nessun movimento */
    }

    /* Bug originale: 'target' non era mai dichiarato (doveva essere
     * d->posizione_target) - non compilava. */
    if (d->posizione_attuale < d->posizione_target) {
        /* Bug originale: 'd->posizione_attuale = d->posizione_attuale++;'
         * e' comportamento indefinito in C (stessa variabile modificata
         * e usata nella stessa espressione, senza punto di sequenza). */
        d->posizione_attuale++;
        d->inPosizione = POSIZIONE_NOT_OK;
    } else if (d->posizione_attuale > d->posizione_target) {
        d->posizione_attuale--;
        d->inPosizione = POSIZIONE_NOT_OK;
    }

    /* Bug originale: 'if (d->posizione_attuale = target)' usava '='
     * (assegnazione) invece di '==' (confronto): assegnava sempre il
     * target alla posizione attuale, saltando il movimento graduale e
     * segnando sempre "in posizione" a prescindere dallo stato reale. */
    if (d->posizione_attuale == d->posizione_target) {
        d->inPosizione = POSIZIONE_OK;
    }

    return 0;
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
