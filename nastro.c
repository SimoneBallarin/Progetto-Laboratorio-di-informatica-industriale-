/**
 * @file nastro.c
 * @brief Implementazione del prototipo del nastro trasportatore.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nastro.h"

/* ------------------------------------------------------------------ */
/*  CREAZIONE / DISTRUZIONE                                            */
/* ------------------------------------------------------------------ */

nastro_t *nastro_create( const char *ID, int capacity, int velocita, short int *errCode )
{
    nastro_t *n;

    if ( ID == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_NULL_PTR; }
        return NULL;
    }
    if ( strlen( ID ) == 0 || strlen( ID ) >= IDLENGTH ) {
        if ( errCode != NULL ) { *errCode = ERR_ID_INVALID; }
        return NULL;
    }
    if ( capacity <= 0 || velocita <= 0 ) {
        if ( errCode != NULL ) { *errCode = ERR_OUT_OF_RANGE; }
        return NULL;
    }

    n = malloc( sizeof( nastro_t ) );
    if ( n == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_ALLOC; }
        return NULL;
    }

    strncpy( n->ID, ID, IDLENGTH - 1 );
    n->ID[IDLENGTH - 1] = '\0';

    n->capacity = capacity;
    n->counter = 0;
    n->velocita = velocita;
    n->head = NULL;
    n->tail = NULL;
    n->inputList = NULL;
    n->outputList = NULL;
    n->sensorList = NULL;
    n->actuatorList = NULL;

    if ( errCode != NULL ) { *errCode = OP_SUCCESS; }

    return n;
}

void nastro_delete( nastro_t *n )
{
    nastroObj_t *cur;
    nastroObj_t *next;

    if ( n == NULL ) {
        return;
    }

    /* Come buffer_delete: libera solo i nodi wrapper, non gli object_t
     * contenuti (restano di proprietà di chi li ha creati). */
    cur = n->head;
    while ( cur != NULL ) {
        next = cur->next;
        free( cur );
        cur = next;
    }

    idlist_free( n->inputList );
    idlist_free( n->outputList );
    idlist_free( n->sensorList );
    idlist_free( n->actuatorList );

    free( n );
}

/* ------------------------------------------------------------------ */
/*  STATO                                                               */
/* ------------------------------------------------------------------ */

bool nastro_isFull( const nastro_t *n )
{
    if ( n == NULL ) {
        return false;
    }
    return n->counter >= n->capacity;
}

bool nastro_isEmpty( const nastro_t *n )
{
    if ( n == NULL ) {
        return true;
    }
    return n->counter == 0;
}

int nastro_getCount( const nastro_t *n )
{
    if ( n == NULL ) {
        return ERR_NULL_PTR;
    }
    return n->counter;
}

const char *nastro_getID( const nastro_t *n )
{
    if ( n == NULL ) {
        return NULL;
    }
    return n->ID;
}

int nastro_getCapacity( const nastro_t *n )
{
    if ( n == NULL ) {
        return ERR_NULL_PTR;
    }
    return n->capacity;
}

int nastro_getVelocita( const nastro_t *n )
{
    if ( n == NULL ) {
        return ERR_NULL_PTR;
    }
    return n->velocita;
}

short int nastro_setVelocita( nastro_t *n, int velocita )
{
    if ( n == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( velocita <= 0 ) {
        return ERR_OUT_OF_RANGE;
    }

    n->velocita = velocita;

    return OP_SUCCESS;
}

/* ------------------------------------------------------------------ */
/*  INGRESSI / USCITE / SENSORI / ATTUATORI (stesso schema di buffer.c) */
/* ------------------------------------------------------------------ */

short int nastro_addInput( nastro_t *n, const char *ID )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &n->inputList, ID );
}

short int nastro_addOutput( nastro_t *n, const char *ID )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &n->outputList, ID );
}

int nastro_getInputCount( const nastro_t *n )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( n->inputList );
}

short int nastro_getInputAt( const nastro_t *n, int index, char outID[IDLENGTH] )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( n->inputList, index, outID );
}

bool nastro_hasInput( const nastro_t *n, const char *ID )
{
    if ( n == NULL ) { return false; }
    return idlist_contains( n->inputList, ID );
}

int nastro_getOutputCount( const nastro_t *n )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( n->outputList );
}

