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
    s->A = 0;
    s->B = 0;
    s->non_classificato = 0;
    for(int i =0 ; i<3; i++){s->type_letture_totali[i] = 0;}
    
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

char get_Material(object_t *object, SensoreQualita *s)
{
    if (s == NULL || object == NULL) return ERR_NULL_PTR;

    float densita[2] = {8.96 , 7.85};
    float ConfrontoA = (float)(((object -> dimensionX)*(object -> raggio)*(object -> raggio))*3.14 * densita[0]);
    float ConfrontoB = (float)(((object -> dimensionX)*(object -> raggio)*(object -> raggio))*3.14 * densita[1]);
    int p=12;

    /* Bug corretto: la versione precedente calcolava "materiale"/"e"
     * UNA SOLA VOLTA in base a object->type, poi li riusava per
     * confrontare ANCHE ConfrontoB (calcolato con la densita' opposta) -
     * un oggetto dichiarato 'A' con dimensioni fuori dalla tolleranza
     * "A" poteva quindi finire classificato 'B' per puro caso numerico,
     * ed essere instradato nel buffer del materiale OPPOSTO a quello
     * dichiarato (vedi README). Non basta ricalcolare "materiale"/"e"
     * separatamente per A e per B: essendo la tolleranza una PERCENTUALE
     * della massa di riferimento, la densita' si semplifica sempre
     * nel confronto (materiale_X ± e_X, diviso per densita[X], da'
     * sempre lo stesso intervallo [volume_target*0.88, volume_target*1.12]
     * indipendentemente da quale densita' X si usi) - un vero confronto
     * incrociato "con densita' corretta" darebbe quindi SEMPRE lo stesso
     * esito del confronto primario, non avrebbe alcun valore
     * discriminante. L'unico confronto che ha senso e' quindi UNO SOLO,
     * quello coerente con il tipo dichiarato: se le dimensioni non sono
     * abbastanza vicine al target per quel tipo, il pezzo e'
     * "non_classificato" (non un secondo tentativo con l'altro
     * materiale). */
    if (object -> type == 'A') {
        float materialeA = (float)(((s -> dimensionX_target)*(s -> raggio_target)*(s -> raggio_target))*3.14 * densita[0]);
        float eA = materialeA*p/100;
        if ((materialeA - eA) <= ConfrontoA && ConfrontoA <= (materialeA + eA)) { s->A++; return 'A'; }
        s->non_classificato++;
        return 0;
    }
    if (object -> type == 'B') {
        float materialeB = (float)(((s -> dimensionX_target)*(s -> raggio_target)*(s -> raggio_target))*3.14 * densita[1]);
        float eB = materialeB*p/100;
        if ((materialeB - eB) <= ConfrontoB && ConfrontoB <= (materialeB + eB)) { s->B++; return 'B'; }
        s->non_classificato++;
        return 0;
    }

    /* Tipo sconosciuto (ne' 'A' ne' 'B'): non classificabile per
     * costruzione, nessun confronto da fare (vedi anche il commento nel
     * .h su questo caso, aggiunto insieme al contatore non_classificato). */
    s->non_classificato++;
    return 0;
}

int get_qualita(SensoreQualita *s, MalfunzionamentoSensore *m,
                int time_current, object_t *object, bool oggetto_presente)
{
    
    int percentualeX;
    int percentualeR;

    if (s == NULL || m == NULL || object == NULL) return ERR_NULL_PTR;
    if (!oggetto_presente) return ERR_NOT_SUPPORTED;

    update_status(s, m, time_current);

    /* Questo ramo si attiva SOLO se get_qualita viene chiamata mentre
     * s->status e' QUALITA_NOT_OK (cioe' il sensore E' in guasto in
     * questo preciso istante). Con l'architettura attuale del progetto
     * questo non succede mai in pratica: Controllore.c (processISP)
     * controlla is_malfunzionante PRIMA di chiamare get_qualita, e se e'
     * vero esce subito senza mai raggiungere questa funzione - vedi
     * README, sezione "Guasto sensore qualita' = stazione
     * indisponibile" (decisione del gruppo: durante il guasto la
     * stazione TRATTIENE il pezzo invece di produrre una lettura
     * incerta). Questo ramo resterebbe pero' l'unico a proteggere la
     * correttezza se in futuro qualcuno chiamasse get_qualita
     * direttamente durante un guasto, bypassando quel controllo a monte
     * (es. un nuovo punto di chiamata, o un test che esercita questa
     * funzione in isolamento) - senza, il codice sotto leggerebbe le
     * dimensioni dell'oggetto come se il sensore funzionasse
     * normalmente, producendo una lettura silenziosamente inaffidabile
     * invece di segnalare esplicitamente l'incertezza. */
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

    /* Bug corretto: il cast (int) si applicava SOLO a fabs(...), non
     * all'intera espressione - per precedenza degli operatori era
     * equivalente a ((int)fabs(diff)) * 100 / target, che tronca la
     * differenza a intero PRIMA di scalarla in percentuale. Con target
     * piccoli (es. raggio_target=6 su ISP2) uno scostamento reale di
     * 0.9 diventava (int)0.9=0, quindi percentuale=0 invece del ~15%
     * reale, falsando la classificazione CONFORME/RIVALUTAZIONE/SCARTO.
     * Ora il troncamento a intero avviene UNA SOLA VOLTA, alla fine, sul
     * risultato gia' scalato in percentuale (100.0 forza la divisione
     * in virgola mobile). */
    percentualeX = (int)( fabs(object->dimensionX - s->dimensionX_target) * 100.0 / s->dimensionX_target );
    percentualeR = (int)( fabs(object->raggio - s->raggio_target) * 100.0 / s->raggio_target );
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

long get_ConteggioMaterialeA(const SensoreQualita *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->A;
}

long get_ConteggioMaterialeB(const SensoreQualita *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->B;
}

long get_ConteggioNonClassificato(const SensoreQualita *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->non_classificato;
}


