/**
 * @file statistiche.c
 * @brief Implementazione del modulo statistiche.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "statistiche.h"

/* ------------------------------------------------------------------ */
/*  STRUTTURE INTERNE                                                  */
/* ------------------------------------------------------------------ */

/** @brief Occupazione accumulata di un buffer monitorato. */
typedef struct bufferStat {
    char ID[IDLENGTH];
    long somma_percentuale;   /**< Somma delle percentuali campionate, per calcolare la media. */
    int  campioni;
    int  massimo_percentuale;
    long blocchi;             /**< Arrivi rifiutati per buffer pieno, vedi statistiche_registraBlocco. */
    struct bufferStat *next;
} bufferStat_t;

/** @brief Motore monitorato, per il tempo cumulativo ON/OFF. */
typedef struct motoreStat {
    char ID[IDLENGTH];
    struct motoreStat *next;
} motoreStat_t;

/** @brief ISP monitorata, per le statistiche del suo sensore di qualità. */
typedef struct ispStat {
    char ID[IDLENGTH];
    struct ispStat *next;
} ispStat_t;

/** @brief ID monitorato per le statistiche del sensore di presenza. */
typedef struct presenzaStat {
    char ID[IDLENGTH];
    struct presenzaStat *next;
} presenzaStat_t;

/** @brief Aggregazione per una singola classe di priorità (0..PRIORITY_MAX). */
typedef struct {
    long completati;
    long completati_entro_scadenza;
    long somma_tempo_attraversamento;
    int  tempo_minimo;   /**< -1 se nessun oggetto ancora registrato per questa priorità. */
    int  tempo_massimo;
} prioritaStat_t;

struct statistiche {
    bufferStat_t *buffer;
    motoreStat_t *motori;
    ispStat_t *isp;
    presenzaStat_t *presenza;
    prioritaStat_t priorita[PRIORITY_MAX + 1];
    long blocchi_non_monitorati;  /**< Blocchi su buffer non presenti tra quelli monitorati. */
};

/* ------------------------------------------------------------------ */
/*  RICERCA INTERNA (stesso schema di findBufferSensorAssoc in Controllore.c) */
/* ------------------------------------------------------------------ */

static bufferStat_t *trovaBufferStat( statistiche_t *s, const char *ID )
{
    bufferStat_t *cur;
    for ( cur = s->buffer; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->ID, ID ) == 0 ) { return cur; }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  CREAZIONE / DISTRUZIONE                                            */
/* ------------------------------------------------------------------ */

statistiche_t *statistiche_create( short int *errCode )
{
    statistiche_t *s;
    int p;

    s = malloc( sizeof( statistiche_t ) );
    if ( s == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_ALLOC; }
        return NULL;
    }

    s->buffer = NULL;
    s->motori = NULL;
    s->isp = NULL;
    s->presenza = NULL;
    s->blocchi_non_monitorati = 0;

    for ( p = 0; p <= PRIORITY_MAX; p++ ) {
        s->priorita[p].completati = 0;
        s->priorita[p].completati_entro_scadenza = 0;
        s->priorita[p].somma_tempo_attraversamento = 0;
        s->priorita[p].tempo_minimo = -1;
        s->priorita[p].tempo_massimo = -1;
    }

    if ( errCode != NULL ) { *errCode = OP_SUCCESS; }
    return s;
}

void statistiche_destroy( statistiche_t *s )
{
    bufferStat_t *bCur, *bNext;
    motoreStat_t *mCur, *mNext;
    ispStat_t *iCur, *iNext;
    presenzaStat_t *pCur, *pNext;

    if ( s == NULL ) {
        return;
    }

    for ( bCur = s->buffer; bCur != NULL; bCur = bNext ) { bNext = bCur->next; free( bCur ); }
    for ( mCur = s->motori; mCur != NULL; mCur = mNext ) { mNext = mCur->next; free( mCur ); }
    for ( iCur = s->isp; iCur != NULL; iCur = iNext ) { iNext = iCur->next; free( iCur ); }
    for ( pCur = s->presenza; pCur != NULL; pCur = pNext ) { pNext = pCur->next; free( pCur ); }

    free( s );
}

