#ifndef S_PRESENZA_H
#define S_PRESENZA_H

#include <stdbool.h>

typedef struct {
    bool status_precedente;
    bool status_corrente;
    char ID[20];
    long letture_totali;
    long rilevamenti_totali; /* numero di fronti di salita (ENTRATA) rilevati */
} SensorePresenza;

/* NB: i valori sono separati da virgola (era ';' nella versione originale,
 * che non compila in un enum). */
typedef enum {
    PRESENZA_PROLUNGATA = 1,
    ASSENZA_PROLUNGATA  = 2,
    ENTRATA             = 3,
    USCITA              = 4
} StatoSensore;

/* Ritorna 0 in caso di successo, un codice ERR_* (vedi errors.h) altrimenti.
 * ID viene copiato internamente (max 19 caratteri + terminatore). */
int  sensore_presenza_init(SensorePresenza *s, const char *ID);

/* Aggiorna lo stato del sensore e ritorna uno tra:
 *   ENTRATA, USCITA, PRESENZA_PROLUNGATA, ASSENZA_PROLUNGATA
 * oppure un codice ERR_* se s e' NULL. */
int  get_status_presenza(SensorePresenza *s, int time_on, int presenza);

long get_letture_totali_presenza(const SensorePresenza *s);
long get_rilevamenti_totali_presenza(const SensorePresenza *s);

#endif
