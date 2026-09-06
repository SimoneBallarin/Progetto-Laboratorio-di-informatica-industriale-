/**
 * @file object.c
 * @brief Implementazione delle funzioni sugli oggetti.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"

/* ------------------------------------------------------------------ */
/*  CREAZIONE / DISTRUZIONE                                            */
/* ------------------------------------------------------------------ */

object_t *object_create( const char *Obj_ID, short int Obj_priority, char Obj_type, int step, double Obj_dimensionX, double Obj_raggio, short int *errCode )
{
    object_t *obj;

    if ( Obj_ID == NULL ) {
        if ( errCode != NULL ) {
            *errCode = ERR_NULL_PTR;
        }
        return NULL;
    }
    if ( strlen( Obj_ID ) == 0 || strlen( Obj_ID ) >= IDLENGTH ) {
        if ( errCode != NULL ) {
            *errCode = ERR_ID_INVALID;
        }
        return NULL;
    }
    if ( Obj_priority < PRIORITY_MIN || Obj_priority > PRIORITY_MAX ) {
        if ( errCode != NULL ) {
            *errCode = ERR_OUT_OF_RANGE;
        }
        return NULL;
    }
    if ( Obj_dimensionX < 0.0 ) {
        if ( errCode != NULL ) {
            *errCode = ERR_OUT_OF_RANGE;
        }
        return NULL;
    }
    if ( Obj_raggio < 0.0 ) {
        if ( errCode != NULL ) {
            *errCode = ERR_OUT_OF_RANGE;
        }
        return NULL;
    }

    obj = malloc( sizeof( object_t ) );
    if ( obj == NULL ) {
        if ( errCode != NULL ) {
            *errCode = ERR_ALLOC;
        }
        return NULL;
    }

    strncpy( obj->ID, Obj_ID, IDLENGTH - 1 );
    obj->ID[IDLENGTH - 1] = '\0';

    obj->priority = Obj_priority;
    obj->type = Obj_type;

    /* Nessuna locazione assegnata alla creazione. */
    strncpy( obj->ID_LOCATION, ID_NONE, IDLENGTH - 1 );
    obj->ID_LOCATION[IDLENGTH - 1] = '\0';

    obj->stepCreation = step;
    obj->stepPartial = STEP_OUT_NONE;
    obj->stepOut = STEP_OUT_NONE;
    obj->dimensionX = Obj_dimensionX;
    obj->raggio = Obj_raggio;

    if ( errCode != NULL ) {
        *errCode = OP_SUCCESS;
    }

    return obj;
}

void object_delete( object_t *Obj )
{
    if ( Obj == NULL ) {
        return;
    }
    free( Obj );
}

/* ------------------------------------------------------------------ */
/*  SETTER                                                              */
/* ------------------------------------------------------------------ */

short int object_setLocation( object_t *Obj, const char *newID )
{
    if ( Obj == NULL || newID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( strlen( newID ) >= IDLENGTH ) {
        return ERR_ID_INVALID;
    }

    strncpy( Obj->ID_LOCATION, newID, IDLENGTH - 1 );
    Obj->ID_LOCATION[IDLENGTH - 1] = '\0';

    return OP_SUCCESS;
}

short int object_setStepOut( object_t *Obj, int step )
{
    if ( Obj == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( step < Obj->stepCreation ) {
        return ERR_OUT_OF_RANGE;
    }

    Obj->stepOut = step;

    return OP_SUCCESS;
}

short int object_setStepPartial( object_t *Obj, int step )
{
    if ( Obj == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( Obj->stepPartial != STEP_OUT_NONE ) {
        /* Gia' impostato in precedenza (l'oggetto e' entrato in una
         * stazione successiva della pipeline): non va sovrascritto,
         * vedi doc in object.h. */
        return ERR_DUPLICATE;
    }
    if ( step < Obj->stepCreation ) {
        return ERR_OUT_OF_RANGE;
    }

    Obj->stepPartial = step;

    return OP_SUCCESS;
}

short int object_setDimensionX( object_t *Obj, double newDimension )
{
    if ( Obj == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( newDimension < 0.0 ) {
        return ERR_OUT_OF_RANGE;
    }

    Obj->dimensionX = newDimension;

    return OP_SUCCESS;
}

short int object_setRaggio( object_t *Obj, double newRaggio )
{
    if ( Obj == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( newRaggio < 0.0 ) {
        return ERR_OUT_OF_RANGE;
    }

    Obj->raggio = newRaggio;

    return OP_SUCCESS;
}

/* ------------------------------------------------------------------ */
/*  GETTER                                                              */
/* ------------------------------------------------------------------ */

short int object_getPriority( const object_t *Obj )
{
    if ( Obj == NULL ) {
        /* La priorità di un oggetto valido è sempre >= PRIORITY_MIN (0),
         * quindi ERR_NULL_PTR (-1) è un valore di errore inequivocabile. */
        return ERR_NULL_PTR;
    }
    return Obj->priority;
}

const char *object_getLocation( const object_t *Obj )
{
    if ( Obj == NULL ) {
        return NULL;
    }
    /* Se non ancora assegnata, contiene la stringa sentinella ID_NONE. */
    return Obj->ID_LOCATION;
}

char object_getType( const object_t *Obj )
{
    if ( Obj == NULL ) {
        /* char non è un puntatore: non può essere NULL. '\0' non è mai
         * un tipo valido ('A'/'B'), quindi è la sentinella di errore. */
        return '\0';
    }
    return Obj->type;
}

const char *object_getID( const object_t *Obj )
{
    if ( Obj == NULL ) {
        return NULL;
    }
    return Obj->ID;
}

int object_getStepCreation( const object_t *Obj )
{
    if ( Obj == NULL ) {
        return ERR_NULL_PTR;
    }
    return Obj->stepCreation;
}

int object_getStepOut( const object_t *Obj )
{
    if ( Obj == NULL ) {
        return ERR_NULL_PTR;
    }
    return Obj->stepOut;
}

int object_getStepPartial( const object_t *Obj )
{
    if ( Obj == NULL ) {
        return ERR_NULL_PTR;
    }
    return Obj->stepPartial;
}

double object_getDimensionX( const object_t *Obj )
{
    if ( Obj == NULL ) {
        /* dimensionX è sempre >= 0 per un oggetto valido, quindi un
         * valore negativo è inequivocabilmente un errore. */
        return (double) ERR_NULL_PTR;
    }
    return Obj->dimensionX;
}

double object_getRaggio( const object_t *Obj )
{
    if ( Obj == NULL ) {
        /* stesso ragionamento di object_getDimensionX. */
        return (double) ERR_NULL_PTR;
    }
    return Obj->raggio;
}

/* ------------------------------------------------------------------ */
/*  STAMPA                                                              */
/* ------------------------------------------------------------------ */

void object_print( const object_t *Obj )
{
    if ( Obj == NULL ) {
        printf( "object_print: oggetto NULL\n" );
        return;
    }

    printf( "Object[ID=%s, priority=%d, type=%c, location=%s, stepCreation=%d, stepPartial=%d, stepOut=%d, dimensionX=%.2f, raggio=%.2f]\n",
            Obj->ID, Obj->priority, Obj->type, Obj->ID_LOCATION,
            Obj->stepCreation, Obj->stepPartial, Obj->stepOut, Obj->dimensionX, Obj->raggio );
}
