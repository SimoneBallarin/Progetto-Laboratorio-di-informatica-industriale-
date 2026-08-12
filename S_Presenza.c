#include <stdlib.h>
#include "S_Presenza.h"

void sensore_presenza_init(SensorePresenza *s, int ID)
{
    if (s == NULL) return;
    s->status_precedente = 0;
    s->status_corrente = 0;
    s->letture_totali = 0;
    s->rilevamenti_totali = 0;
    s -> ID = ID; 
}

int get_status(SensorePresenza *s, int time_on, int presenza)
{
    if (s == NULL) return;
    int fronte_di_salita;
    int fronte_di_discesa;
    int tempo_atteso = 2;
    s->letture_totali++;

    if((time_on >= tempo_atteso) || presenza == 1){ //presenza è un input esterno, time_on verra probabilmente tolto in futuro
        presente = 1;
    } else {
        presente = 0;
    }
    
    fronte_di_salita = (presente == 1 && s->status_precedente == 0);
    fronte_di_discesa = (presente == 0 && s->status_precedente == 1);
    
    if (fronte_di_salita) {
        s->rilevamenti_totali++;
        s -> status_corrente = 1;
        s->status_precedente = presente;
        return ENTRATA;
    }

     if (fronte_di_discesa) {
        s->status_precedente = presente;
        s -> status_corrente = 0;
        return USCITA;
    }
    
    if(presente == 1 && s->status_precedente == 1){
        s->status_precedente = presente;
        s -> status_corrente = 1;
        return PRESENZA_PROLUNGATA;
    }

    if(presente == 0 && s->status_precedente == 0){
        s->status_precedente = presente;
        s -> status_corrente = 0;
        return ASSENZA_PROLUNGATA;
    }
   
}

int get_letture_totali(SensorePresenza *s)
{
    if (s == NULL) return;
    return s->letture_totali;
}

int get_rilevamenti_totali(SensorePresenza *s)
{
    if (s == NULL) return;
    return s->rilevamenti_totali;
}