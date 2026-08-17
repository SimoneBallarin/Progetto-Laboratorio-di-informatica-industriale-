#include <stdlib.h>
#include <string.h>
#include "S_Qualita.h"
#include "errors.h"
#include "object.h"
#include <math.h>

int sensore_qualita_init(SensoreQualita *s, const char *ID,
                          MalfunzionamentoSensore *m,
                          bool malfunzionamento_abilitato,
                          const int dimensionX_target,
                          const int raggio_target )
{
    if (s == NULL || m == NULL || ID == NULL ) return ERR_NULL_PTR;
    if (strlen(ID) > sizeof(s->ID) - 1) return ERR_ID_INVALID;

    strncpy(s->ID, ID, sizeof(s->ID) - 1);
    s->ID[sizeof(s->ID) - 1] = '\0';

    s->status = QUALITA_OK;
    s->risultato_ultima_lettura = -1;
    s->letture_totali = 0;
    s->anomalie_rilevate = 0;
    s->dimensionX_target = dimensionX_target;
    s->raggio_target = raggio_target;


    m->last_lettura = 0;
    m->time_since_last_change = 0;
    m->is_malfunzionante = false;
    m->malfunzionamento_abilitato = malfunzionamento_abilitato;
    /* valori di default, modificabili con sensore_qualita_imposta_guasto */
    m->time_error = 1000;
    m->time_ok = 1000;

    return OP_SUCCESS;
}

int sensore_qualita_imposta_guasto(MalfunzionamentoSensore *m, int time_error, int time_ok)
{
    if (m == NULL) return ERR_NULL_PTR;
    if (time_error <= 0 || time_ok <= 0) return ERR_OUT_OF_RANGE;

    m->time_error = time_error;
    m->time_ok = time_ok;
    return OP_SUCCESS;
}

int update_status(SensoreQualita *s, MalfunzionamentoSensore *m, int time_current)
{
    if (s == NULL || m == NULL) return ERR_NULL_PTR;

    if (!m->malfunzionamento_abilitato) {
        s->status = QUALITA_OK;
        m->is_malfunzionante = false;
        return OP_SUCCESS;
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

    return OP_SUCCESS;
}

int get_status_qualita(const SensoreQualita *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->status;
}

char get_Material(object_t *object, const SensoreQualita *s)
{
    if (s == NULL || object == NULL) return ERR_NULL_PTR;
    
    float densita[2] = {8.96 , 7.85};
    float ConfrontoA = (float)(((object -> dimensionX)*(object -> raggio)*(object -> raggio))*3.14 * densita[0]);
    float ConfrontoB = (float)(((object -> dimensionX)*(object -> raggio)*(object -> raggio))*3.14 * densita[1]);
    float materiale;
    float e;
    int p=5;
    if (object -> type == 'A') {materiale = (float)(((s -> dimensionX_target)*(s -> raggio_target)*(s -> raggio_target))*3.14 * densita[0]); e = 100*p/materiale;}
    if (object -> type == 'B') {materiale = (float)(((s -> dimensionX_target)*(s -> raggio_target)*(s -> raggio_target))*3.14 * densita[1]); e = 100*p/materiale;}
    if((materiale - e)<= ConfrontoA && ConfrontoA <= (materiale + e)){ return 'A';}
    else if((materiale - e)<= ConfrontoB && ConfrontoB <= (materiale + e)){return 'B';}
    else {return 0;}
}

int get_qualita(SensoreQualita *s, MalfunzionamentoSensore *m,
                int time_current, object_t *object, bool oggetto_presente)
{
    
    int percentualeX;
    int percentualeR;

    if (s == NULL || m == NULL || object == NULL) return ERR_NULL_PTR;
    if (!oggetto_presente) return ERR_NOT_SUPPORTED;

    update_status(s, m, time_current);

    if (s->status != QUALITA_OK) {
        s->risultato_ultima_lettura = RIVALUTAZIONE;
        s->type_letture_totali[s->risultato_ultima_lettura]++;
        s->letture_totali++;
        s->anomalie_rilevate++;
        m->last_lettura = s->letture_totali;
        return s->risultato_ultima_lettura;
    }

if(s->status == QUALITA_OK){
    if (s->dimensionX_target == 0 || s->raggio_target == 0 ) {return ERR_NOT_SUPPORTED;} /* target non configurato per questo materiale */

    percentualeX = fabs(object->dimensionX - s->dimensionX_target) * 100 /s->dimensionX_target;
    percentualeR = fabs(object->raggio - s->raggio_target) * 100 /s->raggio_target;
    if (percentualeX <= 5 && percentualeR <= 5  ) {
        s->risultato_ultima_lettura = CONFORME;
    } 
     else if (percentualeX <= 10 && percentualeR <= 10 ) {
        s->risultato_ultima_lettura = RIVALUTAZIONE;
    } 
    else if (percentualeX <= 100 && percentualeR <= 100){
        s->risultato_ultima_lettura = SCARTO;
    }else{return ERR_NOT_SUPPORTED;}

    s->type_letture_totali[s->risultato_ultima_lettura]++;
    s->letture_totali++;
    m->last_lettura = s->letture_totali;

    return s->risultato_ultima_lettura;
}

return ERR_NOT_SUPPORTED;
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
