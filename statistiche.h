/**
 * @file statistiche.h
 * @brief Modulo "statistiche": raccoglie e aggrega le metriche
 *        dell'impianto durante e dopo la simulazione, usando SOLO le
 *        funzioni pubbliche già esistenti di object.h, buffer.h e
 *        Controllore.h (nessuna libreria esistente viene modificata).
 *
 * Segue lo stesso schema del resto del progetto: creazione, uso tramite
 * puntatore opaco, distruzione (vedi Controllore.h/cell.h). Come il
 * Controllore, non crea nulla in automatico: buffer e motori da
 * monitorare vanno dichiarati esplicitamente con
 * statistiche_monitoraBuffer/statistiche_monitoraMotore prima di
 * cominciare a campionare.
 *
 * DUE STATISTICHE (occupazione media/massima nel tempo, numero di
 * blocchi) NON si possono calcolare a posteriori: richiedono di essere
 * "nutrite" ad ogni passo di simulazione, chiamando
 * statistiche_campiona e (quando succede) statistiche_registraBlocco
 * dal ciclo principale del main - vedi i commenti su ciascuna funzione
 * per dove va chiamata esattamente.
 *
 * NON raggiunge il sensore di qualità/presenza attraverso l'ISP stessa
 * (che non li possiede più direttamente): passa dai getter dedicati
 * aggiunti a Controllore.h (controllore_getLettureQualita e affini),
 * seguendo lo stesso schema di controllore_getPercentualeBuffer.
 */

#ifndef STATISTICHE_H
#define STATISTICHE_H

#include "object.h"
#include "errors.h"
#include "Controllore.h"
#include <stdbool.h>

typedef struct statistiche statistiche_t;

/**
 * @brief Crea una statistiche_t vuota, senza nessun buffer/motore
 *        monitorato (vanno dichiarati esplicitamente, vedi sotto).
 * @param errCode puntatore opzionale (può essere NULL) in cui viene
 *        scritto OP_SUCCESS o un codice ERR_* (vedi errors.h).
 * @return Puntatore alla struct allocata, o NULL in caso di errore.
 */
statistiche_t *statistiche_create( short int *errCode );

/**
 * @brief Elimina la statistiche_t e libera tutta la memoria interna.
 * @param s Puntatore alla struct.
 */
void statistiche_destroy( statistiche_t *s );

/**
 * @brief Dichiara un buffer da monitorare per occupazione media/
 *        massima nel tempo (statistica #6 della tabella). Va chiamata
 *        PRIMA del ciclo di simulazione, una volta per ogni buffer che
 *        si vuole seguire.
 * @param s Puntatore alla struct.
 * @param bufferID ID del buffer (deve avere un SensoreBuffer agganciato
 *        nel controllore, altrimenti statistiche_campiona lo salta
 *        silenziosamente per quel buffer ad ogni passo).
 * @return OP_SUCCESS se aggiunto, un codice ERR_* (vedi errors.h)
 *         altrimenti, incluso ERR_DUPLICATE se già monitorato.
 */
short int statistiche_monitoraBuffer( statistiche_t *s, const char *bufferID );

/**
 * @brief Dichiara un motore (su un nastro o una macchina) da
 *        monitorare per il tempo cumulativo ON/OFF (statistica #5).
 *        Va chiamata PRIMA del ciclo di simulazione (ma va bene anche
 *        dopo, il tempo viene letto solo a fine simulazione con
 *        statistiche_stampa - non serve campionarlo passo per passo,
 *        a differenza dei buffer).
 * @param s Puntatore alla struct.
 * @param targetID ID del nastro o della macchina a cui è agganciato
 *        il motore (vedi controllore_collegaMotore).
 * @return OP_SUCCESS se aggiunto, un codice ERR_* (vedi errors.h)
 *         altrimenti, incluso ERR_DUPLICATE se già monitorato.
 */
short int statistiche_monitoraMotore( statistiche_t *s, const char *targetID );

