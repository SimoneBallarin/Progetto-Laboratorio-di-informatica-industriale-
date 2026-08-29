#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "isp.h"

#define MAX_LINE_LEN 256

/* ---------------------------------------------------------------------
 * Utility di basso livello (stesse identiche a quelle usate per il
 * parsing generico gia' discusso col gruppo: qui adattate a lavorare
 * direttamente sulle API di cell.h/object.h invece che su struct proprie).
 * --------------------------------------------------------------------- */

static void trim_line( char *line )
{
    line[strcspn( line, "\r\n" )] = '\0';

    int start = 0;
    while ( line[start] == ' ' || line[start] == '\t' ) start++;
    if ( start > 0 ) memmove( line, line + start, strlen( line + start ) + 1 );

    /* Riga di commento (primo carattere non-spazio e' '#'): trattata come
     * riga vuota. Ogni chiamante di trim_line fa gia' "if (line[0] ==
     * '\0') continue;" subito dopo, quindi basta questa unica modifica
     * per far funzionare i commenti in TUTTI i file di configurazione
     * (impianto, scenario, oggetti), senza dover toccare ogni singolo
     * ciclo di parsing uno per uno - e senza produrre avvisi spuri tipo
     * "tipo di record sconosciuto" per righe che iniziano con '#'. */
    if ( line[0] == '#' ) {
        line[0] = '\0';
        return;
    }

    int len = (int) strlen( line );
    while ( len > 0 && ( line[len - 1] == ' ' || line[len - 1] == '\t' ) ) {
        line[len - 1] = '\0';
        len--;
    }
}

static int split_csv( char *line, char *tokens[], int max_tokens )
{
    int count = 0;
    char *token = strtok( line, "," );
    while ( token != NULL && count < max_tokens ) {
        tokens[count++] = token;
        token = strtok( NULL, "," );
    }
    return count;
}

static int split_key_value( const char *token, char *key_out, char *value_out, int out_len )
{
    const char *eq = strchr( token, '=' );
    if ( eq == NULL ) return 0;

    size_t key_len = (size_t) ( eq - token );
    if ( key_len == 0 || (int) key_len >= out_len ) return 0;

    strncpy( key_out, token, key_len );
    key_out[key_len] = '\0';

    strncpy( value_out, eq + 1, out_len - 1 );
    value_out[out_len - 1] = '\0';

    return ( value_out[0] != '\0' );
}

/* ---------------------------------------------------------------------
 * parser_costruisciCella
 * --------------------------------------------------------------------- */

static void gestisci_riga_buffer( cell_t *cell, char *tokens[], int n, int line_no, int *creati )
{
    char id[IDLENGTH] = "";
    int capacity = 0;
    int has_id = 0, has_cap = 0;

    for ( int i = 1; i < n; i++ ) {
        char key[32], value[32];
        if ( !split_key_value( tokens[i], key, value, sizeof( value ) ) ) continue;
        if ( strcmp( key, "ID" ) == 0 ) { strncpy( id, value, IDLENGTH - 1 ); has_id = 1; }
        else if ( strcmp( key, "CAPACITY" ) == 0 ) { capacity = atoi( value ); has_cap = 1; }
    }
    if ( !has_id || !has_cap ) {
        fprintf( stderr, "[parser riga %d] BUFFER: campo mancante (ID o CAPACITY), riga scartata\n", line_no );
        return;
    }

    short int err;
    if ( cell_addBuffer( cell, id, capacity, &err ) == NULL ) {
        fprintf( stderr, "[parser riga %d] BUFFER '%s': errore creazione (codice %d), riga scartata\n",
                 line_no, id, err );
        return;
    }
    ( *creati )++;
}

static void gestisci_riga_nastro( cell_t *cell, char *tokens[], int n, int line_no, int *creati )
{
    char id[IDLENGTH] = "";
    int capacity = 0, velocita = 0;
    int has_id = 0, has_cap = 0, has_vel = 0;

    for ( int i = 1; i < n; i++ ) {
        char key[32], value[32];
        if ( !split_key_value( tokens[i], key, value, sizeof( value ) ) ) continue;
        if ( strcmp( key, "ID" ) == 0 ) { strncpy( id, value, IDLENGTH - 1 ); has_id = 1; }
        else if ( strcmp( key, "CAPACITY" ) == 0 ) { capacity = atoi( value ); has_cap = 1; }
        else if ( strcmp( key, "VELOCITA" ) == 0 ) { velocita = atoi( value ); has_vel = 1; }
    }
    if ( !has_id || !has_cap || !has_vel ) {
        fprintf( stderr, "[parser riga %d] NASTRO: campo mancante (ID, CAPACITY o VELOCITA), riga scartata\n", line_no );
        return;
    }

    short int err;
    if ( cell_addNastro( cell, id, capacity, velocita, &err ) == NULL ) {
        fprintf( stderr, "[parser riga %d] NASTRO '%s': errore creazione (codice %d), riga scartata\n",
                 line_no, id, err );
        return;
    }
    ( *creati )++;
}

