/**
 * @file buffer.c
 * @brief Implementazione delle funzioni sui buffer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

/* ------------------------------------------------------------------ */
/*  CREAZIONE / DISTRUZIONE                                            */
/* ------------------------------------------------------------------ */

buffer_t *buffer_create( const char *ID, const int capacity, short int *errCode )
{
    buffer_t *buffer;

    if ( ID == NULL ) {
        if ( errCode != NULL ) {
            *errCode = ERR_NULL_PTR;
        }
        return NULL;
    }
    if ( strlen( ID ) == 0 || strlen( ID ) >= IDLENGTH ) {
        if ( errCode != NULL ) {
            *errCode = ERR_ID_INVALID;
        }
        return NULL;
    }
    if ( capacity <= 0 ) {
        if ( errCode != NULL ) {
            *errCode = ERR_OUT_OF_RANGE;
        }
        return NULL;
    }

    buffer = malloc( sizeof( buffer_t ) );
    if ( buffer == NULL ) {
        if ( errCode != NULL ) {
            *errCode = ERR_ALLOC;
        }
        return NULL;
    }

    strncpy( buffer->ID, ID, IDLENGTH - 1 );
    buffer->ID[IDLENGTH - 1] = '\0';

    buffer->capacity = capacity;
    buffer->counter = 0;
    buffer->head = NULL;
    buffer->tail = NULL;
    buffer->inputList = NULL;
    buffer->outputList = NULL;

    if ( errCode != NULL ) {
        *errCode = OP_SUCCESS;
    }

    return buffer;
}

void buffer_delete( buffer_t *buffer )
{
    bufferObj_t *cur;
    bufferObj_t *next;

    if ( buffer == NULL ) {
        return;
    }

    /* Libera solo i nodi wrapper: gli object_t contenuti restano di
     * proprietà di chi li ha creati (object_delete va chiamata a parte). */
    cur = buffer->head;
    while ( cur != NULL ) {
        next = cur->next;
        free( cur );
        cur = next;
    }

    idlist_free( buffer->inputList );
    idlist_free( buffer->outputList );

    free( buffer );
}

/* ------------------------------------------------------------------ */
/*  STATO                                                               */
/* ------------------------------------------------------------------ */

bool buffer_isFull( const buffer_t *buffer )
{
    if ( buffer == NULL ) {
        return false;
    }
    return buffer->counter >= buffer->capacity;
}

bool buffer_isEmpty( const buffer_t *buffer )
{
    if ( buffer == NULL ) {
        return true;
    }
    return buffer->counter == 0;
}

int buffer_getCount( const buffer_t *buffer )
{
    if ( buffer == NULL ) {
        return ERR_NULL_PTR;
    }
    return buffer->counter;
}

/* ------------------------------------------------------------------ */
/*  GETTER SEMPLICI                                                     */
/* ------------------------------------------------------------------ */

const char *buffer_getID( const buffer_t *buffer )
{
    if ( buffer == NULL ) {
        return NULL;
    }
    return buffer->ID;
}

int buffer_getCapacity( const buffer_t *buffer )
{
    if ( buffer == NULL ) {
        return ERR_NULL_PTR;
    }
    return buffer->capacity;
}

/* ------------------------------------------------------------------ */
/*  INGRESSI / USCITE (inputList / outputList)                         */
/* ------------------------------------------------------------------ */

short int buffer_addInput( buffer_t *buffer, const char *ID )
{
    if ( buffer == NULL ) {
        return ERR_NULL_PTR;
    }
    return idlist_add( &buffer->inputList, ID );
}

short int buffer_addOutput( buffer_t *buffer, const char *ID )
{
    if ( buffer == NULL ) {
        return ERR_NULL_PTR;
    }
    return idlist_add( &buffer->outputList, ID );
}

int buffer_getInputCount( const buffer_t *buffer )
{
    if ( buffer == NULL ) {
        return ERR_NULL_PTR;
    }
    return idlist_count( buffer->inputList );
}

short int buffer_getInputAt( const buffer_t *buffer, int index, char outID[IDLENGTH] )
{
    if ( buffer == NULL ) {
        return ERR_NULL_PTR;
    }
    return idlist_getAt( buffer->inputList, index, outID );
}

bool buffer_hasInput( const buffer_t *buffer, const char *ID )
{
    if ( buffer == NULL ) {
        return false;
    }
    return idlist_contains( buffer->inputList, ID );
}

int buffer_getOutputCount( const buffer_t *buffer )
{
    if ( buffer == NULL ) {
        return ERR_NULL_PTR;
    }
    return idlist_count( buffer->outputList );
}

