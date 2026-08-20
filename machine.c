/**
 * @file machine.c
 * @brief Implementazione della stazione di lavorazione M.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "machine.h"

/* Tolleranza di default (11%), usata da machine_create se non
 * sovrascritta con machine_setTolleranzaLavorazione. */
#define TOLLERANZA_LAVORAZIONE_DEFAULT 0.05

machine_t *machine_create( const char *ID, int tempo_lavorazione, short int *errCode )
{
    machine_t *m;

    if ( ID == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_NULL_PTR; }
        return NULL;
    }
    if ( strlen( ID ) == 0 || strlen( ID ) >= IDLENGTH ) {
        if ( errCode != NULL ) { *errCode = ERR_ID_INVALID; }
        return NULL;
    }
    if ( tempo_lavorazione <= 0 ) {
        if ( errCode != NULL ) { *errCode = ERR_OUT_OF_RANGE; }
        return NULL;
    }

    m = malloc( sizeof( machine_t ) );
    if ( m == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_ALLOC; }
        return NULL;
    }

    strncpy( m->ID, ID, IDLENGTH - 1 );
    m->ID[IDLENGTH - 1] = '\0';

    m->tempo_lavorazione = tempo_lavorazione;
    m->stato = MACCHINA_LIBERA;
    m->oggetto_in_lavorazione = NULL;
    m->step_inizio_lavorazione = 0;
    m->tolleranza_lavorazione = TOLLERANZA_LAVORAZIONE_DEFAULT;
    m->inputList = NULL;
    m->outputList = NULL;
    m->sensorList = NULL;
    m->actuatorList = NULL;

    if ( errCode != NULL ) { *errCode = OP_SUCCESS; }

    return m;
}

void machine_delete( machine_t *m )
{
    if ( m == NULL ) {
        return;
    }

    /* L'oggetto eventualmente in lavorazione NON viene liberato: resta
     * di proprieta' di chi lo ha creato (stesso principio di
     * buffer_delete/nastro_delete). */
    idlist_free( m->inputList );
    idlist_free( m->outputList );
    idlist_free( m->sensorList );
    idlist_free( m->actuatorList );

    free( m );
}

bool machine_isBusy( const machine_t *m )
{
    if ( m == NULL ) {
        return false;
    }
    return m->stato == MACCHINA_OCCUPATA;
}

bool machine_isReady( const machine_t *m, int step_corrente )
{
    if ( m == NULL || m->stato == MACCHINA_LIBERA ) {
        return false;
    }
    return ( step_corrente - m->step_inizio_lavorazione ) >= m->tempo_lavorazione;
}

const char *machine_getID( const machine_t *m )
{
    if ( m == NULL ) {
        return NULL;
    }
    return m->ID;
}

int machine_getTempoLavorazione( const machine_t *m )
{
    if ( m == NULL ) {
        return ERR_NULL_PTR;
    }
    return m->tempo_lavorazione;
}

short int machine_setTolleranzaLavorazione( machine_t *m, double tolleranza )
{
    if ( m == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( tolleranza < 0.0 ) {
        return ERR_OUT_OF_RANGE;
    }

    m->tolleranza_lavorazione = tolleranza;

    return OP_SUCCESS;
}

double machine_getTolleranzaLavorazione( const machine_t *m )
{
    if ( m == NULL ) {
        return -1.0;
    }
    return m->tolleranza_lavorazione;
}

short int machine_addInput( machine_t *m, const char *ID )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &m->inputList, ID );
}

short int machine_addOutput( machine_t *m, const char *ID )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &m->outputList, ID );
}

int machine_getInputCount( const machine_t *m )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( m->inputList );
}

short int machine_getInputAt( const machine_t *m, int index, char outID[IDLENGTH] )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( m->inputList, index, outID );
}

bool machine_hasInput( const machine_t *m, const char *ID )
{
    if ( m == NULL ) { return false; }
    return idlist_contains( m->inputList, ID );
}

int machine_getOutputCount( const machine_t *m )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( m->outputList );
}

short int machine_getOutputAt( const machine_t *m, int index, char outID[IDLENGTH] )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( m->outputList, index, outID );
}

bool machine_hasOutput( const machine_t *m, const char *ID )
{
    if ( m == NULL ) { return false; }
    return idlist_contains( m->outputList, ID );
}

short int machine_addSensor( machine_t *m, const char *ID )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &m->sensorList, ID );
}

int machine_getSensorCount( const machine_t *m )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( m->sensorList );
}

short int machine_getSensorAt( const machine_t *m, int index, char outID[IDLENGTH] )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( m->sensorList, index, outID );
}

