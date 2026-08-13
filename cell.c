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
 * @brief Nodo della lista interna dei nastri posseduti dalla cella.
 *
 * Stesso scopo di bufferListNode_t, ma per nastro_t.
 */
typedef struct nastroListNode {
    nastro_t *nastro;
    struct nastroListNode *next;
} nastroListNode_t;

/**
 * @brief Definizione completa di cell_t (nascosta a chi include cell.h).
 *
 * Quando esisteranno machine_t/isp_t, andranno aggiunte qui liste
 * analoghe (machineListNode_t *machines, ecc.), seguendo lo stesso
 * schema di bufferListNode_t/nastroListNode_t.
 */
struct cell {
    bufferListNode_t *buffers;
    nastroListNode_t *nastri;
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
    cell->nastri = NULL;

    return cell;
}

void cell_destroy( cell_t *cell )
{
    bufferListNode_t *curB;
    bufferListNode_t *nextB;
    nastroListNode_t *curN;
    nastroListNode_t *nextN;

    if ( cell == NULL ) {
        return;
    }

    curB = cell->buffers;
    while ( curB != NULL ) {
        nextB = curB->next;
        registry_remove( buffer_getID( curB->buffer ) );
        buffer_delete( curB->buffer );
        free( curB );
        curB = nextB;
    }

    curN = cell->nastri;
    while ( curN != NULL ) {
        nextN = curN->next;
        registry_remove( nastro_getID( curN->nastro ) );
        nastro_delete( curN->nastro );
        free( curN );
        curN = nextN;
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

nastro_t *cell_addNastro( cell_t *cell, const char *ID, int capacity, int velocita, short int *errCode )
{
    nastro_t *nas;
    nastroListNode_t *node;
    short int localErr;

    if ( cell == NULL ) {
        if ( errCode != NULL ) {
            *errCode = ERR_NULL_PTR;
        }
        return NULL;
    }

    nas = nastro_create( ID, capacity, velocita, &localErr );
    if ( nas == NULL ) {
        if ( errCode != NULL ) {
            *errCode = localErr;
        }
        return NULL;
    }

    localErr = registry_add( ID, ENTITY_NASTRO, nas );
    if ( localErr != OP_SUCCESS ) {
        nastro_delete( nas );
        if ( errCode != NULL ) {
            *errCode = localErr;
        }
        return NULL;
    }

    node = malloc( sizeof( nastroListNode_t ) );
    if ( node == NULL ) {
        registry_remove( ID );
        nastro_delete( nas );
        if ( errCode != NULL ) {
            *errCode = ERR_ALLOC;
        }
        return NULL;
    }

    node->nastro = nas;
    node->next = cell->nastri;
    cell->nastri = node;

    if ( errCode != NULL ) {
        *errCode = OP_SUCCESS;
    }

    return nas;
}

/* ------------------------------------------------------------------ */
/*  COLLEGAMENTI                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Aggiunge outID alla lista di output dell'entità ID, qualunque
 *        sia il suo tipo (buffer o nastro).
 *
 * Punto unico da estendere quando arriveranno machine_t/isp_t: basta
 * aggiungere un case in più, seguendo lo stesso schema.
 */
static short int dispatch_addOutput( entity_type_t type, const char *ID, const char *outID )
{
    switch ( type ) {
        case ENTITY_BUFFER:
            return buffer_addOutput( registry_getBuffer( ID ), outID );
        case ENTITY_NASTRO:
            return nastro_addOutput( registry_getNastro( ID ), outID );
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * @brief Aggiunge inID alla lista di input dell'entità ID, qualunque
 *        sia il suo tipo (buffer o nastro). Speculare a dispatch_addOutput.
 */
static short int dispatch_addInput( entity_type_t type, const char *ID, const char *inID )
{
    switch ( type ) {
        case ENTITY_BUFFER:
            return buffer_addInput( registry_getBuffer( ID ), inID );
        case ENTITY_NASTRO:
            return nastro_addInput( registry_getNastro( ID ), inID );
        default:
            return ERR_NOT_SUPPORTED;
    }
}

short int cell_connect( cell_t *cell, const char *fromID, const char *toID )
{
    entity_type_t fromType;
    entity_type_t toType;
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

    /* Validare ENTRAMBI i tipi prima di mutare qualunque cosa: se uno
     * dei due non è supportato, non vogliamo un collegamento a metà
     * (es. output aggiunto a fromID ma nessun input aggiunto a toID). */
    if ( fromType != ENTITY_BUFFER && fromType != ENTITY_NASTRO ) {
        return ERR_NOT_SUPPORTED;
    }
    if ( toType != ENTITY_BUFFER && toType != ENTITY_NASTRO ) {
        return ERR_NOT_SUPPORTED;
    }

    result = dispatch_addOutput( fromType, fromID, toID );
    if ( result != OP_SUCCESS ) {
        return result;
    }

    result = dispatch_addInput( toType, toID, fromID );
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

nastro_t *cell_getNastro( const cell_t *cell, const char *ID )
{
    if ( cell == NULL ) {
        return NULL;
    }
    return registry_getNastro( ID );
}

short int cell_attachSensor( cell_t *cell, const char *targetID, const char *sensorID )
{
    entity_type_t targetType;
    entity_type_t sensorType;

    if ( cell == NULL || targetID == NULL || sensorID == NULL ) {
        return ERR_NULL_PTR;
    }

    if ( registry_getType( targetID, &targetType ) != OP_SUCCESS ) {
        return ERR_NOT_FOUND;
    }
    if ( registry_getType( sensorID, &sensorType ) != OP_SUCCESS ) {
        return ERR_NOT_FOUND;
    }
    if ( sensorType != ENTITY_SENSOR_BUFFER && sensorType != ENTITY_SENSOR_PRESENZA && sensorType != ENTITY_SENSOR_QUALITA ) {
        return ERR_NOT_SUPPORTED;
    }

    /* Quando machine_t/isp_t avranno le proprie sensorList/addSensor
     * (stesso schema di buffer.h/nastro.h), aggiungere qui i
     * rispettivi case, seguendo lo stesso schema: risolvere il
     * puntatore giusto con registry_get* e chiamare la corrispondente
     * _addSensor. */
    switch ( targetType ) {
        case ENTITY_BUFFER:
            return buffer_addSensor( registry_getBuffer( targetID ), sensorID );
        case ENTITY_NASTRO:
            return nastro_addSensor( registry_getNastro( targetID ), sensorID );
        default:
            return ERR_NOT_SUPPORTED;
    }
}

short int cell_attachActuator( cell_t *cell, const char *targetID, const char *actuatorID )
{
    entity_type_t targetType;
    entity_type_t actuatorType;

    if ( cell == NULL || targetID == NULL || actuatorID == NULL ) {
        return ERR_NULL_PTR;
    }

    if ( registry_getType( targetID, &targetType ) != OP_SUCCESS ) {
        return ERR_NOT_FOUND;
    }
    if ( registry_getType( actuatorID, &actuatorType ) != OP_SUCCESS ) {
        return ERR_NOT_FOUND;
    }
    if ( actuatorType != ENTITY_ACTUATOR_MOTORE && actuatorType != ENTITY_ACTUATOR_DEVIATORE ) {
        return ERR_NOT_SUPPORTED;
    }

    /* Stesso discorso di cell_attachSensor. */
    switch ( targetType ) {
        case ENTITY_BUFFER:
            return buffer_addActuator( registry_getBuffer( targetID ), actuatorID );
        case ENTITY_NASTRO:
            return nastro_addActuator( registry_getNastro( targetID ), actuatorID );
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * @brief Regola: ogni buffer con 2+ uscite deve avere almeno un
 *        deviatore tra i suoi attuatori collegati.
 *
 * Funzione a sé: aggiungere una nuova regola in futuro significa
 * scrivere una nuova funzione simile a questa e una riga in più in
 * cell_validateAll, senza toccare questa.
 */
static short int rule_bufferHasDeviatoreIfBranch( const cell_t *cell )
{
    bufferListNode_t *cur;
    int i;
    int n;
    bool trovato;
    entity_type_t type;
    char actID[IDLENGTH];

    cur = cell->buffers;
    while ( cur != NULL ) {
        if ( buffer_getOutputCount( cur->buffer ) >= 2 ) {
            trovato = false;
            n = buffer_getActuatorCount( cur->buffer );
            for ( i = 0; i < n; i++ ) {
                if ( buffer_getActuatorAt( cur->buffer, i, actID ) != OP_SUCCESS ) {
                    continue;
                }
                if ( registry_getType( actID, &type ) == OP_SUCCESS && type == ENTITY_ACTUATOR_DEVIATORE ) {
                    trovato = true;
                    break;
                }
            }
            if ( !trovato ) {
                return ERR_NOT_SUPPORTED;
            }
        }
        cur = cur->next;
    }

    return OP_SUCCESS;
}

/**
 * @brief Stessa regola di rule_bufferHasDeviatoreIfBranch, ma per i nastri.
 *
 * Tenuta separata (non fusa con quella dei buffer) perché ogni regola
 * riguarda un solo tipo di entità: se un domani i nastri smettono di
 * poter avere branch, si tocca solo questa funzione.
 */
static short int rule_nastroHasDeviatoreIfBranch( const cell_t *cell )
{
    nastroListNode_t *cur;
    int i;
    int n;
    bool trovato;
    entity_type_t type;
    char actID[IDLENGTH];

    cur = cell->nastri;
    while ( cur != NULL ) {
        if ( nastro_getOutputCount( cur->nastro ) >= 2 ) {
            trovato = false;
            n = nastro_getActuatorCount( cur->nastro );
            for ( i = 0; i < n; i++ ) {
                if ( nastro_getActuatorAt( cur->nastro, i, actID ) != OP_SUCCESS ) {
                    continue;
                }
                if ( registry_getType( actID, &type ) == OP_SUCCESS && type == ENTITY_ACTUATOR_DEVIATORE ) {
                    trovato = true;
                    break;
                }
            }
            if ( !trovato ) {
                return ERR_NOT_SUPPORTED;
            }
        }
        cur = cur->next;
    }

    return OP_SUCCESS;
}

short int cell_validateAll( const cell_t *cell )
{
    short int r;

    if ( cell == NULL ) {
        return ERR_NULL_PTR;
    }

    r = rule_bufferHasDeviatoreIfBranch( cell );
    if ( r != OP_SUCCESS ) {
        return r;
    }

    r = rule_nastroHasDeviatoreIfBranch( cell );
    if ( r != OP_SUCCESS ) {
        return r;
    }

    /* Nuove regole future: aggiungere qui una funzione rule_XYZ() e
     * una chiamata in più, senza toccare le regole già presenti. */

    return OP_SUCCESS;
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

short int cell_removeNastro( cell_t *cell, const char *ID )
{
    nastroListNode_t *cur;
    nastroListNode_t *prev;

    if ( cell == NULL || ID == NULL ) {
        return ERR_NULL_PTR;
    }

    prev = NULL;
    cur = cell->nastri;
    while ( cur != NULL && strcmp( nastro_getID( cur->nastro ), ID ) != 0 ) {
        prev = cur;
        cur = cur->next;
    }

    if ( cur == NULL ) {
        return ERR_NOT_FOUND;
    }

    if ( prev == NULL ) {
        cell->nastri = cur->next;
    } else {
        prev->next = cur->next;
    }

    registry_remove( ID );
    nastro_delete( cur->nastro );
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

int cell_getNastroCount( const cell_t *cell )
{
    nastroListNode_t *cur;
    int count;

    if ( cell == NULL ) {
        return ERR_NULL_PTR;
    }

    count = 0;
    cur = cell->nastri;
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

bool cell_hasNastro( const cell_t *cell, const char *ID )
{
    nastroListNode_t *cur;

    if ( cell == NULL || ID == NULL ) {
        return false;
    }

    cur = cell->nastri;
    while ( cur != NULL ) {
        if ( strcmp( nastro_getID( cur->nastro ), ID ) == 0 ) {
            return true;
        }
        cur = cur->next;
    }

    return false;
}

void cell_print( const cell_t *cell )
{
    bufferListNode_t *curB;
    nastroListNode_t *curN;

    if ( cell == NULL ) {
        printf( "cell_print: cella NULL\n" );
        return;
    }

    curB = cell->buffers;
    while ( curB != NULL ) {
        buffer_print( curB->buffer );
        curB = curB->next;
    }

    curN = cell->nastri;
    while ( curN != NULL ) {
        nastro_print( curN->nastro );
        curN = curN->next;
    }
}
