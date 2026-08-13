#ifndef S_BUFFER_H
#define S_BUFFER_H

#include <stdbool.h>

/* Rinominato da 'BufferStatus' a 'StatoBuffer' per evitare collisione di
 * nome con l'enum 'BufferStatus' definito nel modulo Sensore Qualita':
 * se il controllore include entrambi gli header nello stesso file, due
 * enum con lo stesso tag non potrebbero coesistere. */
/* NB: con solo due stati, BUFFER_EMPTY va interpretato come "non pieno"
 * (comprende sia il buffer davvero vuoto sia livelli intermedi), non
 * letteralmente "vuoto". Se serve distinguere anche il caso intermedio,
 * va aggiunto un terzo valore (es. BUFFER_PARZIALE) e gestito di
 * conseguenza in aggiornamento_status(). */
typedef enum {
    BUFFER_EMPTY = 0,
    BUFFER_FULL  = 1
} StatoBuffer;

typedef struct {
    char ID[20];
    long livello_attuale;
    long livello_massimo;
    StatoBuffer status;
} SensoreBuffer;

/* Ritorna 0 in caso di successo, un codice ERR_* altrimenti.
 * ID viene copiato internamente (max 19 caratteri + terminatore). */
int  sensore_Buffer_init(SensoreBuffer *s, const char *ID, long livello_massimo);

/* new_object: passare 1 quando entra un oggetto, -1 quando ne esce uno,
 * 0 per un aggiornamento senza variazione di livello. */
int  aggiornamento_status(SensoreBuffer *s, int new_object);

int  get_status_buffer(const SensoreBuffer *s);
long get_livello_attuale(const SensoreBuffer *s);
int  get_percentuale_livello(const SensoreBuffer *s);

#endif