static void gestisci_riga_macchina( cell_t *cell, char *tokens[], int n, int line_no, int *creati )
{
    char id[IDLENGTH] = "";
    int tempo = 0;
    int has_id = 0, has_tempo = 0;

    for ( int i = 1; i < n; i++ ) {
        char key[32], value[32];
        if ( !split_key_value( tokens[i], key, value, sizeof( value ) ) ) continue;
        if ( strcmp( key, "ID" ) == 0 ) { strncpy( id, value, IDLENGTH - 1 ); has_id = 1; }
        else if ( strcmp( key, "TEMPO" ) == 0 ) { tempo = atoi( value ); has_tempo = 1; }
    }
    if ( !has_id || !has_tempo ) {
        fprintf( stderr, "[parser riga %d] MACCHINA: campo mancante (ID o TEMPO), riga scartata\n", line_no );
        return;
    }

    short int err;
    if ( cell_addMachine( cell, id, tempo, &err ) == NULL ) {
        fprintf( stderr, "[parser riga %d] MACCHINA '%s': errore creazione (codice %d), riga scartata\n",
                 line_no, id, err );
        return;
    }
    ( *creati )++;
}

static void gestisci_riga_isp( cell_t *cell, char *tokens[], int n, int line_no, int *creati )
{
    char id[IDLENGTH] = "";
    int tempo = 0, dimx_target = 0, raggio_target = 0;
    int has_id = 0, has_tempo = 0, has_dimx = 0, has_raggio = 0;

    for ( int i = 1; i < n; i++ ) {
        char key[32], value[32];
        if ( !split_key_value( tokens[i], key, value, sizeof( value ) ) ) continue;
        if ( strcmp( key, "ID" ) == 0 ) { strncpy( id, value, IDLENGTH - 1 ); has_id = 1; }
        else if ( strcmp( key, "TEMPO" ) == 0 ) { tempo = atoi( value ); has_tempo = 1; }
        else if ( strcmp( key, "DIMX_TARGET" ) == 0 ) { dimx_target = atoi( value ); has_dimx = 1; }
        else if ( strcmp( key, "RAGGIO_TARGET" ) == 0 ) { raggio_target = atoi( value ); has_raggio = 1; }
    }
    if ( !has_id || !has_tempo || !has_dimx || !has_raggio ) {
        fprintf( stderr, "[parser riga %d] ISP: campo mancante (ID, TEMPO, DIMX_TARGET o RAGGIO_TARGET), riga scartata\n", line_no );
        return;
    }

    (void) dimx_target;
    (void) raggio_target;

    short int err;
    if ( cell_addISP( cell, id, tempo, &err ) == NULL ) {
        fprintf( stderr, "[parser riga %d] ISP '%s': errore creazione (codice %d), riga scartata\n",
                 line_no, id, err );
        return;
    }
    ( *creati )++;
}

static void gestisci_riga_connect( cell_t *cell, char *tokens[], int n, int line_no, int *creati )
{
    char from[IDLENGTH] = "", to[IDLENGTH] = "";
    int has_from = 0, has_to = 0;

    for ( int i = 1; i < n; i++ ) {
        char key[32], value[32];
        if ( !split_key_value( tokens[i], key, value, sizeof( value ) ) ) continue;
        if ( strcmp( key, "FROM" ) == 0 ) { strncpy( from, value, IDLENGTH - 1 ); has_from = 1; }
        else if ( strcmp( key, "TO" ) == 0 ) { strncpy( to, value, IDLENGTH - 1 ); has_to = 1; }
    }
    if ( !has_from || !has_to ) {
        fprintf( stderr, "[parser riga %d] CONNECT: campo mancante (FROM o TO), riga scartata\n", line_no );
        return;
    }

    short int err = cell_connect( cell, from, to );
    if ( err != OP_SUCCESS ) {
        fprintf( stderr, "[parser riga %d] CONNECT '%s' -> '%s': errore (codice %d), riga scartata "
                 "(FROM/TO devono essere gia' stati creati da una riga precedente)\n",
                 line_no, from, to, err );
        return;
    }
    ( *creati )++;
}

