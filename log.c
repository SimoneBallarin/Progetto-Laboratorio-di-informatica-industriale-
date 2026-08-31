/**
 * @file log.c
 * @brief Implementazione del modulo log.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "log.h"

struct log {
    FILE *fp;
    bool anche_su_stdout;
    long contatori[3];   /**< Indicizzato da LogLivello (LOG_INFO/WARNING/ERROR). */
};

static const char *nomeLivello( LogLivello livello )
{
    switch ( livello ) {
        case LOG_INFO:    return "INFO   ";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR:   return "ERROR  ";
        default:          return "???    ";
    }
}

log_t *log_create( const char *path, bool anche_su_stdout, short int *errCode )
{
    log_t *l;

    if ( path == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_NULL_PTR; }
        return NULL;
    }

    l = malloc( sizeof( struct log ) );
    if ( l == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_ALLOC; }
        return NULL;
    }

    l->fp = fopen( path, "w" );
    if ( l->fp == NULL ) {
        free( l );
        if ( errCode != NULL ) { *errCode = ERR_NOT_FOUND; }
        return NULL;
    }

    l->anche_su_stdout = anche_su_stdout;
    l->contatori[LOG_INFO]    = 0;
    l->contatori[LOG_WARNING] = 0;
    l->contatori[LOG_ERROR]   = 0;

    fprintf( l->fp, "=== LOG EVENTI SIMULAZIONE ===\n" );
    fflush( l->fp );

    if ( errCode != NULL ) { *errCode = OP_SUCCESS; }
    return l;
}

void log_destroy( log_t *l )
{
    if ( l == NULL ) {
        return;
    }
    if ( l->fp != NULL ) {
        fclose( l->fp );
    }
    free( l );
}

void log_evento( log_t *l, int step, LogLivello livello, const char *formato, ... )
{
    va_list args;

    if ( l == NULL || formato == NULL ) {
        return;
    }
    if ( livello < LOG_INFO || livello > LOG_ERROR ) {
        return;
    }

    l->contatori[livello]++;

    if ( step >= 0 ) {
        fprintf( l->fp, "[step %5d] [%s] ", step, nomeLivello( livello ) );
    } else {
        fprintf( l->fp, "[         ] [%s] ", nomeLivello( livello ) );
    }
    va_start( args, formato );
    vfprintf( l->fp, formato, args );
    va_end( args );
    fprintf( l->fp, "\n" );
    fflush( l->fp );  /* cosi' il log resta leggibile anche in caso di crash */

    if ( l->anche_su_stdout ) {
        if ( step >= 0 ) {
            printf( "[step %5d] [%s] ", step, nomeLivello( livello ) );
        } else {
            printf( "[         ] [%s] ", nomeLivello( livello ) );
        }
        va_start( args, formato );
        vprintf( formato, args );
        va_end( args );
        printf( "\n" );
    }
}

long log_getContatore( const log_t *l, LogLivello livello )
{
    if ( l == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( livello < LOG_INFO || livello > LOG_ERROR ) {
        return ERR_OUT_OF_RANGE;
    }
    return l->contatori[livello];
}

void log_stampaRiepilogo( const log_t *l, bool anche_su_stdout )
{
    if ( l == NULL ) {
        if ( anche_su_stdout ) {
            printf( "log_stampaRiepilogo: log NULL\n" );
        }
        return;
    }

    /* Scritto SEMPRE nel file (in coda a tutti gli eventi già
     * registrati), indipendentemente da anche_su_stdout: e' il resoconto
     * persistente della run, deve restare disponibile anche quando la
     * stampa a schermo e' disattivata (vedi doc in log.h). */
    if ( l->fp != NULL ) {
        fprintf( l->fp, "\n=== LOG EVENTI (riepilogo) ===\n" );
        fprintf( l->fp, "  INFO:    %ld\n", l->contatori[LOG_INFO] );
        fprintf( l->fp, "  WARNING: %ld\n", l->contatori[LOG_WARNING] );
        fprintf( l->fp, "  ERROR:   %ld\n", l->contatori[LOG_ERROR] );
        fflush( l->fp );
    }

    if ( anche_su_stdout ) {
        printf( "\n=== LOG EVENTI (riepilogo) ===\n" );
        printf( "  INFO:    %ld\n", l->contatori[LOG_INFO] );
        printf( "  WARNING: %ld\n", l->contatori[LOG_WARNING] );
        printf( "  ERROR:   %ld\n", l->contatori[LOG_ERROR] );
    }
}