/**
 * @brief Dichiara un'ISP da monitorare per le statistiche del suo
 *        sensore di qualità (letture totali, anomalie, CONFORME/
 *        RIVALUTAZIONE/SCARTO, conteggio materiale A/B). Come per i
 *        motori, il dato è già cumulativo nel sensore stesso: non
 *        serve campionarlo passo per passo, viene letto fresco da
 *        statistiche_stampa.
 * @param s Puntatore alla struct.
 * @param ispID ID dell'ISP (deve avere un SensoreQualita agganciato,
 *        vedi controllore_collegaSensoreQualita - un'ISP "passacarte"
 *        senza sensore non ha nulla da mostrare qui).
 * @return OP_SUCCESS se aggiunta, un codice ERR_* (vedi errors.h)
 *         altrimenti, incluso ERR_DUPLICATE se già monitorata.
 */
short int statistiche_monitoraISP( statistiche_t *s, const char *ispID );

/**
 * @brief Dichiara un sensore di presenza da monitorare (letture
 *        totali, rilevamenti/nuovi arrivi distinti). Stesso schema di
 *        statistiche_monitoraISP: dato cumulativo, letto fresco da
 *        statistiche_stampa.
 * @param s Puntatore alla struct.
 * @param ID ID a cui è agganciato il sensore di presenza (es. "B1",
 *        vedi controllore_collegaSensorePresenza).
 * @return OP_SUCCESS se aggiunto, un codice ERR_* (vedi errors.h)
 *         altrimenti, incluso ERR_DUPLICATE se già monitorato.
 */
short int statistiche_monitoraSensorePresenza( statistiche_t *s, const char *ID );

/**
 * @brief Campiona l'occupazione di tutti i buffer monitorati in questo
 *        istante. VA CHIAMATA UNA VOLTA PER OGNI PASSO DI SIMULAZIONE,
 *        subito dopo controllore_step - senza questa chiamata ad ogni
 *        passo, media e massimo di occupazione restano a zero (non si
 *        possono calcolare a posteriori dallo stato finale).
 * @param s Puntatore alla struct.
 * @param ctrl Puntatore al controllore. NON è const: la funzione chiama
 *        internamente controllore_getPercentualePiccoBuffer, che
 *        resetta il tracciamento del picco transitorio del buffer ad
 *        ogni chiamata (vedi Controllore.h) - per questo va chiamata
 *        una volta sola per passo, qui, e non altrove.
 * @return OP_SUCCESS, o ERR_NULL_PTR se s o ctrl sono NULL.
 */
short int statistiche_campiona( statistiche_t *s, controllore_t *ctrl );

/**
 * @brief Registra un blocco (un inserimento rifiutato perché un buffer
 *        era pieno) per il buffer indicato (statistica #8). Va chiamata
 *        dal main ogni volta che un inserimento fallisce per questo
 *        motivo (lo stesso punto dove oggi c'è solo una fprintf su
 *        stderr, es. "B1 pieno o inesistente: arrivo scartato").
 * @param s Puntatore alla struct.
 * @param bufferID ID del buffer che ha rifiutato l'inserimento (non
 *        deve necessariamente essere tra quelli monitorati con
 *        statistiche_monitoraBuffer: viene comunque contato).
 * @return OP_SUCCESS, o ERR_NULL_PTR se s o bufferID sono NULL.
 */
short int statistiche_registraBlocco( statistiche_t *s, const char *bufferID );

