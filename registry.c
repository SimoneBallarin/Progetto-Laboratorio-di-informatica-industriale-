/**
 * @file registry.c
 * @brief Implementazione del registro globale.
 *
 * Internamente è una lista concatenata; è l'unico posto in tutto il
 * progetto dove compare un void* per tenere insieme entità di tipo
 * diverso. Tutto il resto del codice (compreso il resto di questo
 * stesso file) usa solo le funzioni tipizzate come registry_getBuffer.
 */

#include <stdlib.h>
#include <string.h>

#include "registry.h"

typedef struct registryEntry {
    char ID[IDLENGTH];
    entity_type_t type;
    void *ptr;
    struct registryEntry *next;
} registryEntry_t;

/* Stato globale del registro: una sola cella per esecuzione. */
static registryEntry_t *registryHead = NULL;

static registryEntry_t *registry_find( const char *ID )
{
    registryEntry_t *cur;

    if ( ID == NULL ) {
        return NULL;
    }

    cur = registryHead;
    while ( cur != NULL ) {
        if ( strcmp( cur->ID, ID ) == 0 ) {
            return cur;
        }
        cur = cur->next;
    }

    return NULL;
}

short int registry_add( const char *ID, entity_type_t type, void *ptr )
{
    registryEntry_t *node;

    if ( ID == NULL || ptr == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( strlen( ID ) == 0 || strlen( ID ) >= IDLENGTH ) {
        return ERR_ID_INVALID;
    }
    if ( registry_find( ID ) != NULL ) {
        return ERR_DUPLICATE;
    }

    node = malloc( sizeof( registryEntry_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    strncpy( node->ID, ID, IDLENGTH - 1 );
    node->ID[IDLENGTH - 1] = '\0';
    node->type = type;
    node->ptr = ptr;
    node->next = registryHead;
    registryHead = node;

    return OP_SUCCESS;
}

short int registry_remove( const char *ID )
{
    registryEntry_t *cur;
    registryEntry_t *prev;

    if ( ID == NULL ) {
        return ERR_NULL_PTR;
    }

    prev = NULL;
    cur = registryHead;
    while ( cur != NULL && strcmp( cur->ID, ID ) != 0 ) {
        prev = cur;
        cur = cur->next;
    }

    if ( cur == NULL ) {
        return ERR_NOT_FOUND;
    }

    if ( prev == NULL ) {
        registryHead = cur->next;
    } else {
        prev->next = cur->next;
    }
    free( cur );   /* libera solo il nodo del registro, non cur->ptr */

    return OP_SUCCESS;
}

short int registry_getType( const char *ID, entity_type_t *outType )
{
    registryEntry_t *entry;

    if ( ID == NULL || outType == NULL ) {
        return ERR_NULL_PTR;
    }

    entry = registry_find( ID );
    if ( entry == NULL ) {
        return ERR_NOT_FOUND;
    }

    *outType = entry->type;

    return OP_SUCCESS;
}

/* Unico punto del modulo che maneggia il void*: usata solo dalle
 * funzioni tipizzate qui sotto, mai esposta in registry.h. */
static void *registry_getIfType( const char *ID, entity_type_t expectedType )
{
    registryEntry_t *entry;

    entry = registry_find( ID );
    if ( entry == NULL || entry->type != expectedType ) {
        return NULL;
    }

    return entry->ptr;
}

buffer_t *registry_getBuffer( const char *ID )
{
    return (buffer_t *) registry_getIfType( ID, ENTITY_BUFFER );
}

nastro_t *registry_getNastro( const char *ID )
{
    return (nastro_t *) registry_getIfType( ID, ENTITY_NASTRO );
}

SensoreBuffer *registry_getSensoreBuffer( const char *ID )
{
    return (SensoreBuffer *) registry_getIfType( ID, ENTITY_SENSOR_BUFFER );
}

SensorePresenza *registry_getSensorePresenza( const char *ID )
{
    return (SensorePresenza *) registry_getIfType( ID, ENTITY_SENSOR_PRESENZA );
}

SensoreQualita *registry_getSensoreQualita( const char *ID )
{
    return (SensoreQualita *) registry_getIfType( ID, ENTITY_SENSOR_QUALITA );
}

Motore *registry_getMotore( const char *ID )
{
    return (Motore *) registry_getIfType( ID, ENTITY_ACTUATOR_MOTORE );
}

Deviatore *registry_getDeviatore( const char *ID )
{
    return (Deviatore *) registry_getIfType( ID, ENTITY_ACTUATOR_DEVIATORE );
}

void registry_clear( void )
{
    registryEntry_t *cur;
    registryEntry_t *next;

    cur = registryHead;
    while ( cur != NULL ) {
        next = cur->next;
        free( cur );
        cur = next;
    }
    registryHead = NULL;
}
