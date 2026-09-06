/**
 * @file S_Presenza.h
 * @brief Sensore di presenza: rileva il fronte di ingresso/uscita di un
 *        oggetto in un punto della cella (es. l'ingresso di un buffer),
 *        con un piccolo timer di persistenza per filtrare letture
 *        intermittenti (sez. 5.1 del progetto preliminare).
 */
#ifndef S_PRESENZA_H
#define S_PRESENZA_H
#include "object.h"
#include <stdbool.h>

/** @brief Stato interno del sensore di presenza. */
typedef struct {
    bool status_precedente;
    bool status_corrente;
    char ID[IDLENGTH];
    long letture_totali;
    long rilevamenti_totali; /**< Numero di fronti di salita (ENTRATA) rilevati. */
} SensorePresenza;

/** @brief Esito di una lettura del sensore (vedi get_status_presenza). */
typedef enum {
    PRESENZA_PROLUNGATA = 1,  /**< Presenza rilevata, gia' segnalata al passo precedente. */
    ASSENZA_PROLUNGATA  = 2,  /**< Assenza rilevata, gia' segnalata al passo precedente. */
    ENTRATA             = 3,  /**< Fronte di salita: un oggetto e' appena arrivato. */
    USCITA              = 4   /**< Fronte di discesa: un oggetto e' appena uscito. */
} StatoSensore;

/**
 * @brief Inizializza un sensore di presenza.
 * @param s Puntatore al sensore.
 * @param ID Identificativo del sensore (max 19 caratteri + terminatore),
 *        copiato internamente.
 * @return OP_SUCCESS se inizializzato, un codice ERR_* (vedi errors.h) altrimenti.
 */
int  sensore_presenza_init(SensorePresenza *s, const char *ID);

/**
 * @brief Aggiorna lo stato del sensore con la lettura corrente e
 *        restituisce l'evento rilevato.
 * @param s Puntatore al sensore.
 * @param time_on Passi consecutivi con segnale di presenza attivo (timer
 *        di persistenza).
 * @param presenza Segnale diretto di presenza (1 = presente, 0 = assente).
 * @return ENTRATA, USCITA, PRESENZA_PROLUNGATA o ASSENZA_PROLUNGATA, oppure
 *         ERR_NULL_PTR se s e' NULL.
 */
int  get_status_presenza(SensorePresenza *s, int time_on, int presenza);

/**
 * @brief Numero totale di letture effettuate dal sensore.
 * @param s Puntatore al sensore.
 * @return Numero di letture, oppure ERR_NULL_PTR se s e' NULL.
 */
long get_letture_totali_presenza(const SensorePresenza *s);

/**
 * @brief Numero di ingressi distinti rilevati (fronti di salita/ENTRATA).
 * @param s Puntatore al sensore.
 * @return Numero di rilevamenti, oppure ERR_NULL_PTR se s e' NULL.
 */
long get_rilevamenti_totali_presenza(const SensorePresenza *s);

#endif