int parser_costruisciCella( cell_t *cell, const char *path, short int *errCode )
{
    int creati = 0;

    if ( cell == NULL || path == NULL ) {
        if ( errCode ) *errCode = ERR_NULL_PTR;
        return 0;
    }

    FILE *f = fopen( path, "r" );
    if ( f == NULL ) {
        fprintf( stderr, "[parser] impossibile aprire il file di configurazione '%s'\n", path );
        if ( errCode ) *errCode = ERR_NOT_FOUND;
        return 0;
    }

    char raw_line[MAX_LINE_LEN];
    char line_copy[MAX_LINE_LEN];
    int line_no = 0;

    while ( fgets( raw_line, sizeof( raw_line ), f ) ) {
        line_no++;
        trim_line( raw_line );
        if ( raw_line[0] == '\0' ) continue;

        strncpy( line_copy, raw_line, MAX_LINE_LEN - 1 );
        line_copy[MAX_LINE_LEN - 1] = '\0';

        char *tokens[16];
        int n = split_csv( line_copy, tokens, 16 );
        if ( n == 0 ) continue;

        if ( strcmp( tokens[0], "BUFFER" ) == 0 ) {
            gestisci_riga_buffer( cell, tokens, n, line_no, &creati );
        } else if ( strcmp( tokens[0], "NASTRO" ) == 0 ) {
            gestisci_riga_nastro( cell, tokens, n, line_no, &creati );
        } else if ( strcmp( tokens[0], "MACCHINA" ) == 0 ) {
            gestisci_riga_macchina( cell, tokens, n, line_no, &creati );
        } else if ( strcmp( tokens[0], "ISP" ) == 0 ) {
            gestisci_riga_isp( cell, tokens, n, line_no, &creati );
        } else if ( strcmp( tokens[0], "CONNECT" ) == 0 ) {
            gestisci_riga_connect( cell, tokens, n, line_no, &creati );
                } else if ( strcmp( tokens[0], "MOTORE" ) == 0 || strcmp( tokens[0], "DEVIATORE" ) == 0 ) {
            continue; /* gestite da parser_collegaAttuatori, non qui */
        } else if ( strncmp( raw_line, "SIM_", 4 ) == 0 || strncmp( raw_line, "GEN_", 4 ) == 0
                    || strncmp( raw_line, "SOGLIA_BUFFER=", 14 ) == 0 ) {
            continue; /* gestite da parser_caricaSimulazione, non qui */
        } else if ( strcmp( tokens[0], "INGRESSO" ) == 0 ) {
            continue; /* gestita da parser_collegaSensoriPresenza, non qui */
        } else {
            fprintf( stderr, "[parser riga %d] tipo di record sconosciuto: '%s'\n", line_no, tokens[0] );
        }
    }

    fclose( f );

    if ( errCode ) *errCode = ( creati > 0 ) ? OP_SUCCESS : ERR_NOT_FOUND;
    return creati;
}

/* ---------------------------------------------------------------------
 * parser_collegaAttuatori
 * --------------------------------------------------------------------- */

