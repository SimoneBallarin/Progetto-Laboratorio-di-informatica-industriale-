/**
 * @file S_Buffer.c
 * @brief Implementazione del sensore di livello buffer.
 */
#include <stdlib.h>
#include <string.h>
#include "S_Buffer.h"
#include "errors.h"
#include "object.h"

int sensore_Buffer_init(SensoreBuffer *s, const char *ID, long livello_massimo)
{
    if (s == NULL || ID == NULL) return ERR_NULL_PTR;
    if (strlen(ID) > sizeof(s->ID) - 1) return ERR_ID_INVALID;
    if (livello_massimo <= 0) return ERR_OUT_OF_RANGE;

    strncpy(s->ID, ID, sizeof(s->ID) - 1);
    s->ID[sizeof(s->ID) - 1] = '\0';

    s->livello_attuale = 0;
    s->livello_massimo = livello_massimo;
    s->status = BUFFER_EMPTY;

    return OP_SUCCESS;
}

int aggiornamento_status(SensoreBuffer *s, int new_object)
{
    if (s == NULL) return ERR_NULL_PTR;

    /* new_object: 1 = e' appena entrato un oggetto, -1 = ne e' appena
     * uscito uno, 0 = nessuna variazione (solo un aggiornamento di stato). */
    if (new_object == 1 && s->livello_attuale < s->livello_massimo) {
        s->livello_attuale++;
    } else if (new_object == -1 && s->livello_attuale > 0) {
        s->livello_attuale--;
    }

    if (s->livello_attuale < 0) {
        s->livello_attuale = 0;
    }

    /* Lo stato va ricalcolato ad ogni chiamata (non solo quando si
     * raggiungono gli estremi): con solo due stati possibili, BUFFER_FULL
     * indica "pieno adesso" e BUFFER_EMPTY indica "non pieno adesso".
     * Aggiornarlo solo ai bordi lascerebbe lo stato "vecchio" quando il
     * buffer si svuota parzialmente dopo essere stato pieno. */
    if (s->livello_attuale >= s->livello_massimo) {
        s->livello_attuale = s->livello_massimo;
        s->status = BUFFER_FULL;
    } else {
        s->status = BUFFER_EMPTY;
    }

    return OP_SUCCESS;
}

int get_status_buffer(const SensoreBuffer *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->status;
}

long get_livello_attuale(const SensoreBuffer *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    return s->livello_attuale;
}

int get_percentuale_livello(const SensoreBuffer *s)
{
    if (s == NULL) return ERR_NULL_PTR;
    if (s->livello_massimo <= 0) return 0; /* difensivo: evita divisione per zero */

    return (int)((s->livello_attuale * 100) / s->livello_massimo);
}
