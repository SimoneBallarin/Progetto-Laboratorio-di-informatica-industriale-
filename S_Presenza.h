#ifndef S_Presenza_H
#define S_Presenza_H

#include <stdbool.h>

typedef struct {
    bool status_precedente;
    bool status_corrente;
    int ID;
    long letture_totali;
    long rilevamenti_totali; /* numero di fronti di salita rilevati */
}SensorePresenza;

typedef enum {
    PRESENZA_PROLUNGATA = 1;
    ASSENZA_PROLUNGATA = 2;
    ENTRATA = 3; 
    USCITA = 4;
} StatoSensore;

void sensore_presenza_init(SensorePresenza *s, int ID);
int get_status(SensorePresenza *s, int time_on, int presenza);
int get_letture_totali(SensorePresenza *s);
int get_rilevamenti_totali(SensorePresenza *s);

#endif