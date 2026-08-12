#include "S_Qualita.h"
#include <stdlib.h>


void sensore_qualita_init(SensoreQualità *s, int ID, MulfuctionSensor *m, bool enableMultiFunction) {
    s->ID = ID;
    s->status = OK;
    s->isempty = false;
    s->risult = 0;
    s->letture_totali = 0;

    for(int i = 0; i < 3; i++){
        s->type_letture_totali[i] = 0;
    }

    m->last_letterua = 0;
    m->time_since_last_MultiFunction = 0;
    m->isMulFunction = false;
    m->enabledMultiFunction = enableMultiFunction; // Assuming enableMultiFunction is defined elsewhere
    m -> time_error = 1000; // Assuming a default value for time_error, you can change it as needed
    m -> time_ok = 1000; // Assuming a default value for time_ok, you can change it as needed
}

void update_status(SensoreQualità *s, MulfuctionSensor *m, int time_current, bool isempty) {
    s->isempty = isempty;
if(m->enabledMultiFunction){  
 
  if((time_current - m->time_since_last_MultiFunction >= m-> time_error ) && s->stasus == OK){s->status = NOT_OK; m -> isMulFunction = true; m->time_since_last_MultiFunction = time_current;}
  }
  if((time_current - m->time_since_last_MultiFunction >= m->time_ok ) && s->stasus == NOT_OK){s->status = OK; m -> isMulFunction = false; }

if(!m-> enabledMultiFunction){s->status = OK; m->time_since_last_MultiFunction = time_current;}
}

int get_status_sensore(SensoreQualità *s) {
    return s->status;
}

int get_qualita(SensoreQualità *s, MulfuctionSensor *m, int time_current, int object) {
    update_status(s, m, time_current); // aggiornamento dello stato del sensore prima di valutare la qualità dell'oggetto
    if (s-> status == OK && s->isempty == true) {

        int percentuale = (object- s->target)*100/s->target; // Calculate the percentage difference between the object and the target

        if (percentuale <= 5) {
            s->risult = CONFORME;
            s->type_letture_totali[0]++;
        } else if (percentuale > 5 && percentuale <= 10) {
            s->risult = RIVALUTAZIONE;
            s->type_letture_totali[1]++;
        } else {
            s->risult = SCARTO;
            s->type_letture_totali[2]++;
        }

        s->letture_totali++;
        m->last_letterua = s->letture_totali;
        return s->risult;
    } 
    if (s-> status == NOT_OK && s->isempty == true) { // se il sensore è in malfunzionamento, restituisce un valore casuale per la qualità dell'oggetto
        return random() % 3; // Return a random quality type (0, 1, or 2)
    }
    else{return -1;} // se il sensore non ha rilevato alcun oggetto, restituisce -1
}   


void get_type_letture_totali(SensoreQualità *s, long *type_letture_totali) {
    for(int i = 0; i < 3; i++){
        type_letture_totali[i] = s->type_letture_totali[i];
    }
}

long get_letture_totali(SensoreQualità *s) {
    if (s == NULL) {return;} 
    return s->letture_totali;
}

int get_Material(int object) {
    if (object == 0) {
        return 0; // Material type 0
    } else if (object == 1) {
        return 1; // Material type 1
    } 

}