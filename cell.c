/**
 * @file cell.c
 * @brief Implementazione del modulo cella.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "registry.h"

/**
 * @brief Nodo della lista interna dei buffer posseduti dalla cella.
 *
 * Serve solo per sapere "quali buffer ho creato io" così da poterli
 * tutti liberare in cell_destroy; la risoluzione ID->puntatore durante
 * la simulazione passa comunque dal registro (registry_getBuffer),
 * non da questa lista.
 */
typedef struct bufferListNode {
    buffer_t *buffer;
    struct bufferListNode *next;
} bufferListNode_t;

/**
 * @brief Definizione completa di cell_t (nascosta a chi include cell.h).
 *
 * Quando esisteranno machine_t/isp_t/sensor_t, andranno aggiunte qui
 * liste analoghe (machineListNode_t *machines, ecc.), seguendo lo
 * stesso schema di bufferListNode_t.
 */
struct cell {
    bufferListNode_t *buffers;
};

/* ------------------------------------------------------------------ */
/*  CREAZIONE / DISTRUZIONE                                            */
/* ------------------------------------------------------------------ */

cell_t *cell_create( void )
{
    cell_t *cell;

    cell = malloc( sizeof( cell_t ) );
    if ( cell == NULL ) {
        return NULL;
    }

    cell->buffers = NULL;

    return cell;
}

void cell_destroy( cell_t *cell )
{
    bufferListNode_t *cur;
    bufferListNode_t *next;

    if ( cell == NULL ) {
        return;
    }

    cur = cell->buffers;
    while ( cur != NULL ) {
        next = cur->next;
        registry_remove( buffer_getID( cur->buffer ) );
        buffer_delete( cur->buffer );
        free( cur );
        cur = next;
    }

    free( cell );
}

/* ------------------------------------------------------------------ */
/*  AGGIUNTA ENTITÀ                                                     */
/* ------------------------------------------------------------------ */

buffer_t *cell_addBuffer( cell_t *cell, const char *ID, int capacity, short int *errCode )
{
    buffer_t *buf;
    bufferListNode_t *node;
    short int localErr;

    if ( cell == NULL ) {
        if ( errCode != NULL ) {
            *errCode = ERR_NULL_PTR;
        }
        return NULL;
    }

    buf = buffer_create( ID, capacity, &localErr );
    if ( buf == NULL ) {
        if ( errCode != NULL ) {
            *errCode = localErr;
        }
        return NULL;
    }

    localErr = registry_add( ID, ENTITY_BUFFER, buf );
    if ( localErr != OP_SUCCESS ) {
        /* ID già presente nel registro: non entra a far parte della cella. */
        buffer_delete( buf );
        if ( errCode != NULL ) {
            *errCode = localErr;
        }
        return NULL;
    }

    node = malloc( sizeof( bufferListNode_t ) );
    if ( node == NULL ) {
        registry_remove( ID );
        buffer_delete( buf );
        if ( errCode != NULL ) {
            *errCode = ERR_ALLOC;
        }
        return NULL;
    }

    node->buffer = buf;
    node->next = cell->buffers;
    cell->buffers = node;

    if ( errCode != NULL ) {
        *errCode = OP_SUCCESS;
    }

    return buf;
}

/* ------------------------------------------------------------------ */
/*  COLLEGAMENTI                                                        */
/* ------------------------------------------------------------------ */

short int cell_connect( cell_t *cell, const char *fromID, const char *toID )
{
    entity_type_t fromType;
    entity_type_t toType;
    buffer_t *fromBuf;
    buffer_t *toBuf;
    short int result;

    if ( cell == NULL || fromID == NULL || toID == NULL ) {
        return ERR_NULL_PTR;
    }

    if ( registry_getType( fromID, &fromType ) != OP_SUCCESS ) {
        return ERR_NOT_FOUND;
    }
    if ( registry_getType( toID, &toType ) != OP_SUCCESS ) {
        return ERR_NOT_FOUND;
    }

    /* Per ora sappiamo collegare solo buffer<->buffer. Quando
     * machine_t/isp_t esisteranno, questo blocco andrà esteso con gli
     * altri casi (es. buffer->machine, machine->isp, ...), seguendo
     * lo stesso schema: risolvere il puntatore giusto in base al tipo
     * e chiamare la corrispondente _addOutput/_addInput. */
    if ( fromType != ENTITY_BUFFER || toType != ENTITY_BUFFER ) {
        return ERR_NOT_SUPPORTED;
    }

    fromBuf = registry_getBuffer( fromID );
    toBuf = registry_getBuffer( toID );

    result = buffer_addOutput( fromBuf, toID );
    if ( result != OP_SUCCESS ) {
        return result;
    }

    result = buffer_addInput( toBuf, fromID );
    if ( result != OP_SUCCESS ) {
        return result;
    }

    return OP_SUCCESS;
}

/* ------------------------------------------------------------------ */
/*  ACCESSO / STAMPA                                                    */
/* ------------------------------------------------------------------ */

buffer_t *cell_getBuffer( const cell_t *cell, const char *ID )
{
    if ( cell == NULL ) {
        return NULL;
    }
    return registry_getBuffer( ID );
}

short int cell_removeBuffer( cell_t *cell, const char *ID )
{
    bufferListNode_t *cur;
    bufferListNode_t *prev;

    if ( cell == NULL || ID == NULL ) {
        return ERR_NULL_PTR;
    }

    prev = NULL;
    cur = cell->buffers;
    while ( cur != NULL && strcmp( buffer_getID( cur->buffer ), ID ) != 0 ) {
        prev = cur;
        cur = cur->next;
    }

    if ( cur == NULL ) {
        return ERR_NOT_FOUND;
    }

    if ( prev == NULL ) {
        cell->buffers = cur->next;
    } else {
        prev->next = cur->next;
    }

    registry_remove( ID );
    buffer_delete( cur->buffer );
    free( cur );

    return OP_SUCCESS;
}

int cell_getBufferCount( const cell_t *cell )
{
    bufferListNode_t *cur;
    int count;

    if ( cell == NULL ) {
        return ERR_NULL_PTR;
    }

    count = 0;
    cur = cell->buffers;
    while ( cur != NULL ) {
        count++;
        cur = cur->next;
    }

    return count;
}

bool cell_hasBuffer( const cell_t *cell, const char *ID )
{
    bufferListNode_t *cur;

    if ( cell == NULL || ID == NULL ) {
        return false;
    }

    cur = cell->buffers;
    while ( cur != NULL ) {
        if ( strcmp( buffer_getID( cur->buffer ), ID ) == 0 ) {
            return true;
        }
        cur = cur->next;
    }

    return false;
}

void cell_print( const cell_t *cell )
{
    bufferListNode_t *cur;

    if ( cell == NULL ) {
        printf( "cell_print: cella NULL\n" );
        return;
    }

    cur = cell->buffers;
    while ( cur != NULL ) {
        buffer_print( cur->buffer );
        cur = cur->next;
    }
}