/**
 * @brief Registra il completamento di un oggetto (arrivato in un
 *        buffer terminale, cioè object_getStepOut(obj) != STEP_OUT_NONE)
 *        per le statistiche di tempo in sistema e "% entro scadenza" per
 *        classe di priorità (statistiche #2, #3, #4). Va chiamata una
 *        volta per ogni oggetto arrivato, tipicamente scorrendo i buffer
 *        terminali a fine simulazione (vedi bufferObj_t in buffer.h:
 *        buffer->head, ->dato, ->next).
 *
 * Oggetti con stepOut == STEP_OUT_NONE (non ancora arrivati/ancora in
 * coda) vengono ignorati silenziosamente: non chiamare questa funzione
 * per loro, o passa comunque l'oggetto - il controllo è già fatto
 * internamente, quindi è sicuro chiamarla per ogni oggetto trovato nei
 * buffer terminali senza doverli filtrare prima a mano.
 * @param s Puntatore alla struct.
 * @param obj Puntatore all'oggetto (sola lettura, non modificato).
 * @param scadenza_passi Soglia (in passi di simulazione) usata per DUE
 *        conteggi separati e indipendenti (statistica #4), per non
 *        creare ambiguità su cosa significhi "entro scadenza" (vedi
 *        doc di prioritaStat_t/statistiche_riepilogo_t):
 *          - tempo SISTEMA (coda di ingresso + pipeline) <= scadenza_passi
 *          - tempo PROCESSO (solo pipeline, ESCLUSA la coda) <= scadenza_passi
 *        Nessun oggetto nel progetto ha oggi un concetto di scadenza
 *        proprio: è un parametro esterno, deciso da chi chiama (es. una
 *        soglia fissa uguale per tutti, o calcolata da altrove) - passa
 *        un numero <= 0 per disattivare entrambe le statistiche (tutti
 *        gli oggetti verranno contati come "fuori scadenza" in entrambe).
 * @return OP_SUCCESS, o ERR_NULL_PTR se s o obj sono NULL.
 */
short int statistiche_registraCompletamento( statistiche_t *s, const object_t *obj, int scadenza_passi );

/**
 * @brief Stampa un riepilogo di tutte le statistiche raccolte finora.
 *        Può essere chiamata più volte (es. anche a metà simulazione
 *        per un controllo intermedio, non solo alla fine).
 * @param s Puntatore alla struct.
 * @param ctrl Puntatore al controllore (sola lettura): serve per
 *        leggere il tempo ON/OFF dei motori monitorati con
 *        statistiche_monitoraMotore (controllore_getTempoMotoreOn/Off),
 *        dato che quel dato non viene campionato passo per passo ma
 *        letto fresco al momento della stampa.
 * @param n_step_simulazione Numero di passi totali della simulazione
 *        (statistica #1) - non è tenuto internamente dalla libreria,
 *        va passato da chi lo sa (il main).
 * @param path_output Percorso di un file .txt su cui scrivere il
 *        riepilogo (sovrascrive un eventuale file preesistente con lo
 *        stesso nome), oppure NULL per non scrivere su file. Scritto
 *        SEMPRE, indipendentemente da anche_su_stdout: rappresenta il
 *        resoconto persistente della run, va salvato anche quando la
 *        stampa a schermo è disattivata. Se il file non è apribile, un
 *        avviso viene stampato su stderr e la funzione prosegue comunque.
 * @param anche_su_stdout Se true, il riepilogo viene anche stampato su
 *        stdout (oltre che scritto su file, se path_output non è NULL);
 *        se false, va SOLO su file. Utile per non affollare la console
 *        quando la stessa funzione viene chiamata più volte nello stesso
 *        processo (es. il confronto automatico tra due strategie in
 *        app/main.c, dove solo la prima run stampa a schermo).
 */
void statistiche_stampa( const statistiche_t *s, const controllore_t *ctrl, int n_step_simulazione,
                          const char *path_output, bool anche_su_stdout );

