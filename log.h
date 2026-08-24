/**
 * @file log.h
 * @brief Modulo "log": registra su file (ed eventualmente anche su
 *        stdout) gli eventi rilevanti della simulazione - sez. 2.2 del
 *        progetto preliminare ("log degli eventi rilevanti" aggiornato
 *        ad ogni passo) e sez. 7 (errori di parsing "riportati nel
 *        log").
 *
 * Stesso schema del resto del progetto: creazione, uso tramite
 * puntatore opaco, distruzione (vedi statistiche.h/Controllore.h). Non
 * sostituisce le funzioni *_print / *_stampa gia' presenti (che mostrano
 * lo stato CORRENTE di cella/statistiche): il log e' invece uno
 * storico cronologico di eventi (un arrivo, un blocco, un'anomalia del
 * sensore di qualita', un errore di parsing...), ciascuno con lo step
 * di simulazione in cui e' avvenuto e un livello di gravita'.
 *
 * Il modulo non conosce nessun'altra struttura del progetto (object_t,
 * controllore_t, ecc.): chi lo usa costruisce il messaggio con
 * log_evento( l, step, livello, "formato", ... ), in stile printf.
 * Questo lo rende utilizzabile da qualunque modulo (main, parser,
 * Controllore) senza introdurre dipendenze circolari.
 */

#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include "errors.h"

/** @brief Livello di gravita' di un evento registrato nel log. */
typedef enum {
    LOG_INFO    = 0,  /**< Evento normale (arrivo, completamento, instradamento...). */
    LOG_WARNING = 1,  /**< Evento anomalo ma gestito (blocco per buffer pieno,
                         *   anomalia del sensore di qualita', riga scartata
                         *   dal parser...). */
    LOG_ERROR   = 2   /**< Errore che impedisce un'operazione (file di
                         *   configurazione non apribile, allocazione
                         *   fallita...). */
} LogLivello;

typedef struct log log_t;

/**
 * @brief Crea un log e apre il file indicato in scrittura (sovrascrive
 *        un eventuale file preesistente con lo stesso nome).
 * @param path Percorso del file di log (non deve essere NULL).
 * @param anche_su_stdout Se true, ogni evento registrato viene anche
 *        stampato su stdout oltre che scritto sul file (utile in fase
 *        di sviluppo; in una run "pulita" conviene false e leggere solo
 *        il file).
 * @param errCode puntatore opzionale (puo' essere NULL) in cui viene
 *        scritto OP_SUCCESS oppure un codice ERR_* (vedi errors.h):
 *        ERR_NULL_PTR se path e' NULL, ERR_NOT_FOUND se il file non e'
 *        apribile (es. percorso/permessi non validi), ERR_ALLOC se
 *        l'allocazione fallisce.
 * @return Puntatore al log allocato, o NULL in caso di errore.
 */
log_t *log_create( const char *path, bool anche_su_stdout, short int *errCode );

/**
 * @brief Chiude il file di log e libera la memoria. Puo' essere
 *        chiamata con l == NULL (non fa nulla), cosi' come le altre
 *        *_destroy del progetto.
 * @param l Puntatore al log.
 */
void log_destroy( log_t *l );

/**
 * @brief Registra un evento nel log, con lo step di simulazione e il
 *        livello di gravita' indicati. Formato della riga scritta:
 *            [step %5d] [LIVELLO] messaggio
 *        (oppure "[         ]" al posto dello step se step < 0, per
 *        eventi avvenuti fuori dal ciclo di simulazione, es. durante il
 *        caricamento della configurazione).
 * @param l Puntatore al log. Se NULL, la funzione non fa nulla: questo
 *        rende il logging facoltativo in ogni modulo chiamante senza
 *        bisogno di controlli "if (log != NULL)" sparsi ovunque.
 * @param step Step di simulazione corrente, o un valore negativo se
 *        l'evento avviene fuori dal ciclo di simulazione.
 * @param livello Livello di gravita' dell'evento (vedi LogLivello).
 * @param formato Stringa di formato in stile printf.
 * @param ... Argomenti variabili, come in printf.
 */
void log_evento( log_t *l, int step, LogLivello livello, const char *formato, ... );

/**
 * @brief Numero di eventi registrati finora per un dato livello.
 * @param l Puntatore al log.
 * @param livello Livello di gravita'.
 * @return Numero di eventi, ERR_NULL_PTR se l e' NULL, ERR_OUT_OF_RANGE
 *         se livello non e' uno dei valori validi di LogLivello.
 */
long log_getContatore( const log_t *l, LogLivello livello );

/**
 * @brief Stampa su stdout un riepilogo del numero di eventi registrati
 *        per ciascun livello. Va chiamata a fine simulazione, come le
 *        altre funzioni *_stampa del progetto (es. statistiche_stampa).
 * @param l Puntatore al log.
 */
void log_stampaRiepilogo( const log_t *l );

#endif /* LOG_H */
