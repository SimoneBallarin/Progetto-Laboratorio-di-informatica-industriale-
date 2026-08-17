/**
 * @file registry.h
 * @brief Registro globale che associa un ID al puntatore dell'entità
 *        reale (buffer, macchina, ISP, sensore) e al suo tipo.
 *
 * È l'unico punto della cella dove, dato un ID stringa (es. letto da
 * ID_Next di un buffer, o dal file di configurazione), puoi risalire
 * al puntatore vero dell'entità corrispondente.
 *
 * Il puntatore generico è tenuto SOLO dentro registry.c (non è mai
 * esposto all'esterno): chi usa questo modulo chiama sempre una
 * funzione tipizzata (es. registry_getBuffer), che restituisce già
 * il tipo giusto senza bisogno di cast manuali o di maneggiare void*.
 *
 * Lo stato del registro è globale all'interno del programma (una sola
 * cella per esecuzione, coerente con lo scenario del progetto).
 */

#ifndef REGISTRY_H
#define REGISTRY_H

#include "object.h"   /* IDLENGTH */
#include "errors.h"
#include "buffer.h"
#include "machine.h"
#include "isp.h"
#include "nastro.h"

/* Header dei moduli sensori/attuatori: servono solo per i tipi
 * (SensoreBuffer, Motore, ecc.) usati dai getter tipizzati sotto.
 * registry.c non ha altre dipendenze da quei moduli. */
#include "S_Buffer.h"
#include "S_Presenza.h"
#include "S_Qualita.h"
#include "Motore.h"
#include "Deviatore.h"

/**
 * @brief Tipo di entità registrata.
 *
 * ENTITY_BUFFER/MACHINE/ISP/NASTRO sono entità di trasporto o
 * lavorazione allo stesso livello nella topologia della cella (si
 * collegano con cell_connect); le ENTITY_SENSOR_x e ENTITY_ACTUATOR_x
 * invece si "agganciano" a una di quelle entità (con
 * cell_attachSensor/cell_attachActuator), non fanno parte della
 * topologia dei collegamenti.
 */
typedef enum {
    ENTITY_BUFFER,
    ENTITY_MACHINE,
    ENTITY_ISP,
    ENTITY_NASTRO,
    ENTITY_SENSOR_BUFFER,
    ENTITY_SENSOR_PRESENZA,
    ENTITY_SENSOR_QUALITA,
    ENTITY_ACTUATOR_MOTORE,
    ENTITY_ACTUATOR_DEVIATORE
} entity_type_t;

/**
 * @brief Registra un'entità nel registro globale.
 * @param ID Identificativo dell'entità (es. "B1"), non vuoto e < IDLENGTH.
 * @param type Tipo dell'entità (vedi entity_type_t).
 * @param ptr Puntatore alla struct reale (es. un buffer_t* già creato).
 *        Il registro non ne prende possesso: non lo libera mai, resta
 *        di proprietà di chi l'ha creato.
 * @return OP_SUCCESS se registrato, ERR_DUPLICATE se l'ID è già presente,
 *         un altro codice ERR_* (vedi errors.h) per altri errori.
 */
short int registry_add( const char *ID, entity_type_t type, void *ptr );

/**
 * @brief Rimuove una voce dal registro.
 *
 * Rimuove solo la voce del registro: non libera la memoria della
 * struct puntata (va fatto a parte, es. con buffer_delete).
 * @param ID Identificativo dell'entità da rimuovere.
 * @return OP_SUCCESS se rimossa, ERR_NOT_FOUND se non presente,
 *         ERR_NULL_PTR se ID è NULL.
 */
short int registry_remove( const char *ID );

/**
 * @brief Restituisce il tipo di entità associato a un ID.
 * @param ID Identificativo cercato.
 * @param outType puntatore in cui viene scritto il tipo trovato.
 * @return OP_SUCCESS se trovato, ERR_NOT_FOUND se l'ID non è registrato,
 *         ERR_NULL_PTR se ID o outType sono NULL.
 */
short int registry_getType( const char *ID, entity_type_t *outType );

/**
 * @brief Restituisce il puntatore al buffer con l'ID indicato.
 * @param ID Identificativo cercato.
 * @return Puntatore al buffer_t, oppure NULL se l'ID non è registrato
 *         o corrisponde a un'entità di un tipo diverso da ENTITY_BUFFER.
 */
buffer_t *registry_getBuffer( const char *ID );

/**
 * @brief Restituisce il puntatore alla macchina (stazione M) con l'ID indicato.
 * @param ID Identificativo cercato.
 * @return Puntatore al machine_t, oppure NULL se l'ID non è registrato
 *         o corrisponde a un'entità di un tipo diverso da ENTITY_MACHINE.
 */
machine_t *registry_getMachine( const char *ID );

/**
 * @brief Restituisce il puntatore alla ISP con l'ID indicato.
 * @param ID Identificativo cercato.
 * @return Puntatore all'isp_t, oppure NULL se l'ID non è registrato
 *         o corrisponde a un'entità di un tipo diverso da ENTITY_ISP.
 */
isp_t *registry_getISP( const char *ID );

/**
 * @brief Restituisce il puntatore al nastro con l'ID indicato.
 * @param ID Identificativo cercato.
 * @return Puntatore al nastro_t, oppure NULL se l'ID non è registrato
 *         o corrisponde a un'entità di un tipo diverso da ENTITY_NASTRO.
 */
nastro_t *registry_getNastro( const char *ID );

/**
 * @brief Restituisce il sensore di livello buffer con l'ID indicato.
 * @param ID Identificativo cercato.
 * @return Puntatore, oppure NULL se non registrato o di un altro tipo.
 */
SensoreBuffer *registry_getSensoreBuffer( const char *ID );

/**
 * @brief Restituisce il sensore di presenza con l'ID indicato.
 * @param ID Identificativo cercato.
 * @return Puntatore, oppure NULL se non registrato o di un altro tipo.
 */
SensorePresenza *registry_getSensorePresenza( const char *ID );

/**
 * @brief Restituisce il sensore di qualità con l'ID indicato.
 * @param ID Identificativo cercato.
 * @return Puntatore, oppure NULL se non registrato o di un altro tipo.
 */
SensoreQualita *registry_getSensoreQualita( const char *ID );

/**
 * @brief Restituisce il motore con l'ID indicato.
 * @param ID Identificativo cercato.
 * @return Puntatore, oppure NULL se non registrato o di un altro tipo.
 */
Motore *registry_getMotore( const char *ID );

/**
 * @brief Restituisce il deviatore con l'ID indicato.
 * @param ID Identificativo cercato.
 * @return Puntatore, oppure NULL se non registrato o di un altro tipo.
 */
Deviatore *registry_getDeviatore( const char *ID );

/**
 * @brief Svuota il registro.
 *
 * Libera solo le voci del registro, non le struct puntate.
 */
void registry_clear( void );

#endif /* REGISTRY_H */
