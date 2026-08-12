#ifndef S_Buffer_H
#define S_Buffer_H

#include <stdbool.h>

typedef struct {
    int ID;
    long livello_attuale;
    long livello_massimo;
    bool status;
}SensoreBuffer;

typdef enum {
    EMPTY = 0,
    FULL = 1
} BufferStatus;


void sensore_Buffer_init(SensoreBuffer *s, int ID, long livello_massimo);
void aggiornamento_status(SensoreBuffer *s, int new_object);
int get_status(SensoreBuffer *s);
int get_livello_attuale(SensoreBuffer *s);
int get_percentuale_livello(SensoreBuffer *s);

#endif