bool machine_hasSensor( const machine_t *m, const char *ID )
{
    if ( m == NULL ) { return false; }
    return idlist_contains( m->sensorList, ID );
}

short int machine_addActuator( machine_t *m, const char *ID )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &m->actuatorList, ID );
}

int machine_getActuatorCount( const machine_t *m )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( m->actuatorList );
}

short int machine_getActuatorAt( const machine_t *m, int index, char outID[IDLENGTH] )
{
    if ( m == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( m->actuatorList, index, outID );
}

bool machine_hasActuator( const machine_t *m, const char *ID )
{
    if ( m == NULL ) { return false; }
    return idlist_contains( m->actuatorList, ID );
}

short int machine_admit( machine_t *m, object_t *object, int step_corrente )
{
    if ( m == NULL || object == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( m->stato == MACCHINA_OCCUPATA ) {
        return ERR_FULL;
    }

    m->oggetto_in_lavorazione = object;
    m->step_inizio_lavorazione = step_corrente;
    m->stato = MACCHINA_OCCUPATA;

    return OP_SUCCESS;
}

/**
 * @brief Restituisce un fattore moltiplicativo casuale in
 *        [1 - tolleranza, 1 + tolleranza], da applicare a un valore per
 *        simularne una lettura/lavorazione imprecisa.
 */
static double fattore_rumore( double tolleranza )
{
    double r; /* uniforme in [0.0, 1.0) */

    if ( tolleranza <= 0.0 ) {
        return 1.0;
    }

    r = (double) rand() / ( (double) RAND_MAX + 1.0 );

    /* scala r da [0,1) a [-tolleranza, +tolleranza], poi centra su 1.0 */
    return 1.0 + ( ( r * 2.0 * tolleranza ) - tolleranza );
}

object_t *machine_tryRelease( machine_t *m, int step_corrente )
{
    object_t *result;
    double nuova_dimensione;
    double nuovo_raggio;

    if ( m == NULL || m->stato == MACCHINA_LIBERA ) {
        return NULL;
    }
    if ( step_corrente - m->step_inizio_lavorazione < m->tempo_lavorazione ) {
        return NULL; /* lavorazione non ancora completata */
    }

    result = m->oggetto_in_lavorazione;
    int Dlavorato = 20;
    int Rlavorato = 4;
    /* Rumore indipendente su dimensionX e raggio: una lavorazione reale
     * non e' mai perfettamente precisa. I due fattori sono estratti
     * separatamente (non lo stesso rumore applicato a entrambi), cosi'
     * un pezzo puo' uscire con la dimensione fuori tolleranza ma il
     * raggio nella norma, o viceversa. */
    nuova_dimensione = (object_getDimensionX( result )-Dlavorato) * fattore_rumore( m->tolleranza_lavorazione );
    nuovo_raggio      = (object_getRaggio( result )-Rlavorato) * fattore_rumore( m->tolleranza_lavorazione );

    /* dimensione e raggio non hanno senso negativi (object_setRaggio
     * richiede >= 0): un rumore che li spingerebbe sotto zero viene
     * limitato a zero invece di essere lasciato passare. */
    if ( nuova_dimensione < 0.0 ) { nuova_dimensione = 0.0; }
    if ( nuovo_raggio < 0.0 )     { nuovo_raggio = 0.0; }

    object_setDimensionX( result, nuova_dimensione );
    object_setRaggio( result, nuovo_raggio );

    m->oggetto_in_lavorazione = NULL;
    m->stato = MACCHINA_LIBERA;

    return result;
}

void machine_print( const machine_t *m )
{
    const idNode_t *idCur;

    if ( m == NULL ) {
        printf( "machine_print: macchina NULL\n" );
        return;
    }

    printf( "Machine[ID=%s, tempo_lavorazione=%d, tolleranza=%.1f%%, stato=%s]\n",
            m->ID, m->tempo_lavorazione, m->tolleranza_lavorazione * 100.0,
            m->stato == MACCHINA_OCCUPATA ? "OCCUPATA" : "LIBERA" );

    printf( "  input: " );
    idCur = m->inputList;
    if ( idCur == NULL ) { printf( "(nessuno)" ); }
    while ( idCur != NULL ) { printf( "%s ", idCur->ID ); idCur = idCur->next; }
    printf( "\n" );

    printf( "  output: " );
    idCur = m->outputList;
    if ( idCur == NULL ) { printf( "(nessuno)" ); }
    while ( idCur != NULL ) { printf( "%s ", idCur->ID ); idCur = idCur->next; }
    printf( "\n" );

    if ( m->oggetto_in_lavorazione != NULL ) {
        printf( "  in lavorazione (dallo step %d): ", m->step_inizio_lavorazione );
        object_print( m->oggetto_in_lavorazione );
    }
}
