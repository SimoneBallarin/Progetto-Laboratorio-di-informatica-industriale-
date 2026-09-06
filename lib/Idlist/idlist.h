/**
 * @file idlist.h
 * @brief Lista concatenata generica di ID, usata per rappresentare
 *        ingressi/uscite di qualunque entità della cella (buffer,
 *        macchina, ISP, sensore).
 *
 * Il modulo espone solo operazioni di base (aggiungi, cerca, conta,
 * leggi per indice, rimuovi, libera). Le entità che la usano NON
 * espongono idNode_t nella propria interfaccia pubblica: ci
 * costruiscono sopra funzioni proprie tipo entita_getOutputCount /
 * entita_getOutputAt, che nascondono la lista e copiano l'ID invece
 * di restituire un puntatore interno.
 */

#ifndef IDLIST_H
#define IDLIST_H

#include <stdbool.h>
#include "object.h"   /* per IDLENGTH */
#include "errors.h"

/**
 * @brief Nodo della lista: un ID e il puntatore al nodo successivo.
 */
typedef struct idNode {
    char ID[IDLENGTH];        /**< ID memorizzato in questo nodo. */
    struct idNode *next;      /**< Nodo successivo, NULL se ultimo. */
} idNode_t;

/**
 * @brief Aggiunge un ID in coda alla lista, se non già presente.
 * @param head puntatore alla testa della lista (può essere modificata,
 *        es. se la lista era vuota).
 * @param ID ID da aggiungere, non vuoto e < IDLENGTH caratteri.
 * @return OP_SUCCESS se aggiunto, ERR_DUPLICATE se l'ID è già presente,
 *         un altro codice ERR_* (vedi errors.h) per altri errori.
 */
short int idlist_add( idNode_t **head, const char *ID );

/**
 * @brief Controlla se un ID è presente nella lista.
 * @param head testa della lista.
 * @param ID ID cercato.
 * @return true se presente, false altrimenti (anche se head o ID sono NULL).
 */
bool idlist_contains( const idNode_t *head, const char *ID );

/**
 * @brief Conta gli elementi della lista.
 * @param head testa della lista.
 * @return Numero di elementi (0 se la lista è vuota/NULL).
 */
int idlist_count( const idNode_t *head );

/**
 * @brief Legge l'ID all'indice indicato (0-based), copiandolo in outID.
 * @param head testa della lista.
 * @param index indice dell'elemento richiesto (0 = primo elemento).
 * @param outID buffer di dimensione almeno IDLENGTH in cui viene copiato l'ID.
 * @return OP_SUCCESS se trovato, ERR_NULL_PTR se outID è NULL,
 *         ERR_OUT_OF_RANGE se index è negativo, ERR_NOT_FOUND se index
 *         è oltre la fine della lista.
 */
short int idlist_getAt( const idNode_t *head, int index, char outID[IDLENGTH] );

/**
 * @brief Rimuove un ID dalla lista, se presente.
 * @param head puntatore alla testa della lista (può essere modificata).
 * @param ID ID da rimuovere.
 * @return OP_SUCCESS se rimosso, ERR_NOT_FOUND se non presente,
 *         ERR_NULL_PTR se head o ID sono NULL.
 */
short int idlist_remove( idNode_t **head, const char *ID );

/**
 * @brief Libera tutti i nodi della lista.
 * @param head testa della lista da liberare.
 */
void idlist_free( idNode_t *head );

#endif /* IDLIST_H */
