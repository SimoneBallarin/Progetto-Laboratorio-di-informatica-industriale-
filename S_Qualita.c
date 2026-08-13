#include <stdlib.h>
#include <string.h>
#include "S_Qualita.h"
#include "errors.h"

int sensore_qualita_init(SensoreQualita *s, const char *ID,
                          MalfunzionamentoSensore *m,
                          bool malfunzionamento_abilitato,
                          const int target[3])
{
    int i;

    if (s == NULL || m == NULL || ID == NULL || target == NULL) return ERR_NULL_PTR;
    if (strlen(ID) > sizeof(s->ID) - 1) return ERR_ID_INVALID;

    strncpy(s->ID, ID, sizeof(s->ID) - 1);
    s->ID[sizeof(s->ID) - 1] = '\0';

    s->status = QUALITA_OK;
    s->risultato_ultima_lettura = -1;
    s->letture_totali = 0;
    s->anomalie_rilevate = 0;
    for (i = 0; i < 3; i++) {
        s->target[i] = target[i];
        s->type_letture_totali[i] = 0;
    }

    m->last_lettura = 0;
    m->time_since_last_change = 0;
    m->is_malfunzionante = false;
    m->malfunzionamento_abilitato = malfunzionamento_abilitato;
    /* valori di default, modificabili con sensore_qualita_imposta_guasto */
    m->time_error = 1000;
    m->time_ok = 1000;

    return 0;
}

int sensore_qualita_imposta_guasto(MalfunzionamentoSensore *m, int time_error, int time_ok)
{
    if (m == NULL) return ERR_NULL_PTR;
    if (time_error <= 0 || time_ok <= 0) return ERR_OUT_OF_RANGE;

    m->time_error = time_error;
    m->time_ok = time_ok;
    return 0;
}

int update_status(SensoreQualita *s, MalfunzionamentoSensore *m, int time_current)
{
    if (s == NULL || m == NULL) return ERR_NULL_PTR;

    if (!m->malfunzionamento_abilitato) {
        s->status = QUALITA_OK;
        m->is_malfunzionante = false;
        return 0;
    }

    if (s->status == QUALITA_OK &&
        (time_current - m->time_since_last_change) >= m->time_error) {
        s->status = QUALITA_NOT_OK;
        m->is_malfunzionante = true;
        m->time_since_last_change = time_current;
    } else if (s->status == QUALITA_NOT_OK &&
               (time_current - m->time_since_last_change) >= m->time_ok) {
        s->status = QUALITA_OK;
        m->is_malfunzionante = false;
        m->time_since_last_change = time_current;
    }

    return 0;
}

int get_status_qualita(const SensoreQualita *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->status;
}

int get_Material(int object)
{
    /* Nella versione originale mancava un return di default: se
     * 'object' non era 0 ne' 1, la funzione non restituiva nulla
     * (comportamento indefinito). Qui il caso non riconosciuto ricade
     * sul materiale 2. */
    if (object == 0) return 0;
    if (object == 1) return 1;
    return 2;
}

int get_qualita(SensoreQualita *s, MalfunzionamentoSensore *m,
                int time_current, int object, bool oggetto_presente)
{
    int materiale;
    int target;
    int percentuale;

    if (s == NULL || m == NULL) return ERR_NULL_PTR;
    if (!oggetto_presente) return ERR_NOT_SUPPORTED;

    update_status(s, m, time_current);

    if (s->status != QUALITA_OK) {
        /* Sensore in malfunzionamento: la lettura e' inaffidabile per
         * definizione, quindi non ha senso calcolarla dal target. La
         * generiamo casualmente, ma - a differenza della versione
         * originale - la contiamo anche come anomalia, cosi' il
         * controllore/log puo' sapere quante letture sono avvenute
         * durante un guasto (metrica richiesta in sez. 2.1). */
        s->risultato_ultima_lettura = rand() % 3;
        s->type_letture_totali[s->risultato_ultima_lettura]++;
        s->letture_totali++;
        s->anomalie_rilevate++;
        m->last_lettura = s->letture_totali;
        return s->risultato_ultima_lettura;
    }

    materiale = get_Material(object);
    if (materiale < 0 || materiale >= 3) return ERR_NOT_SUPPORTED;

    target = s->target[materiale];
    if (target == 0) return ERR_NOT_SUPPORTED; /* target non configurato per questo materiale */

    percentuale = abs(object - target) * 100 / target;

    if (percentuale <= 5) {
        s->risultato_ultima_lettura = CONFORME;
    } else if (percentuale <= 10) {
        s->risultato_ultima_lettura = RIVALUTAZIONE;
    } else {
        s->risultato_ultima_lettura = SCARTO;
    }

    s->type_letture_totali[s->risultato_ultima_lettura]++;
    s->letture_totali++;
    m->last_lettura = s->letture_totali;

    return s->risultato_ultima_lettura;
}

void get_type_letture_totali(const SensoreQualita *s, long type_letture_totali[3])
{
    int i;
    if (s == NULL || type_letture_totali == NULL) return;
    for (i = 0; i < 3; i++) {
        type_letture_totali[i] = s->type_letture_totali[i];
    }
}

long get_letture_totali_qualita(const SensoreQualita *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->letture_totali;
}

long get_anomalie_rilevate(const SensoreQualita *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->anomalie_rilevate;
}
