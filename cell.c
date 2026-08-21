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
 * @brief Nodo della lista interna delle macchine possedute dalla cella.
 *
 * Stesso scopo di bufferListNode_t, ma per machine_t.
 */
typedef struct machineListNode {
    machine_t *machine;
    struct machineListNode *next;
} machineListNode_t;

/**
 * @brief Nodo della lista interna delle ISP possedute dalla cella.
 *
 * Stesso scopo di bufferListNode_t, ma per isp_t.
 */
typedef struct ispListNode {
    isp_t *isp;
    struct ispListNode *next;
} ispListNode_t;

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
 */
struct cell {
    bufferListNode_t *buffers;
    machineListNode_t *machines;
    ispListNode_t *isps;
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
    cell->machines = NULL;
    cell->isps = NULL;
    cell->nastri = NULL;

    return cell;
}

void cell_destroy( cell_t *cell )
{
    bufferListNode_t *curB;
    bufferListNode_t *nextB;
    machineListNode_t *curM;
    machineListNode_t *nextM;
    ispListNode_t *curI;
    ispListNode_t *nextI;
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

    curM = cell->machines;
    while ( curM != NULL ) {
        nextM = curM->next;
        registry_remove( machine_getID( curM->machine ) );
        machine_delete( curM->machine );
        free( curM );
        curM = nextM;
    }

    curI = cell->isps;
    while ( curI != NULL ) {
        nextI = curI->next;
        registry_remove( isp_getID( curI->isp ) );
        isp_delete( curI->isp );
        free( curI );
        curI = nextI;
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

machine_t *cell_addMachine( cell_t *cell, const char *ID, int tempo_lavorazione, short int *errCode )
{
    machine_t *m;
    machineListNode_t *node;
    short int localErr;

    if ( cell == NULL ) {
        if ( errCode != NULL ) {
            *errCode = ERR_NULL_PTR;
        }
        return NULL;
    }

    m = machine_create( ID, tempo_lavorazione, &localErr );
    if ( m == NULL ) {
        if ( errCode != NULL ) {
            *errCode = localErr;
        }
        return NULL;
    }

    localErr = registry_add( ID, ENTITY_MACHINE, m );
    if ( localErr != OP_SUCCESS ) {
        machine_delete( m );
        if ( errCode != NULL ) {
            *errCode = localErr;
        }
        return NULL;
    }

    node = malloc( sizeof( machineListNode_t ) );
    if ( node == NULL ) {
        registry_remove( ID );
        machine_delete( m );
        if ( errCode != NULL ) {
            *errCode = ERR_ALLOC;
        }
        return NULL;
    }

    node->machine = m;
    node->next = cell->machines;
    cell->machines = node;

    if ( errCode != NULL ) {
        *errCode = OP_SUCCESS;
    }

    return m;
}

isp_t *cell_addISP( cell_t *cell, const char *ID, int tempo_controllo, short int *errCode )
{
    isp_t *i;
    ispListNode_t *node;
    short int localErr;

    if ( cell == NULL ) {
        if ( errCode != NULL ) {
            *errCode = ERR_NULL_PTR;
        }
        return NULL;
    }

    i = isp_create( ID, tempo_controllo, &localErr );
    if ( i == NULL ) {
        if ( errCode != NULL ) {
            *errCode = localErr;
        }
        return NULL;
    }

    localErr = registry_add( ID, ENTITY_ISP, i );
    if ( localErr != OP_SUCCESS ) {
        isp_delete( i );
        if ( errCode != NULL ) {
            *errCode = localErr;
        }
        return NULL;
    }

    node = malloc( sizeof( ispListNode_t ) );
    if ( node == NULL ) {
        registry_remove( ID );
        isp_delete( i );
        if ( errCode != NULL ) {
            *errCode = ERR_ALLOC;
        }
        return NULL;
    }

    node->isp = i;
    node->next = cell->isps;
    cell->isps = node;

    if ( errCode != NULL ) {
        *errCode = OP_SUCCESS;
    }

    return i;
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
 *        sia il suo tipo (buffer, machine, isp o nastro).
 *
 * Punto unico da estendere per nuovi tipi di entità: basta aggiungere
 * un case in più, seguendo lo stesso schema.
 */
static short int dispatch_addOutput( entity_type_t type, const char *ID, const char *outID )
{
    switch ( type ) {
        case ENTITY_BUFFER:
            return buffer_addOutput( registry_getBuffer( ID ), outID );
        case ENTITY_MACHINE:
            return machine_addOutput( registry_getMachine( ID ), outID );
        case ENTITY_ISP:
            return isp_addOutput( registry_getISP( ID ), outID );
        case ENTITY_NASTRO:
            return nastro_addOutput( registry_getNastro( ID ), outID );
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * @brief Aggiunge inID alla lista di input dell'entità ID, qualunque
 *        sia il suo tipo. Speculare a dispatch_addOutput.
 */
static short int dispatch_addInput( entity_type_t type, const char *ID, const char *inID )
{
    switch ( type ) {
        case ENTITY_BUFFER:
            return buffer_addInput( registry_getBuffer( ID ), inID );
        case ENTITY_MACHINE:
            return machine_addInput( registry_getMachine( ID ), inID );
        case ENTITY_ISP:
            return isp_addInput( registry_getISP( ID ), inID );
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
    if ( fromType != ENTITY_BUFFER && fromType != ENTITY_MACHINE &&
         fromType != ENTITY_ISP && fromType != ENTITY_NASTRO ) {
        return ERR_NOT_SUPPORTED;
    }
    if ( toType != ENTITY_BUFFER && toType != ENTITY_MACHINE &&
         toType != ENTITY_ISP && toType != ENTITY_NASTRO ) {
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

machine_t *cell_getMachine( const cell_t *cell, const char *ID )
{
    if ( cell == NULL ) {
        return NULL;
    }
    return registry_getMachine( ID );
}

isp_t *cell_getISP( const cell_t *cell, const char *ID )
{
    if ( cell == NULL ) {
        return NULL;
    }
    return registry_getISP( ID );
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

    switch ( targetType ) {
        case ENTITY_BUFFER:
            return buffer_addSensor( registry_getBuffer( targetID ), sensorID );
        case ENTITY_MACHINE:
            return machine_addSensor( registry_getMachine( targetID ), sensorID );
        case ENTITY_ISP:
            return isp_addSensor( registry_getISP( targetID ), sensorID );
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

    switch ( targetType ) {
        case ENTITY_BUFFER:
            return buffer_addActuator( registry_getBuffer( targetID ), actuatorID );
        case ENTITY_MACHINE:
            return machine_addActuator( registry_getMachine( targetID ), actuatorID );
        case ENTITY_ISP:
            return isp_addActuator( registry_getISP( targetID ), actuatorID );
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
 * @brief Stessa regola di rule_bufferHasDeviatoreIfBranch, ma per le macchine.
 */
static short int rule_machineHasDeviatoreIfBranch( const cell_t *cell )
{
    machineListNode_t *cur;
    int i;
    int n;
    bool trovato;
    entity_type_t type;
    char actID[IDLENGTH];

    cur = cell->machines;
    while ( cur != NULL ) {
        if ( machine_getOutputCount( cur->machine ) >= 2 ) {
            trovato = false;
            n = machine_getActuatorCount( cur->machine );
            for ( i = 0; i < n; i++ ) {
                if ( machine_getActuatorAt( cur->machine, i, actID ) != OP_SUCCESS ) {
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
 * @brief Stessa regola di rule_bufferHasDeviatoreIfBranch, ma per le ISP.
 *
 * È il caso più comune in pratica: la ISP è tipicamente l'unico punto
 * della cella con più uscite (conforme/rivalutazione/scarto).
 */
static short int rule_ispHasDeviatoreIfBranch( const cell_t *cell )
{
    ispListNode_t *cur;
    int i;
    int n;
    bool trovato;
    entity_type_t type;
    char actID[IDLENGTH];

    cur = cell->isps;
    while ( cur != NULL ) {
        if ( isp_getOutputCount( cur->isp ) >= 2 ) {
            trovato = false;
            n = isp_getActuatorCount( cur->isp );
            for ( i = 0; i < n; i++ ) {
                if ( isp_getActuatorAt( cur->isp, i, actID ) != OP_SUCCESS ) {
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
 * Tenuta separata (non fusa con le altre) perché ogni regola riguarda
 * un solo tipo di entità: se un domani un tipo smette di poter avere
 * branch, si tocca solo la sua funzione.
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

    r = rule_machineHasDeviatoreIfBranch( cell );
    if ( r != OP_SUCCESS ) {
        return r;
    }

    r = rule_ispHasDeviatoreIfBranch( cell );
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

short int cell_removeMachine( cell_t *cell, const char *ID )
{
    machineListNode_t *cur;
    machineListNode_t *prev;

    if ( cell == NULL || ID == NULL ) {
        return ERR_NULL_PTR;
    }

    prev = NULL;
    cur = cell->machines;
    while ( cur != NULL && strcmp( machine_getID( cur->machine ), ID ) != 0 ) {
        prev = cur;
        cur = cur->next;
    }

    if ( cur == NULL ) {
        return ERR_NOT_FOUND;
    }

    if ( prev == NULL ) {
        cell->machines = cur->next;
    } else {
        prev->next = cur->next;
    }

    registry_remove( ID );
    machine_delete( cur->machine );
    free( cur );

    return OP_SUCCESS;
}

short int cell_removeISP( cell_t *cell, const char *ID )
{
    ispListNode_t *cur;
    ispListNode_t *prev;

    if ( cell == NULL || ID == NULL ) {
        return ERR_NULL_PTR;
    }

    prev = NULL;
    cur = cell->isps;
    while ( cur != NULL && strcmp( isp_getID( cur->isp ), ID ) != 0 ) {
        prev = cur;
        cur = cur->next;
    }

    if ( cur == NULL ) {
        return ERR_NOT_FOUND;
    }

    if ( prev == NULL ) {
        cell->isps = cur->next;
    } else {
        prev->next = cur->next;
    }

    registry_remove( ID );
    isp_delete( cur->isp );
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

int cell_getMachineCount( const cell_t *cell )
{
    machineListNode_t *cur;
    int count;

    if ( cell == NULL ) {
        return ERR_NULL_PTR;
    }

    count = 0;
    cur = cell->machines;
    while ( cur != NULL ) {
        count++;
        cur = cur->next;
    }

    return count;
}

int cell_getISPCount( const cell_t *cell )
{
    ispListNode_t *cur;
    int count;

    if ( cell == NULL ) {
        return ERR_NULL_PTR;
    }

    count = 0;
    cur = cell->isps;
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

bool cell_hasMachine( const cell_t *cell, const char *ID )
{
    machineListNode_t *cur;

    if ( cell == NULL || ID == NULL ) {
        return false;
    }

    cur = cell->machines;
    while ( cur != NULL ) {
        if ( strcmp( machine_getID( cur->machine ), ID ) == 0 ) {
            return true;
        }
        cur = cur->next;
    }

    return false;
}

bool cell_hasISP( const cell_t *cell, const char *ID )
{
    ispListNode_t *cur;

    if ( cell == NULL || ID == NULL ) {
        return false;
    }

    cur = cell->isps;
    while ( cur != NULL ) {
        if ( strcmp( isp_getID( cur->isp ), ID ) == 0 ) {
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

short int cell_getBufferIDAt( const cell_t *cell, int index, char outID[IDLENGTH] )
{
    bufferListNode_t *cur;
    int i;

    if ( cell == NULL || outID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( index < 0 ) {
        return ERR_OUT_OF_RANGE;
    }

    i = 0;
    cur = cell->buffers;
    while ( cur != NULL ) {
        if ( i == index ) {
            strncpy( outID, buffer_getID( cur->buffer ), IDLENGTH - 1 );
            outID[IDLENGTH - 1] = '\0';
            return OP_SUCCESS;
        }
        i++;
        cur = cur->next;
    }

    return ERR_NOT_FOUND;
}

short int cell_getMachineIDAt( const cell_t *cell, int index, char outID[IDLENGTH] )
{
    machineListNode_t *cur;
    int i;

    if ( cell == NULL || outID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( index < 0 ) {
        return ERR_OUT_OF_RANGE;
    }

    i = 0;
    cur = cell->machines;
    while ( cur != NULL ) {
        if ( i == index ) {
            strncpy( outID, machine_getID( cur->machine ), IDLENGTH - 1 );
            outID[IDLENGTH - 1] = '\0';
            return OP_SUCCESS;
        }
        i++;
        cur = cur->next;
    }

    return ERR_NOT_FOUND;
}

short int cell_getISPIDAt( const cell_t *cell, int index, char outID[IDLENGTH] )
{
    ispListNode_t *cur;
    int i;

    if ( cell == NULL || outID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( index < 0 ) {
        return ERR_OUT_OF_RANGE;
    }

    i = 0;
    cur = cell->isps;
    while ( cur != NULL ) {
        if ( i == index ) {
            strncpy( outID, isp_getID( cur->isp ), IDLENGTH - 1 );
            outID[IDLENGTH - 1] = '\0';
            return OP_SUCCESS;
        }
        i++;
        cur = cur->next;
    }

    return ERR_NOT_FOUND;
}

short int cell_getNastroIDAt( const cell_t *cell, int index, char outID[IDLENGTH] )
{
    nastroListNode_t *cur;
    int i;

    if ( cell == NULL || outID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( index < 0 ) {
        return ERR_OUT_OF_RANGE;
    }

    i = 0;
    cur = cell->nastri;
    while ( cur != NULL ) {
        if ( i == index ) {
            strncpy( outID, nastro_getID( cur->nastro ), IDLENGTH - 1 );
            outID[IDLENGTH - 1] = '\0';
            return OP_SUCCESS;
        }
        i++;
        cur = cur->next;
    }

    return ERR_NOT_FOUND;
}

void cell_print( const cell_t *cell )
{
    bufferListNode_t *curB;
    machineListNode_t *curM;
    ispListNode_t *curI;
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

    curM = cell->machines;
    while ( curM != NULL ) {
        machine_print( curM->machine );
        curM = curM->next;
    }

    curI = cell->isps;
    while ( curI != NULL ) {
        isp_print( curI->isp );
        curI = curI->next;
    }

    curN = cell->nastri;
    while ( curN != NULL ) {
        nastro_print( curN->nastro );
        curN = curN->next;
    }
}