/* ------------------------------------------------------------------ */
/*  DICHIARAZIONE DI COSA MONITORARE                                   */
/* ------------------------------------------------------------------ */

short int statistiche_monitoraBuffer( statistiche_t *s, const char *bufferID )
{
    bufferStat_t *node;

    if ( s == NULL || bufferID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( trovaBufferStat( s, bufferID ) != NULL ) {
        return ERR_DUPLICATE;
    }

    node = malloc( sizeof( bufferStat_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    strncpy( node->ID, bufferID, IDLENGTH - 1 );
    node->ID[IDLENGTH - 1] = '\0';
    node->somma_percentuale = 0;
    node->campioni = 0;
    node->massimo_percentuale = 0;
    node->blocchi = 0;

    node->next = s->buffer;
    s->buffer = node;

    return OP_SUCCESS;
}

short int statistiche_monitoraMotore( statistiche_t *s, const char *targetID )
{
    motoreStat_t *cur;
    motoreStat_t *node;

    if ( s == NULL || targetID == NULL ) {
        return ERR_NULL_PTR;
    }
    for ( cur = s->motori; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->ID, targetID ) == 0 ) { return ERR_DUPLICATE; }
    }

    node = malloc( sizeof( motoreStat_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    strncpy( node->ID, targetID, IDLENGTH - 1 );
    node->ID[IDLENGTH - 1] = '\0';

    node->next = s->motori;
    s->motori = node;

    return OP_SUCCESS;
}

short int statistiche_monitoraISP( statistiche_t *s, const char *ispID )
{
    ispStat_t *cur;
    ispStat_t *node;

    if ( s == NULL || ispID == NULL ) {
        return ERR_NULL_PTR;
    }
    for ( cur = s->isp; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->ID, ispID ) == 0 ) { return ERR_DUPLICATE; }
    }

    node = malloc( sizeof( ispStat_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    strncpy( node->ID, ispID, IDLENGTH - 1 );
    node->ID[IDLENGTH - 1] = '\0';

    node->next = s->isp;
    s->isp = node;

    return OP_SUCCESS;
}

short int statistiche_monitoraSensorePresenza( statistiche_t *s, const char *ID )
{
    presenzaStat_t *cur;
    presenzaStat_t *node;

    if ( s == NULL || ID == NULL ) {
        return ERR_NULL_PTR;
    }
    for ( cur = s->presenza; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->ID, ID ) == 0 ) { return ERR_DUPLICATE; }
    }

    node = malloc( sizeof( presenzaStat_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    strncpy( node->ID, ID, IDLENGTH - 1 );
    node->ID[IDLENGTH - 1] = '\0';

    node->next = s->presenza;
    s->presenza = node;

    return OP_SUCCESS;
}

/* ------------------------------------------------------------------ */
/*  RACCOLTA DATI (da chiamare durante la simulazione)                 */
/* ------------------------------------------------------------------ */

short int statistiche_campiona( statistiche_t *s, const controllore_t *ctrl )
{
    bufferStat_t *cur;
    int perc;

    if ( s == NULL || ctrl == NULL ) {
        return ERR_NULL_PTR;
    }

    for ( cur = s->buffer; cur != NULL; cur = cur->next ) {
        perc = controllore_getPercentualeBuffer( ctrl, cur->ID );
        if ( perc < 0 ) {
            /* Nessun SensoreBuffer agganciato a questo ID (vedi
             * controllore_collegaSensoreBuffer): saltiamo il campione
             * invece di contarlo come 0%, per non falsare la media. */
            continue;
        }
        cur->somma_percentuale += perc;
        cur->campioni++;
        if ( perc > cur->massimo_percentuale ) {
            cur->massimo_percentuale = perc;
        }
    }

    return OP_SUCCESS;
}

short int statistiche_registraBlocco( statistiche_t *s, const char *bufferID )
{
    bufferStat_t *b;

    if ( s == NULL || bufferID == NULL ) {
        return ERR_NULL_PTR;
    }

    b = trovaBufferStat( s, bufferID );
    if ( b != NULL ) {
        b->blocchi++;
    } else {
        s->blocchi_non_monitorati++;
    }

    return OP_SUCCESS;
}

short int statistiche_registraCompletamento( statistiche_t *s, const object_t *obj, int scadenza_passi )
{
    int stepOut;
    int stepCreation;
    int tempo;
    short int priorita;
    prioritaStat_t *ps;

    if ( s == NULL || obj == NULL ) {
        return ERR_NULL_PTR;
    }

    stepOut = object_getStepOut( obj );
    if ( stepOut == STEP_OUT_NONE ) {
        return OP_SUCCESS; /* non ancora completato: ignorato silenziosamente, vedi doc in statistiche.h */
    }

    stepCreation = object_getStepCreation( obj );
    tempo = stepOut - stepCreation;

    priorita = object_getPriority( obj );
    if ( priorita < 0 || priorita > PRIORITY_MAX ) {
        return ERR_OUT_OF_RANGE; /* priorita' fuori dal range atteso, object_create non dovrebbe permetterlo */
    }

    ps = &s->priorita[priorita];
    ps->completati++;
    ps->somma_tempo_attraversamento += tempo;
    if ( ps->tempo_minimo == -1 || tempo < ps->tempo_minimo ) { ps->tempo_minimo = tempo; }
    if ( ps->tempo_massimo == -1 || tempo > ps->tempo_massimo ) { ps->tempo_massimo = tempo; }

    if ( scadenza_passi > 0 && tempo <= scadenza_passi ) {
        ps->completati_entro_scadenza++;
    }

    return OP_SUCCESS;
}

/* ------------------------------------------------------------------ */
/*  STAMPA                                                             */
/* ------------------------------------------------------------------ */

void statistiche_stampa( const statistiche_t *s, const controllore_t *ctrl, int n_step_simulazione )
{
    bufferStat_t *bCur;
    motoreStat_t *mCur;
    long totale_completati = 0;
    int p;

    if ( s == NULL ) {
        printf( "statistiche_stampa: statistiche NULL\n" );
        return;
    }

    printf( "\n=== STATISTICHE ===\n" );

    printf( "\n-- Tempo --\n" );
    printf( "  Tempo totale di simulazione: %d passi\n", n_step_simulazione );

    printf( "\n-- Occupazione buffer (media/massima nel tempo, su tutti i campioni raccolti) --\n" );
    if ( s->buffer == NULL ) {
        printf( "  (nessun buffer monitorato: vedi statistiche_monitoraBuffer)\n" );
    }
    for ( bCur = s->buffer; bCur != NULL; bCur = bCur->next ) {
        double media = ( bCur->campioni > 0 ) ? ( (double) bCur->somma_percentuale / bCur->campioni ) : 0.0;
        printf( "  %-16s media=%5.1f%%  massima=%3d%%  campioni=%-4d  blocchi=%ld\n",
                bCur->ID, media, bCur->massimo_percentuale, bCur->campioni, bCur->blocchi );
    }
    if ( s->blocchi_non_monitorati > 0 ) {
        printf( "  (altri %ld blocchi su buffer non monitorati)\n", s->blocchi_non_monitorati );
    }

    printf( "\n-- Motori (tempo cumulativo ON/OFF, in passi di simulazione) --\n" );
    if ( s->motori == NULL ) {
        printf( "  (nessun motore monitorato: vedi statistiche_monitoraMotore)\n" );
    }
    for ( mCur = s->motori; mCur != NULL; mCur = mCur->next ) {
        if ( ctrl == NULL ) {
            printf( "  %-8s (controllore non passato a statistiche_stampa: impossibile leggere il tempo)\n", mCur->ID );
            continue;
        }
        long on = controllore_getTempoMotoreOn( ctrl, mCur->ID );
        long off = controllore_getTempoMotoreOff( ctrl, mCur->ID );
        printf( "  %-8s ON=%-5ld OFF=%-5ld totale=%ld\n", mCur->ID, on, off, on + off );
    }

    printf( "\n-- Sensori di qualità (per ISP monitorata) --\n" );
    if ( s->isp == NULL ) {
        printf( "  (nessuna ISP monitorata: vedi statistiche_monitoraISP)\n" );
    }
    for ( ispStat_t *iCur = s->isp; iCur != NULL; iCur = iCur->next ) {
        if ( ctrl == NULL ) {
            printf( "  %-8s (controllore non passato a statistiche_stampa: impossibile leggere)\n", iCur->ID );
            continue;
        }
        long letture = controllore_getLettureQualita( ctrl, iCur->ID );
        if ( letture < 0 ) {
            printf( "  %-8s nessun sensore di qualità agganciato (vedi controllore_collegaSensoreQualita)\n", iCur->ID );
            continue;
        }
        long anomalie = controllore_getAnomalieQualita( ctrl, iCur->ID );
        long tipi[3] = { 0, 0, 0 };
        long matA = controllore_getMaterialeA( ctrl, iCur->ID );
        long matB = controllore_getMaterialeB( ctrl, iCur->ID );
        controllore_getTipoLettureQualita( ctrl, iCur->ID, tipi );
        printf( "  %-8s letture=%-4ld anomalie=%-4ld  CONFORME=%ld RIVALUTAZIONE=%ld SCARTO=%ld  materiale A=%ld B=%ld\n",
                iCur->ID, letture, anomalie, tipi[0], tipi[1], tipi[2], matA, matB );
    }

    printf( "\n-- Sensori di presenza (per ID monitorato) --\n" );
    if ( s->presenza == NULL ) {
        printf( "  (nessun sensore di presenza monitorato: vedi statistiche_monitoraSensorePresenza)\n" );
    }
    for ( presenzaStat_t *pCur = s->presenza; pCur != NULL; pCur = pCur->next ) {
        if ( ctrl == NULL ) {
            printf( "  %-8s (controllore non passato a statistiche_stampa: impossibile leggere)\n", pCur->ID );
            continue;
        }
        long letture = controllore_getLetturePresenza( ctrl, pCur->ID );
        if ( letture < 0 ) {
            printf( "  %-8s nessun sensore di presenza agganciato (vedi controllore_collegaSensorePresenza)\n", pCur->ID );
            continue;
        }
        long rilevamenti = controllore_getRilevamentiPresenza( ctrl, pCur->ID );
        printf( "  %-8s letture=%-4ld  rilevamenti (nuovi arrivi distinti)=%ld\n", pCur->ID, letture, rilevamenti );
    }

    printf( "\n-- Tempo di attraversamento e completamento, per classe di priorità --\n" );
    for ( p = 0; p <= PRIORITY_MAX; p++ ) {
        const prioritaStat_t *ps = &s->priorita[p];
        if ( ps->completati == 0 ) {
            continue; /* nessun oggetto di questa priorita': non stampiamo una riga vuota */
        }
        double media_tempo = (double) ps->somma_tempo_attraversamento / ps->completati;
        double perc_entro_scadenza = 100.0 * ps->completati_entro_scadenza / ps->completati;
        printf( "  priorita' %2d: completati=%-4ld  tempo(min/media/max)=%d/%.1f/%d  entro scadenza=%.0f%%\n",
                p, ps->completati, ps->tempo_minimo, media_tempo, ps->tempo_massimo, perc_entro_scadenza );
        totale_completati += ps->completati;
    }
    printf( "  TOTALE completati (con tempo di attraversamento registrato): %ld\n", totale_completati );
}