int parser_collegaAttuatori( controllore_t *ctrl, const char *path, short int *errCode )
{
    int collegati = 0;

    if ( ctrl == NULL || path == NULL ) {
        if ( errCode ) *errCode = ERR_NULL_PTR;
        return 0;
    }

    FILE *f = fopen( path, "r" );
    if ( f == NULL ) {
        fprintf( stderr, "[parser] impossibile aprire il file di configurazione '%s'\n", path );
        if ( errCode ) *errCode = ERR_NOT_FOUND;
        return 0;
    }

    char raw_line[MAX_LINE_LEN];
    char line_copy[MAX_LINE_LEN];
    int line_no = 0;

    while ( fgets( raw_line, sizeof( raw_line ), f ) ) {
        line_no++;
        trim_line( raw_line );
        if ( raw_line[0] == '\0' ) continue;

        strncpy( line_copy, raw_line, MAX_LINE_LEN - 1 );
        line_copy[MAX_LINE_LEN - 1] = '\0';

        char *tokens[16];
        int n = split_csv( line_copy, tokens, 16 );
        if ( n == 0 ) continue;

        if ( strcmp( tokens[0], "MOTORE" ) == 0 ) {
            char nastro[IDLENGTH] = "";
            int velocita = 0, accel = 0;
            int has_nastro = 0, has_vel = 0, has_accel = 0;

            for ( int i = 1; i < n; i++ ) {
                char key[32], value[32];
                if ( !split_key_value( tokens[i], key, value, sizeof( value ) ) ) continue;
                if ( strcmp( key, "NASTRO" ) == 0 ) { strncpy( nastro, value, IDLENGTH - 1 ); has_nastro = 1; }
                else if ( strcmp( key, "VELOCITA" ) == 0 ) { velocita = atoi( value ); has_vel = 1; }
                else if ( strcmp( key, "ACCEL" ) == 0 ) { accel = atoi( value ); has_accel = 1; }
            }
            if ( !has_nastro || !has_vel || !has_accel ) {
                fprintf( stderr, "[parser riga %d] MOTORE: campo mancante, riga scartata\n", line_no );
                continue;
            }
            short int err = controllore_collegaMotore( ctrl, nastro, velocita, accel );
            if ( err != OP_SUCCESS ) {
                fprintf( stderr, "[parser riga %d] MOTORE su '%s': errore (codice %d), riga scartata\n",
                         line_no, nastro, err );
                continue;
            }
            collegati++;

        } else if ( strcmp( tokens[0], "DEVIATORE" ) == 0 ) {
            char isp[IDLENGTH] = "";
            int tempo_min = 0;
            int has_isp = 0, has_tempo = 0;

            for ( int i = 1; i < n; i++ ) {
                char key[32], value[32];
                if ( !split_key_value( tokens[i], key, value, sizeof( value ) ) ) continue;
                if ( strcmp( key, "ISP" ) == 0 ) { strncpy( isp, value, IDLENGTH - 1 ); has_isp = 1; }
                else if ( strcmp( key, "TEMPO_MIN_COMMUT" ) == 0 ) { tempo_min = atoi( value ); has_tempo = 1; }
            }
            if ( !has_isp || !has_tempo ) {
                fprintf( stderr, "[parser riga %d] DEVIATORE: campo mancante, riga scartata\n", line_no );
                continue;
            }
            short int err = controllore_collegaDeviatore( ctrl, isp, tempo_min );
            if ( err != OP_SUCCESS ) {
                fprintf( stderr, "[parser riga %d] DEVIATORE su '%s': errore (codice %d), riga scartata\n",
                         line_no, isp, err );
                continue;
            }
            collegati++;
        }
        /* le altre righe (BUFFER/NASTRO/MACCHINA/ISP/CONNECT) sono gestite
         * da parser_costruisciCella, non qui: si ignorano silenziosamente. */
    }

    fclose( f );

    if ( errCode ) *errCode = OP_SUCCESS;
    return collegati;
}

/* ---------------------------------------------------------------------
 * parser_collegaSensoriQualita
 * --------------------------------------------------------------------- */

int parser_collegaSensoriQualita( controllore_t *ctrl, const char *path, short int *errCode )
{
    int collegati = 0;

    if ( ctrl == NULL || path == NULL ) {
        if ( errCode ) *errCode = ERR_NULL_PTR;
        return 0;
    }

    FILE *f = fopen( path, "r" );
    if ( f == NULL ) {
        fprintf( stderr, "[parser] impossibile aprire il file di configurazione '%s'\n", path );
        if ( errCode ) *errCode = ERR_NOT_FOUND;
        return 0;
    }

    char raw_line[MAX_LINE_LEN];
    char line_copy[MAX_LINE_LEN];
    int line_no = 0;

    while ( fgets( raw_line, sizeof( raw_line ), f ) ) {
        line_no++;
        trim_line( raw_line );
        if ( raw_line[0] == '\0' ) continue;

        strncpy( line_copy, raw_line, MAX_LINE_LEN - 1 );
        line_copy[MAX_LINE_LEN - 1] = '\0';

        char *tokens[16];
        int n = split_csv( line_copy, tokens, 16 );
        if ( n == 0 || strcmp( tokens[0], "ISP" ) != 0 ) continue;

        char id[IDLENGTH] = "";
        int dimx_target = 0, raggio_target = 0;
        int has_id = 0;

        for ( int i = 1; i < n; i++ ) {
            char key[32], value[32];
            if ( !split_key_value( tokens[i], key, value, sizeof( value ) ) ) continue;
            if ( strcmp( key, "ID" ) == 0 ) { strncpy( id, value, IDLENGTH - 1 ); has_id = 1; }
            else if ( strcmp( key, "DIMX_TARGET" ) == 0 ) { dimx_target = atoi( value ); }
            else if ( strcmp( key, "RAGGIO_TARGET" ) == 0 ) { raggio_target = atoi( value ); }
        }
        if ( !has_id ) continue;

        /* ISP "passacarte" (nessun controllo qualita' reale): si salta,
         * per design - vedi isp.h. */
        if ( dimx_target <= 0 || raggio_target <= 0 ) continue;

        short int err = controllore_collegaSensoreQualita( ctrl, id, dimx_target, raggio_target );
        if ( err != OP_SUCCESS ) {
            fprintf( stderr, "[parser riga %d] SensoreQualita su '%s': errore (codice %d), riga scartata\n",
                     line_no, id, err );
            continue;
        }
        collegati++;
    }

    fclose( f );
    if ( errCode ) *errCode = OP_SUCCESS;
    return collegati;
}
/* ---------------------------------------------------------------------
 * parser_caricaOggetti
 * --------------------------------------------------------------------- */

