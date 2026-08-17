#ifndef DEVIATORE_H
#define DEVIATORE_H
#include "object.h"
#include <stdbool.h>

/* Rinominati per evitare collisioni con gli enum generici di Motore.h
 * (ON/OFF) e per correggere l'errore di sintassi (';' invece di ','
 * nella versione originale, che non compilava). */
typedef enum {
    DEVIATORE_ON  = 1,
    DEVIATORE_OFF = 0
} StatoDeviatore;

/* Rinominato da 'posizioneDviatore' (refuso) e da OK/NOT_OK (collidevano
 * con altri enum del progetto) a PosizioneDeviatore / POSIZIONE_*. */
typedef enum {
    POSIZIONE_OK     = 1,
    POSIZIONE_NOT_OK = 0
} PosizioneDeviatore;

typedef struct { /* struttura per rappresentare un deviatore */
    char ID[IDLENGTH];
    int  posizione_target;
    int  posizione_attuale;
    StatoDeviatore status;
    StatoDeviatore status_precedente;
    PosizioneDeviatore inPosizione;
} Deviatore;

/* Tempo minimo tra due commutazioni consecutive (sez. 5.2 del progetto:
 * "meccanismo del dispositivo di smistamento, con tempo minimo tra due
 * commutazioni"). Nella versione originale questa struct era dichiarata
 * ma non usata da nessuna funzione: il vincolo non era implementato. */
typedef struct {
    int  time_last_commutazione;      /* istante dell'ultimo cambio di posizione_target */
    int  tempo_minimo_tra_commutazioni;
    bool prima_commutazione_fatta;    /* false finche' non e' mai avvenuta una commutazione */
} DeviatoreTime;

/* Tutte le funzioni ritornano 0 in successo o un codice ERR_* (vedi
 * errors.h) su puntatore NULL. I getter che restituiscono bool
 * restituiscono false su puntatore NULL (un bool non puo' distinguere
 * un errore da un valore legittimo). */

int  deviatore_init(Deviatore *d, const char *ID);
int  deviatoretime_init(DeviatoreTime *dt, int tempo_minimo_tra_commutazioni);

/* Accende/spegne il deviatore (comportamento invariato rispetto
 * all'originale, solo con controllo su NULL e tipo enum invece di bool). */
int  set_deviatore(Deviatore *d, StatoDeviatore stato);

/* NUOVA funzione: nella versione originale non esisteva alcun modo di
 * impostare posizione_target (il codice usava una variabile 'target'
 * mai dichiarata): senza questa funzione il deviatore non poteva
 * muoversi verso una posizione diversa da quella iniziale (0).
 * Applica anche il vincolo "tempo minimo tra due commutazioni"
 * richiesto in sez. 5.2: se non e' ancora trascorso
 * dt->tempo_minimo_tra_commutazioni dall'ultima commutazione accettata,
 * la richiesta viene ignorata e si ritorna ERR_NOT_SUPPORTED. */
int  deviatore_imposta_target(Deviatore *d, DeviatoreTime *dt,
                               int nuova_posizione_target, int time_globale);

/* Fa avanzare posizione_attuale verso posizione_target di una unita'
 * per chiamata (coerente con la simulazione a passi discreti, sez. 6). */
int  posizionamento_deviatore(Deviatore *d);

bool get_status_deviatore(const Deviatore *d);
int  get_posizione(const Deviatore *d);
bool get_inPosizione(const Deviatore *d);

#endif