short int buffer_getOutputAt( const buffer_t *buffer, int index, char outID[IDLENGTH] )
{
    if ( buffer == NULL ) {
        return ERR_NULL_PTR;
    }
    return idlist_getAt( buffer->outputList, index, outID );
}

bool buffer_hasOutput( const buffer_t *buffer, const char *ID )
{
    if ( buffer == NULL ) {
        return false;
    }
    return idlist_contains( buffer->outputList, ID );
}

/* ------------------------------------------------------------------ */
/*  INSERIMENTO / RIMOZIONE OGGETTI                                     */
/* ------------------------------------------------------------------ */

int buffer_insertObject( buffer_t *buffer, object_t *object, const bool priority )
{
    bufferObj_t *node;
    bufferObj_t *cur;
    short int newPriority;

    if ( buffer == NULL || object == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( buffer_isFull( buffer ) ) {
        return ERR_FULL;
    }

    node = malloc( sizeof( bufferObj_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }
    node->dato = object;
    node->next = NULL;

    if ( buffer->head == NULL ) {
        /* Buffer vuoto: il nodo diventa sia testa che coda. */
        buffer->head = node;
        buffer->tail = node;
    } else if ( !priority ) {
        /* Inserimento in coda (FIFO). */
        buffer->tail->next = node;
        buffer->tail = node;
    } else {
        /* Inserimento ordinato per priorità decrescente: la testa ha
         * sempre la priorità più alta. */
        newPriority = object_getPriority( object );

        if ( newPriority > object_getPriority( buffer->head->dato ) ) {
            node->next = buffer->head;
            buffer->head = node;
        } else {
            cur = buffer->head;
            while ( cur->next != NULL && object_getPriority( cur->next->dato ) >= newPriority ) {
                cur = cur->next;
            }
            node->next = cur->next;
            cur->next = node;
            if ( node->next == NULL ) {
                buffer->tail = node;
            }
        }
    }

    buffer->counter++;

    return OP_SUCCESS;
}

object_t *buffer_removeObject( buffer_t *buffer, const bool priority )
{
    bufferObj_t *toRemove;
    bufferObj_t *prevOfToRemove;
    object_t *result;

    if ( buffer == NULL || buffer->head == NULL ) {
        return NULL;
    }

    if ( !priority ) {
        /* Rimozione dalla testa (FIFO). */
        toRemove = buffer->head;
        prevOfToRemove = NULL;
    } else {
        /* Ricerca del nodo con priorità più alta, indipendentemente
         * dall'ordine con cui gli oggetti sono stati inseriti. */
        bufferObj_t *scanPrev = NULL;
        bufferObj_t *scan = buffer->head;

        toRemove = buffer->head;
        prevOfToRemove = NULL;

        while ( scan != NULL ) {
            if ( object_getPriority( scan->dato ) > object_getPriority( toRemove->dato ) ) {
                toRemove = scan;
                prevOfToRemove = scanPrev;
            }
            scanPrev = scan;
            scan = scan->next;
        }
    }

    if ( prevOfToRemove == NULL ) {
        buffer->head = toRemove->next;
    } else {
        prevOfToRemove->next = toRemove->next;
    }
    if ( toRemove == buffer->tail ) {
        buffer->tail = prevOfToRemove;
    }

    result = toRemove->dato;
    free( toRemove );
    buffer->counter--;

    return result;
}

/* ------------------------------------------------------------------ */
/*  STAMPA                                                              */
/* ------------------------------------------------------------------ */

void buffer_print( const buffer_t *buffer )
{
    const idNode_t *idCur;
    bufferObj_t *objCur;

    if ( buffer == NULL ) {
        printf( "buffer_print: buffer NULL\n" );
        return;
    }

    printf( "Buffer[ID=%s, capacity=%d, counter=%d]\n",
            buffer->ID, buffer->capacity, buffer->counter );

    printf( "  input: " );
    idCur = buffer->inputList;
    if ( idCur == NULL ) {
        printf( "(nessuno)" );
    }
    while ( idCur != NULL ) {
        printf( "%s ", idCur->ID );
        idCur = idCur->next;
    }
    printf( "\n" );

    printf( "  output: " );
    idCur = buffer->outputList;
    if ( idCur == NULL ) {
        printf( "(nessuno)" );
    }
    while ( idCur != NULL ) {
        printf( "%s ", idCur->ID );
        idCur = idCur->next;
    }
    printf( "\n" );

    objCur = buffer->head;
    while ( objCur != NULL ) {
        object_print( objCur->dato );
        objCur = objCur->next;
    }
}