int parser_caricaOggetti( cell_t *cell, controllore_t *ctrl, const char *path,
                           const char *bufferIngressoID, short int *errCode )
{
    int caricati = 0;

    if ( cell == NULL || ctrl == NULL || path == NULL || bufferIngressoID == NULL ) {
        if ( errCode ) *errCode = ERR_NULL_PTR;
        return 0;
    }

    /* Verifica solo che il buffer esista ADESSO (fail-fast su un
     * percorso/ID sbagliato nel file di configurazione): NON serve piu'
     * controllare buffer_isFull qui, dato che ogni oggetto viene
     * SCHEDULATO (controllore_schedulaArrivo) per il proprio
     * ARRIVAL_STEP invece di essere inserito subito - l'eventuale buffer
     * pieno al momento giusto viene gestito automaticamente da
     * retryArriviSchedulati (ritenta ai passi successivi, non scarta). */
    if ( cell_getBuffer( cell, bufferIngressoID ) == NULL ) {
        fprintf( stderr, "[parser] buffer di ingresso '%s' non trovato nella cella\n", bufferIngressoID );
        if ( errCode ) *errCode = ERR_NOT_FOUND;
        return 0;
    }

    FILE *f = fopen( path, "r" );
    if ( f == NULL ) {
        fprintf( stderr, "[parser] impossibile aprire il file oggetti '%s'\n", path );
        if ( errCode ) *errCode = ERR_NOT_FOUND;
        return 0;
    }

    char raw_line[MAX_LINE_LEN];
    char line_copy[MAX_LINE_LEN];
    int line_no = 0;
    int header_skipped = 0;

    while ( fgets( raw_line, sizeof( raw_line ), f ) ) {
        line_no++;
        trim_line( raw_line );
        if ( raw_line[0] == '\0' ) continue;

        if ( !header_skipped ) {
            header_skipped = 1;
            if ( strncmp( raw_line, "ID,", 3 ) == 0 ) continue;
        }

        strncpy( line_copy, raw_line, MAX_LINE_LEN - 1 );
        line_copy[MAX_LINE_LEN - 1] = '\0';

        char *tokens[8];
        int n = split_csv( line_copy, tokens, 8 );
        if ( n < 6 ) {
            fprintf( stderr, "[parser riga %d] oggetto: riga incompleta (attesi 6 campi: "
                     "ID,PRIORITY,TYPE,ARRIVAL_STEP,DIMENSIONX,RAGGIO), riga scartata\n", line_no );
            continue;
        }

        const char *id_str    = tokens[0];
        short int   priority  = (short int) atoi( tokens[1] );
        char        type      = tokens[2][0];
        int         arrival   = atoi( tokens[3] );
        double      dimensionX = atof( tokens[4] );
        double      raggio    = atof( tokens[5] );

        if ( id_str[0] == '\0' ) {
            fprintf( stderr, "[parser riga %d] oggetto: ID mancante, riga scartata\n", line_no );
            continue;
        }
        if ( type != 'A' && type != 'B' ) {
            fprintf( stderr, "[parser riga %d] oggetto '%s': TYPE non valido ('%c'), atteso 'A' o 'B', "
                     "riga scartata\n", line_no, id_str, type );
            continue;
        }
        if ( priority < PRIORITY_MIN || priority > PRIORITY_MAX ) {
            fprintf( stderr, "[parser riga %d] oggetto '%s': PRIORITY %d fuori intervallo [%d-%d], "
                     "riga scartata\n", line_no, id_str, priority, PRIORITY_MIN, PRIORITY_MAX );
            continue;
        }
        if ( dimensionX < 0 || raggio < 0 ) {
            fprintf( stderr, "[parser riga %d] oggetto '%s': DIMENSIONX/RAGGIO negativi, riga scartata\n",
                     line_no, id_str );
            continue;
        }
        if ( arrival < 0 ) {
            fprintf( stderr, "[parser riga %d] oggetto '%s': ARRIVAL_STEP negativo, riga scartata\n",
                     line_no, id_str );
            continue;
        }

        short int err;
        /* stepCreation = arrival: adesso e' corretto farlo coincidere,
         * perche' l'oggetto verra' DAVVERO inserito nel buffer a quello
         * step (vedi controllore_schedulaArrivo sotto), non prima come
         * succedeva con l'inserimento immediato della versione
         * precedente di questa funzione. */
        object_t *obj = object_create( id_str, priority, type, arrival, dimensionX, raggio, &err );
        if ( obj == NULL ) {
            fprintf( stderr, "[parser riga %d] oggetto '%s': errore creazione (codice %d), riga scartata\n",
                     line_no, id_str, err );
            continue;
        }

        /* controllore_schedulaArrivo (non buffer_insertObject diretto):
         * inserisce l'oggetto nel buffer SOLO quando la simulazione
         * raggiunge arrival_step, aggiornando correttamente anche
         * l'eventuale SensoreBuffer agganciato (un inserimento diretto
         * lo lascerebbe permanentemente disallineato dal reale livello
         * del buffer - bug della versione precedente di questa
         * funzione, corretto insieme all'introduzione di questo
         * meccanismo). */
        err = controllore_schedulaArrivo( ctrl, bufferIngressoID, obj, arrival );
        if ( err != OP_SUCCESS ) {
            fprintf( stderr, "[parser riga %d] oggetto '%s': errore schedulazione arrivo (codice %d), riga scartata\n",
                     line_no, id_str, err );
            object_delete( obj );
            continue;
        }

        caricati++;
    }

    fclose( f );

    if ( errCode ) *errCode = ( caricati > 0 ) ? OP_SUCCESS : ERR_NOT_FOUND;
    return caricati;
}

