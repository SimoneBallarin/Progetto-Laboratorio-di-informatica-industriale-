/**
 * @file isp.c
 * @brief Implementazione della stazione di controllo qualita' ISP.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "isp.h"

isp_t *isp_create( const char *ID, int tempo_controllo, int dimensionX_target, int raggio_target, short int *errCode )
{
    isp_t *i;
    int localErr;

    if ( ID == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_NULL_PTR; }
        return NULL;
    }
    if ( strlen( ID ) == 0 || strlen( ID ) >= IDLENGTH ) {
        if ( errCode != NULL ) { *errCode = ERR_ID_INVALID; }
        return NULL;
    }
    if ( tempo_controllo <= 0 ) {
        if ( errCode != NULL ) { *errCode = ERR_OUT_OF_RANGE; }
        return NULL;
    }

    i = malloc( sizeof( isp_t ) );
    if ( i == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_ALLOC; }
        return NULL;
    }

    strncpy( i->ID, ID, IDLENGTH - 1 );
    i->ID[IDLENGTH - 1] = '\0';

    i->tempo_controllo = tempo_controllo;
    i->stato = ISP_LIBERA;
    i->oggetto_in_controllo = NULL;
    i->step_inizio_controllo = 0;
    i->inputList = NULL;
    i->outputList = NULL;
    i->sensorList = NULL;
    i->actuatorList = NULL;

    /* Guasto disabilitato di default: si attiva esplicitamente con
     * isp_impostaGuasto (sez. 5.3 del progetto).
     * Bug corretto: sensore_qualita_init si aspetta due target scalari
     * (dimensionX_target, raggio_target), non piu' un array target[3]:
     * la vecchia chiamata passava un array 'target' che non era nemmeno
     * piu' un parametro di questa funzione - non compilava. */
    localErr = sensore_qualita_init( &i->sensore, ID, &i->guasto, false, dimensionX_target, raggio_target );
    if ( localErr != OP_SUCCESS ) {
        free( i );
        if ( errCode != NULL ) { *errCode = localErr; }
        return NULL;
    }

    if ( errCode != NULL ) { *errCode = OP_SUCCESS; }

    return i;
}

void isp_delete( isp_t *i )
{
    if ( i == NULL ) {
        return;
    }

    /* L'oggetto eventualmente in controllo NON viene liberato: resta
     * di proprieta' di chi lo ha creato. */
    idlist_free( i->inputList );
    idlist_free( i->outputList );
    idlist_free( i->sensorList );
    idlist_free( i->actuatorList );

    free( i );
}

bool isp_isBusy( const isp_t *i )
{
    if ( i == NULL ) {
        return false;
    }
    return i->stato == ISP_OCCUPATA;
}

bool isp_isReady( const isp_t *i, int step_corrente )
{
    if ( i == NULL || i->stato == ISP_LIBERA ) {
        return false;
    }
    return ( step_corrente - i->step_inizio_controllo ) >= i->tempo_controllo;
}

const char *isp_getID( const isp_t *i )
{
    if ( i == NULL ) {
        return NULL;
    }
    return i->ID;
}

int isp_getTempoControllo( const isp_t *i )
{
    if ( i == NULL ) {
        return ERR_NULL_PTR;
    }
    return i->tempo_controllo;
}

short int isp_addInput( isp_t *i, const char *ID )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &i->inputList, ID );
}

short int isp_addOutput( isp_t *i, const char *ID )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &i->outputList, ID );
}

int isp_getInputCount( const isp_t *i )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( i->inputList );
}

short int isp_getInputAt( const isp_t *i, int index, char outID[IDLENGTH] )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( i->inputList, index, outID );
}

bool isp_hasInput( const isp_t *i, const char *ID )
{
    if ( i == NULL ) { return false; }
    return idlist_contains( i->inputList, ID );
}

int isp_getOutputCount( const isp_t *i )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( i->outputList );
}

short int isp_getOutputAt( const isp_t *i, int index, char outID[IDLENGTH] )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( i->outputList, index, outID );
}

bool isp_hasOutput( const isp_t *i, const char *ID )
{
    if ( i == NULL ) { return false; }
    return idlist_contains( i->outputList, ID );
}

