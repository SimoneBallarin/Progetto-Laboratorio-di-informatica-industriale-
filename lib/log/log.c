/**
 * @file log.c
 * @brief Implementazione del modulo log.
 */
//  qui dentro c'è il "come funziona davvero" del log. In
// log.h avevo detto solo "cosa fanno" le funzioni; qui c'è il codice
// vero che le fa funzionare.
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "log.h"

struct log {//  questo è il contenuto VERO di "log_t", quello che in
// log.h avevo nascosto (puntatore opaco). Solo questo file lo vede.

    FILE *fp;
    bool anche_su_stdout;
    long contatori[3];   /**< Indicizzato da LogLivello (LOG_INFO/WARNING/ERROR). */
};

static const char *nomeLivello( LogLivello livello ) // piccola funzione di appoggio. Prende il livello (che è
// solo un numero: 0, 1 o 2) e restituisce la scritta giusta da
// mettere nel file ("INFO", "WARNING", "ERROR"), 
{
    switch ( livello ) {
        case LOG_INFO:    return "INFO   ";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR:   return "ERROR  ";
        default:          return "???    ";
    }
}

log_t *log_create( const char *path, bool anche_su_stdout, short int *errCode ) // : questa è la funzione che crea il log vero e proprio: apre
// il file e prepara la memoria. Se qualcosa va storto (file non
// apribile, memoria finita, path vuoto) ritorna NULL invece di far
// crashare tutto.
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

    l->fp = fopen( path, "w" );     // N provo ad aprire davvero il file, in modalità  (scrittura): se il file esisteva già, viene svuotato e riscritto
    // da zero - così ogni volta che lancio il programma ottengo un log pulito, senza roba vecchia mescolata dentro.
    // Se fopen fallisce (es. percorso sbagliato, permessi mancanti),DEVO fare "free(l)" prima di uscire: quella memoria l'avevo già
    // chiesta due righe sopra con malloc, e se non la libero resta "persa" per sempre finché il programma non chiude (si chiama
    // memory leak, un errore che Valgrind scoprirebbe).
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

void log_destroy( log_t *l )//  questa "spegne" il log: chiude il file e libera la
// memoria che avevo occupato con malloc in log_create. Va chiamata
// una volta sola, alla fine, quando non mi serve più scrivere niente. Se "l" è già NULL, non faccio niente e non crasho 
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

    if ( l == NULL || formato == NULL ) {     // : se il log è NULL, o non mi hanno dato nessuna frase
    // da scrivere, non faccio niente e esco subito, senza crashare.
    // Questo mi permette di chiamare log_evento(log, ...) ovunque nel
    // progetto senza dover controllare io ogni volta se il log esiste
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

long log_getContatore( const log_t *l, LogLivello livello )//  questa mi dice quante volte ho registrato un certo
// livello di evento finora. Prima controllo che "l" non sia NULL e
// che "livello" sia uno valido, poi vado semplicemente a leggere il
// numero dall'array contatori. Se uno dei controlli fallisce, torno
// il codice di errore giusto invece del numero.
{
    if ( l == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( livello < LOG_INFO || livello > LOG_ERROR ) {
        return ERR_OUT_OF_RANGE;
    }
    return l->contatori[livello];
}

void log_stampaRiepilogo( const log_t *l, bool anche_su_stdout ) //  questa scrive un riepilogo finale con il totale di INFO,
// WARNING ed ERROR registrati durante tutta la run. La chiamo una
// volta sola, alla fine della simulazione.
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