/* ---------------------------------------------------------------------
 * parser_caricaScenario / parser_applicaScenario
 * --------------------------------------------------------------------- */

int parser_caricaScenario( const char *path, ScenarioConfig *out, short int *errCode )
{
    if ( out == NULL ) {
        if ( errCode ) *errCode = ERR_NULL_PTR;
        return 0;
    }

    memset( out, 0, sizeof( *out ) );
    out->moltiplicatore_carico = 1.0;

    FILE *f = fopen( path, "r" );
    if ( f == NULL ) {
        fprintf( stderr, "[parser] impossibile aprire il file di scenario '%s'\n", path );
        if ( errCode ) *errCode = ERR_NOT_FOUND;
        return 0;
    }

    char raw_line[MAX_LINE_LEN];
    int line_no = 0;

    while ( fgets( raw_line, sizeof( raw_line ), f ) ) {
        line_no++;
        trim_line( raw_line );
        if ( raw_line[0] == '\0' ) continue;

        char key[32], value[64];
        if ( !split_key_value( raw_line, key, value, sizeof( value ) ) ) {
            fprintf( stderr, "[parser riga %d] scenario: riga malformata, riga scartata\n", line_no );
            continue;
        }

        if ( strcmp( key, "SCENARIO_NAME" ) == 0 ) {
            strncpy( out->nome, value, sizeof( out->nome ) - 1 );
        } else if ( strcmp( key, "LOAD_MULTIPLIER" ) == 0 ) {
            out->moltiplicatore_carico = atof( value );
        } else if ( strcmp( key, "FAULT_ENABLED" ) == 0 ) {
            out->guasto_abilitato = ( atoi( value ) != 0 );
        } else if ( strcmp( key, "FAULT_ISP" ) == 0 ) {
            /* Una o piu' ISP separate da virgola, es. "ISP1,ISP2" (senza
             * spazi attorno alla virgola - trim_line ha gia' tolto solo
             * gli spazi a inizio/fine riga, non quelli interni). Oltre
             * MAX_GUASTO_ISP entrate, le successive vengono scartate con
             * un avviso invece di essere ignorate silenziosamente o
             * sovrascrivere memoria oltre l'array. Riusa split_csv (gia'
             * usata per il file oggetti) invece di strtok_r, che non e'
             * C11 standard senza una macro POSIX. */
            char value_copy[64];
            char *tokens[MAX_GUASTO_ISP + 1];  /* +1 per rilevare "troppe entrate" senza troncare silenziosamente */
            int n_tokens;
            int t;

            strncpy( value_copy, value, sizeof( value_copy ) - 1 );
            value_copy[sizeof( value_copy ) - 1] = '\0';

            n_tokens = split_csv( value_copy, tokens, MAX_GUASTO_ISP + 1 );
            out->n_guasto_isp = 0;
            for ( t = 0; t < n_tokens; t++ ) {
                if ( out->n_guasto_isp >= MAX_GUASTO_ISP ) {
                    fprintf( stderr, "[parser riga %d] scenario: FAULT_ISP ha piu' di %d ISP, '%s' e le successive scartate\n",
                             line_no, MAX_GUASTO_ISP, tokens[t] );
                    break;
                }
                strncpy( out->guasto_isp_id[out->n_guasto_isp], tokens[t], IDLENGTH - 1 );
                out->guasto_isp_id[out->n_guasto_isp][IDLENGTH - 1] = '\0';
                out->n_guasto_isp++;
            }
        } else if ( strcmp( key, "FAULT_TIME_ERROR" ) == 0 ) {
            out->guasto_tempo_errore = atoi( value );
        } else if ( strcmp( key, "FAULT_TIME_OK" ) == 0 ) {
            out->guasto_tempo_ok = atoi( value );
        } else {
            fprintf( stderr, "[parser riga %d] scenario: chiave sconosciuta '%s'\n", line_no, key );
        }
    }

    fclose( f );
    if ( errCode ) *errCode = OP_SUCCESS;
    return 1;
}