short int nastro_getOutputAt( const nastro_t *n, int index, char outID[IDLENGTH] )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( n->outputList, index, outID );
}

bool nastro_hasOutput( const nastro_t *n, const char *ID )
{
    if ( n == NULL ) { return false; }
    return idlist_contains( n->outputList, ID );
}

short int nastro_addSensor( nastro_t *n, const char *ID )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &n->sensorList, ID );
}

int nastro_getSensorCount( const nastro_t *n )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( n->sensorList );
}

short int nastro_getSensorAt( const nastro_t *n, int index, char outID[IDLENGTH] )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( n->sensorList, index, outID );
}

bool nastro_hasSensor( const nastro_t *n, const char *ID )
{
    if ( n == NULL ) { return false; }
    return idlist_contains( n->sensorList, ID );
}

short int nastro_addActuator( nastro_t *n, const char *ID )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &n->actuatorList, ID );
}

int nastro_getActuatorCount( const nastro_t *n )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( n->actuatorList );
}

short int nastro_getActuatorAt( const nastro_t *n, int index, char outID[IDLENGTH] )
{
    if ( n == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( n->actuatorList, index, outID );
}

bool nastro_hasActuator( const nastro_t *n, const char *ID )
{
    if ( n == NULL ) { return false; }
    return idlist_contains( n->actuatorList, ID );
}

/* ------------------------------------------------------------------ */
/*  TRASPORTO OGGETTI                                                   */
/* ------------------------------------------------------------------ */

int nastro_insertObject( nastro_t *n, object_t *object, int step_corrente )
{
    nastroObj_t *node;

    if ( n == NULL || object == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( nastro_isFull( n ) ) {
        return ERR_FULL;
    }

    node = malloc( sizeof( nastroObj_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }
    node->dato = object;
    node->step_ingresso = step_corrente;
    node->next = NULL;

    /* Sempre in coda (FIFO): il nastro trasporta, non decide priorità. */
    if ( n->head == NULL ) {
        n->head = node;
        n->tail = node;
    } else {
        n->tail->next = node;
        n->tail = node;
    }

    n->counter++;

    return OP_SUCCESS;
}

bool nastro_isReady( const nastro_t *n, int step_corrente )
{
    if ( n == NULL || n->head == NULL ) {
        return false;
    }
    return ( step_corrente - n->head->step_ingresso ) >= n->velocita;
}

object_t *nastro_removeReadyObject( nastro_t *n, int step_corrente )
{
    object_t *result;
    nastroObj_t *toRemove;

    if ( n == NULL || n->head == NULL ) {
        return NULL;
    }

    /* L'oggetto in testa e' il primo entrato: se lui non e' ancora
     * pronto, nessun altro dietro di lui puo' esserlo (sono entrati
     * dopo, quindi finiranno dopo). */
    if ( step_corrente - n->head->step_ingresso < n->velocita ) {
        return NULL;
    }

    toRemove = n->head;
    n->head = toRemove->next;
    if ( n->head == NULL ) {
        n->tail = NULL;
    }

    result = toRemove->dato;
    free( toRemove );
    n->counter--;

    return result;
}

/* ------------------------------------------------------------------ */
/*  STAMPA                                                              */
/* ------------------------------------------------------------------ */

void nastro_print( const nastro_t *n )
{
    const idNode_t *idCur;
    nastroObj_t *objCur;

    if ( n == NULL ) {
        printf( "nastro_print: nastro NULL\n" );
        return;
    }

    printf( "Nastro[ID=%s, capacity=%d, counter=%d, velocita=%d]\n",
            n->ID, n->capacity, n->counter, n->velocita );

    printf( "  input: " );
    idCur = n->inputList;
    if ( idCur == NULL ) { printf( "(nessuno)" ); }
    while ( idCur != NULL ) { printf( "%s ", idCur->ID ); idCur = idCur->next; }
    printf( "\n" );

    printf( "  output: " );
    idCur = n->outputList;
    if ( idCur == NULL ) { printf( "(nessuno)" ); }
    while ( idCur != NULL ) { printf( "%s ", idCur->ID ); idCur = idCur->next; }
    printf( "\n" );

    objCur = n->head;
    while ( objCur != NULL ) {
        printf( "  [ingresso@%d] ", objCur->step_ingresso );
        object_print( objCur->dato );
        objCur = objCur->next;
    }
}