short int isp_addSensor( isp_t *i, const char *ID )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &i->sensorList, ID );
}

int isp_getSensorCount( const isp_t *i )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( i->sensorList );
}

short int isp_getSensorAt( const isp_t *i, int index, char outID[IDLENGTH] )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( i->sensorList, index, outID );
}

bool isp_hasSensor( const isp_t *i, const char *ID )
{
    if ( i == NULL ) { return false; }
    return idlist_contains( i->sensorList, ID );
}

short int isp_addActuator( isp_t *i, const char *ID )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_add( &i->actuatorList, ID );
}

int isp_getActuatorCount( const isp_t *i )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_count( i->actuatorList );
}

short int isp_getActuatorAt( const isp_t *i, int index, char outID[IDLENGTH] )
{
    if ( i == NULL ) { return ERR_NULL_PTR; }
    return idlist_getAt( i->actuatorList, index, outID );
}

bool isp_hasActuator( const isp_t *i, const char *ID )
{
    if ( i == NULL ) { return false; }
    return idlist_contains( i->actuatorList, ID );
}

short int isp_impostaGuasto( isp_t *i, bool abilitato, int time_error, int time_ok )
{
    short int result;

    if ( i == NULL ) {
        return ERR_NULL_PTR;
    }

    result = sensore_qualita_imposta_guasto( &i->guasto, time_error, time_ok );
    if ( result != OP_SUCCESS ) {
        return result;
    }

    i->guasto.malfunzionamento_abilitato = abilitato;

    return OP_SUCCESS;
}

short int isp_admit( isp_t *i, object_t *object, int step_corrente )
{
    if ( i == NULL || object == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( i->stato == ISP_OCCUPATA ) {
        return ERR_FULL;
    }

    i->oggetto_in_controllo = object;
    i->step_inizio_controllo = step_corrente;
    i->stato = ISP_OCCUPATA;

    return OP_SUCCESS;
}

object_t *isp_tryRelease( isp_t *i, int step_corrente, TipoQualita *outEsito )
{
    object_t *result;
    int esito;

    if ( i == NULL || i->stato == ISP_LIBERA ) {
        return NULL;
    }
    if ( step_corrente - i->step_inizio_controllo < i->tempo_controllo ) {
        return NULL; /* controllo non ancora completato */
    }

    esito = get_qualita( &i->sensore, &i->guasto, step_corrente, i->oggetto_in_controllo, true );

    result = i->oggetto_in_controllo;
    i->oggetto_in_controllo = NULL;
    i->stato = ISP_LIBERA;

    if ( outEsito != NULL ) {
        /* get_qualita puo' restituire un codice ERR_* (negativo) solo in
         * casi anomali di configurazione (es. target non impostato per
         * questo materiale): per non bloccare la ISP l'oggetto viene
         * comunque rilasciato, trattato convenzionalmente come SCARTO.
         * Da confermare col gruppo se serve un comportamento diverso. */
        *outEsito = ( esito >= 0 ) ? (TipoQualita) esito : SCARTO;
    }

    return result;
}

void isp_print( const isp_t *i )
{
    const idNode_t *idCur;

    if ( i == NULL ) {
        printf( "isp_print: ISP NULL\n" );
        return;
    }

    printf( "ISP[ID=%s, tempo_controllo=%d, stato=%s]\n",
            i->ID, i->tempo_controllo,
            i->stato == ISP_OCCUPATA ? "OCCUPATA" : "LIBERA" );

    printf( "  input: " );
    idCur = i->inputList;
    if ( idCur == NULL ) { printf( "(nessuno)" ); }
    while ( idCur != NULL ) { printf( "%s ", idCur->ID ); idCur = idCur->next; }
    printf( "\n" );

    printf( "  output: " );
    idCur = i->outputList;
    if ( idCur == NULL ) { printf( "(nessuno)" ); }
    while ( idCur != NULL ) { printf( "%s ", idCur->ID ); idCur = idCur->next; }
    printf( "\n" );

    if ( i->oggetto_in_controllo != NULL ) {
        printf( "  in controllo (dallo step %d): ", i->step_inizio_controllo );
        object_print( i->oggetto_in_controllo );
    }
}