short int parser_applicaScenario( controllore_t *ctrl, const ScenarioConfig *scenario )
{
    int i;
    short int ultimoErrore = OP_SUCCESS;

    if ( ctrl == NULL || scenario == NULL ) return ERR_NULL_PTR;
    if ( scenario->n_guasto_isp == 0 ) return OP_SUCCESS; /* nessun guasto da configurare */

    /* Applica lo STESSO guasto (abilitato/tempo_errore/tempo_ok) a OGNI
     * ISP elencata in FAULT_ISP - vedi doc in parser.h. Continua anche
     * se una fallisce (es. ID non trovato o senza sensore agganciato),
     * per non lasciare le altre non configurate solo perche' una e'
     * sbagliata; l'errore dell'ultima entrata fallita viene comunque
     * restituito al chiamante. */
    for ( i = 0; i < scenario->n_guasto_isp; i++ ) {
        short int err = controllore_impostaGuastoQualita( ctrl, scenario->guasto_isp_id[i],
                                                            scenario->guasto_abilitato,
                                                            scenario->guasto_tempo_errore,
                                                            scenario->guasto_tempo_ok );
        if ( err != OP_SUCCESS ) {
            fprintf( stderr, "[parser] guasto su '%s': errore (codice %d), le altre ISP elencate vengono comunque applicate\n",
                     scenario->guasto_isp_id[i], err );
            ultimoErrore = err;
        }
    }
    return ultimoErrore;
}

/* ---------------------------------------------------------------------
 * parser_caricaSimulazione
 * --------------------------------------------------------------------- */

int parser_caricaSimulazione( const char *path, SimulationConfig *out, short int *errCode )
{
    if ( out == NULL ) {
        if ( errCode ) *errCode = ERR_NULL_PTR;
        return 0;
    }

    /* Default ragionevoli, usati per ogni chiave assente dal file. */
    memset( out, 0, sizeof( *out ) );
    out->n_step_simulazione    = 60;
    out->n_pezzi_prova         = 10;
    out->n_pezzi_prova_b2      = 0;   /* default: nessun pezzo pre-caricato in B2, comportamento storico */
    out->soglia_buffer         = 0.8;
    out->gen_target_dimensionX = 100.0;
    out->gen_target_raggio     = 10.0;
    out->gen_errore_pct        = 2;

    FILE *f = fopen( path, "r" );
    if ( f == NULL ) {
        fprintf( stderr, "[parser] impossibile aprire il file di configurazione '%s'\n", path );
        if ( errCode ) *errCode = ERR_NOT_FOUND;
        return 0;
    }

    char raw_line[MAX_LINE_LEN];
    int line_no = 0;

    while ( fgets( raw_line, sizeof( raw_line ), f ) ) {
        line_no++;
        trim_line( raw_line );
        if ( raw_line[0] == '\0' ) continue;

        char key[32], value[32];
        if ( !split_key_value( raw_line, key, value, sizeof( value ) ) ) continue;

        if ( strcmp( key, "SIM_STEPS" ) == 0 ) {
            out->n_step_simulazione = atoi( value );
        } else if ( strcmp( key, "SIM_PEZZI" ) == 0 ) {
            out->n_pezzi_prova = atoi( value );
        } else if ( strcmp( key, "SIM_PEZZI_B2" ) == 0 ) {
            out->n_pezzi_prova_b2 = atoi( value );
        } else if ( strcmp( key, "SOGLIA_BUFFER" ) == 0 ) {
            out->soglia_buffer = atof( value );
        } else if ( strcmp( key, "GEN_TARGET_DIMENSIONX" ) == 0 ) {
            out->gen_target_dimensionX = atof( value );
        } else if ( strcmp( key, "GEN_TARGET_RAGGIO" ) == 0 ) {
            out->gen_target_raggio = atof( value );
        } else if ( strcmp( key, "GEN_ERRORE_PCT" ) == 0 ) {
            out->gen_errore_pct = atoi( value );
        }
    }

    fclose( f );
    if ( errCode ) *errCode = OP_SUCCESS;
    return 1;
}
/* ---------------------------------------------------------------------
 * parser_collegaSensoriBuffer
 * --------------------------------------------------------------------- */

