/**
 * @file errors.h
 * @brief Codici di ritorno condivisi tra i moduli del progetto.
 *
 * Aggiungere nuovi ERR_* qui man mano che emergono nuovi
 * casi da distinguere, senza toccare le firme delle funzioni esistenti.
 */

#ifndef ERRORS_H
#define ERRORS_H

/** @brief Operazione completata con successo. */
#define OP_SUCCESS       1

/** @brief Puntatore in ingresso NULL dove non era atteso. */
#define ERR_NULL_PTR    -1

/** @brief ID non valido (vuoto, troppo lungo, o formato non corretto). */
#define ERR_ID_INVALID  -2

/** @brief ID o oggetto cercato non trovato. */
#define ERR_NOT_FOUND   -3

/** @brief Operazione non eseguibile perché il buffer è pieno. */
#define ERR_FULL        -4

/** @brief Operazione non eseguibile perché il buffer è vuoto. */
#define ERR_EMPTY       -5

/** @brief malloc/calloc fallita. */
#define ERR_ALLOC       -6

/** @brief Valore fuori dal range accettabile (es. capacità <= 0). */
#define ERR_OUT_OF_RANGE -7

#endif /* ERRORS_H */
