#include <stdlib.h>
#include "S_Buffer.h"

void sensore_Buffer_init(SensoreBuffer *s, int ID, long livello_massimo)
{
    if (s == NULL) return;
    s->livello_attuale = 0;
    s->livello_massimo = livello_massimo;
    s->status = false;
    s -> ID = ID; 
}


void aggiornamento_status(SensoreBuffer *s, int new_object)
{
    if (s == NULL) return;

    if ((s->livello_attuale < s->livello_massimo) && (new_object == 1)) s->livello_attuale++;
    if ((s->livello_attuale > 0) && (new_object == -1)) s->livello_attuale--;
    if (s->livello_attuale >= s->livello_massimo) s->livello_attuale = s->livello_massimo;  s->status = FULL;
    if (s->livello_attuale <= 0) s->livello_attuale = 0;  s->status = EMPTY;

}

int get_percentuale_livello(SensoreBuffer *s)
{
    if (s == NULL) return;
    return (s->livello_attuale * 100) / s->livello_massimo;
}

int get_status(SensoreBuffer *s)
{
    if (s == NULL) return;
    return s->status;
}

int get_livello_attuale(SensoreBuffer *s)
{
    if (s == NULL) return;
    return s->livello_attuale;
}