int parser_collegaSensoriBuffer( controllore_t *ctrl, const char *path, short int *errCode )
{
    int collegati = 0;

    if ( ctrl == NULL || path == NULL ) {
        if ( errCode ) *errCode = ERR_NULL_PTR;
        return 0;
    }

    FILE *f = fopen( path, "r" );
    if ( f == NULL ) {
        fprintf( stderr, "[parser] impossibile aprire il file di configurazione '%s'\n", path );
        if ( errCode ) *errCode = ERR_NOT_FOUND;
        return 0;
    }

    char raw_line[MAX_LINE_LEN];
    char line_copy[MAX_LINE_LEN];
    int line_no = 0;

    while ( fgets( raw_line, sizeof( raw_line ), f ) ) {
        line_no++;
        trim_line( raw_line );
        if ( raw_line[0] == '\0' ) continue;

        strncpy( line_copy, raw_line, MAX_LINE_LEN - 1 );
        line_copy[MAX_LINE_LEN - 1] = '\0';

        char *tokens[16];
        int n = split_csv( line_copy, tokens, 16 );
        if ( n == 0 || strcmp( tokens[0], "BUFFER" ) != 0 ) continue;

        char id[IDLENGTH] = "";
        int has_id = 0;
        for ( int i = 1; i < n; i++ ) {
            char key[32], value[32];
            if ( !split_key_value( tokens[i], key, value, sizeof( value ) ) ) continue;
            if ( strcmp( key, "ID" ) == 0 ) { strncpy( id, value, IDLENGTH - 1 ); has_id = 1; }
        }
        if ( !has_id ) continue;

        short int err = controllore_collegaSensoreBuffer( ctrl, id );
        if ( err != OP_SUCCESS ) {
            fprintf( stderr, "[parser riga %d] SensoreBuffer su '%s': errore (codice %d), riga scartata\n",
                     line_no, id, err );
            continue;
        }
        collegati++;
    }

    fclose( f );
    if ( errCode ) *errCode = OP_SUCCESS;
    return collegati;
}

/* ---------------------------------------------------------------------
 * parser_collegaSensoriPresenza
 * --------------------------------------------------------------------- */

int parser_collegaSensoriPresenza( controllore_t *ctrl, const char *path, short int *errCode )
{
    int collegati = 0;

    if ( ctrl == NULL || path == NULL ) {
        if ( errCode ) *errCode = ERR_NULL_PTR;
        return 0;
    }

    FILE *f = fopen( path, "r" );
    if ( f == NULL ) {
        fprintf( stderr, "[parser] impossibile aprire il file di configurazione '%s'\n", path );
        if ( errCode ) *errCode = ERR_NOT_FOUND;
        return 0;
    }

    char raw_line[MAX_LINE_LEN];
    char line_copy[MAX_LINE_LEN];
    int line_no = 0;

    while ( fgets( raw_line, sizeof( raw_line ), f ) ) {
        line_no++;
        trim_line( raw_line );
        if ( raw_line[0] == '\0' ) continue;

        strncpy( line_copy, raw_line, MAX_LINE_LEN - 1 );
        line_copy[MAX_LINE_LEN - 1] = '\0';

        char *tokens[16];
        int n = split_csv( line_copy, tokens, 16 );
        if ( n == 0 || strcmp( tokens[0], "INGRESSO" ) != 0 ) continue;

        char id[IDLENGTH] = "";
        int has_id = 0;
        for ( int i = 1; i < n; i++ ) {
            char key[32], value[32];
            if ( !split_key_value( tokens[i], key, value, sizeof( value ) ) ) continue;
            if ( strcmp( key, "ID" ) == 0 ) { strncpy( id, value, IDLENGTH - 1 ); has_id = 1; }
        }
        if ( !has_id ) continue;

        short int err = controllore_collegaSensorePresenza( ctrl, id );
        if ( err != OP_SUCCESS ) {
            fprintf( stderr, "[parser riga %d] SensorePresenza su '%s': errore (codice %d), riga scartata\n",
                     line_no, id, err );
            continue;
        }
        collegati++;
    }

    fclose( f );
    if ( errCode ) *errCode = OP_SUCCESS;
    return collegati;
}