/**
 * @brief Riepilogo aggregato "a una riga per metrica", pensato per un
 *        confronto affiancato tra due esecuzioni (es. due strategie di
 *        controllo, o due scenari) - vedi statistiche_getRiepilogo.
 *
 * Ogni campo aggrega su TUTTE le entità monitorate (tutti i buffer
 * dichiarati con statistiche_monitoraBuffer, tutte le priorità, ecc.):
 * per il dettaglio per singolo buffer/priorità resta statistiche_stampa.
 *
 * DUE DEFINIZIONI DI TEMPO, tenute SEMPRE separate per evitare
 * l'ambiguità che il nome "tempo di attraversamento" causava in
 * precedenza (il valore era in realtà sempre stato il tempo totale,
 * coda inclusa) - vedi anche prioritaStat_t in statistiche.c:
 *   - "sistema"  = tempo TOTALE in sistema (coda di ingresso + pipeline),
 *     stepOut - stepCreation.
 *   - "processo" = SOLO la pipeline vera e propria (ISP1->N1->M->B2->ISP2),
 *     ESCLUSA l'attesa in coda su B1, stepOut - stepPartial.
 * Entrambe vengono confrontate con la STESSA scadenza (il parametro
 * scadenza_passi passato a statistiche_registraCompletamento): un
 * oggetto può quindi risultare "entro scadenza" secondo una definizione
 * e non secondo l'altra.
 */
typedef struct {
    long   totale_completati;              /**< Somma su tutte le priorità (stat. #2/#3). */
    double perc_entro_scadenza_tempo_sistema;    /**< % con tempo SISTEMA (coda+processo) <= scadenza. */
    double tempo_medio_sistema;                   /**< Media pesata di stepOut-stepCreation sui completati. */
    double perc_entro_scadenza_tempo_processo;   /**< % con tempo PROCESSO (solo pipeline, no coda) <= scadenza. */
    double tempo_medio_processo;                  /**< Media pesata di stepOut-stepPartial (solo pipeline). */
    double occupazione_media_buffer;        /**< Media delle medie di occupazione dei buffer monitorati (stat. #6). */
    int    occupazione_massima_buffer;      /**< Massimo tra i massimi di occupazione dei buffer monitorati (stat. #6). */
    long   totale_blocchi;                  /**< Blocchi su buffer monitorati + non monitorati (stat. #8). */
    long   totale_anomalie_qualita;         /**< Somma delle anomalie su tutte le ISP monitorate (stat. #9). */
    int    pending_finale;                  /**< controllore_getPendingCount a fine simulazione. */

    /* Scomposizione per fascia di priorità (sez. 2.1 del progetto
     * preliminare: "rispetto delle priorità" come obiettivo separato
     * dalla fluidità generale) - permette di vedere, in un confronto tra
     * strategie, l'effetto che le medie aggregate sopra possono
     * nascondere (in una coda a singolo servitore non preemptive, il
     * tempo di attesa TOTALE medio è invariante rispetto all'ordine di
     * servizio: cambia solo CHI aspetta di più, non la media generale).
     * Basata sul tempo SISTEMA (l'attesa vissuta dall'oggetto), non sul
     * tempo PROCESSO. */
    long   completati_alta_priorita;        /**< priorita' >= 7 (soglia arbitraria su PRIORITY_MAX=10). */
    double perc_entro_scadenza_alta_priorita;
    long   completati_bassa_priorita;       /**< priorita' <= 3. */
    double perc_entro_scadenza_bassa_priorita;
} statistiche_riepilogo_t;

/**
 * @brief Calcola il riepilogo aggregato di cui sopra, dallo stesso stato
 *        interno usato da statistiche_stampa (nessuna logica duplicata:
 *        stesse somme, stessi criteri di aggregazione).
 * @param s Puntatore alla struct.
 * @param ctrl Puntatore al controllore (sola lettura): serve per leggere
 *        le anomalie qualità (controllore_getAnomalieQualita) e il
 *        pending finale (controllore_getPendingCount) di ogni entità
 *        monitorata, dato che quei dati non sono campionati passo per
 *        passo (stesso motivo per cui statistiche_stampa richiede ctrl).
 * @param out Struct di destinazione, allocata dal chiamante.
 * @return OP_SUCCESS se calcolato, ERR_NULL_PTR se s, ctrl o out sono NULL.
 */
short int statistiche_getRiepilogo( const statistiche_t *s, const controllore_t *ctrl, statistiche_riepilogo_t *out );

#endif /* STATISTICHE_H */
