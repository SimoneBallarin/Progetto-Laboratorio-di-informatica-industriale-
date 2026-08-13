#include <stdlib.h>
#include <string.h>
#include "S_Presenza.h"
#include "errors.h"

int sensore_presenza_init(SensorePresenza *s, const char *ID)
{
    if (s == NULL || ID == NULL) return ERR_NULL_PTR;
    if (strlen(ID) > sizeof(s->ID) - 1) return ERR_ID_INVALID;

    s->status_precedente = false;
    s->status_corrente = false;
    s->letture_totali = 0;
    s->rilevamenti_totali = 0;

    /* s->ID e' un array: va copiato con strncpy, non assegnato con '=' */
    strncpy(s->ID, ID, sizeof(s->ID) - 1);
    s->ID[sizeof(s->ID) - 1] = '\0';

    return 0;
}

int get_status_presenza(SensorePresenza *s, int time_on, int presenza)
{
    int presente;
    int fronte_di_salita;
    int fronte_di_discesa;
    const int tempo_atteso = 2;

    if (s == NULL) return ERR_NULL_PTR;

    s->letture_totali++;

    /* presente e' vero se il timer di persistenza ha superato la soglia
     * OPPURE se il segnale diretto di presenza e' attivo.
     * (era 'presente' non dichiarata nella versione originale: refuso
     * del parametro 'presenza') */
    presente = (time_on >= tempo_atteso) || (presenza == 1);

    fronte_di_salita  = (presente == 1 && s->status_precedente == 0);
    fronte_di_discesa = (presente == 0 && s->status_precedente == 1);

    s->status_precedente = presente;
    s->status_corrente   = presente;

    if (fronte_di_salita) {
        s->rilevamenti_totali++;
        return ENTRATA;
    }
    if (fronte_di_discesa) {
        return USCITA;
    }
    if (presente) {
        return PRESENZA_PROLUNGATA;
    }
    return ASSENZA_PROLUNGATA;
}

long get_letture_totali_presenza(const SensorePresenza *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->letture_totali;
}

long get_rilevamenti_totali_presenza(const SensorePresenza *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->rilevamenti_totali;
}
