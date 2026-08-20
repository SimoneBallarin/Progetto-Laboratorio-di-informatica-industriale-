/**
 * @file main.c
 * @brief Main della cella meccatronica, per il layout:
 *
 *   [ingresso] -> B1 -> ISP1 -[N1]-> M -> B2 -> ISP2 -> {Alacciaio, riqualifica, TRASH, Blrame}
 *
 * ASSUNZIONI FATTE (da confermare/correggere col gruppo):
 *   - ISP1 ("che materiale è"): non fa alcun controllo qualità reale.
 *     Il materiale è già scritto su object->type alla creazione
 *     dell'oggetto (object_create), quindi ISP1 è semplicemente una
 *     stazione con tempo di attraversamento fisso, un solo ingresso e
 *     una sola uscita (verso N1) — l'esito che calcola internamente
 *     (S_Qualita è comunque agganciato, perché isp_t lo richiede) non
 *     viene usato per instradare, dato che ha un'unica uscita.
 *   - Un solo Nastro (N1) nel layout, tra ISP1 e M, con Motore collegato
 *     (arco blu nel disegno). M -> B2 è un collegamento diretto (arco
 *     grigio, nessun nastro dedicato nel disegno).
 *   - ISP2 (quella con 4 uscite) ha il Deviatore collegato. L'ordine dei
 *     collegamenti verso le uscite è FISSO e deve rispettare la
 *     convenzione aggiunta in Controllore.c:
 *         indice 0 -> CONFORME materiale 'A' -> Alacciaio
 *         indice 1 -> RIVALUTAZIONE           -> riqualifica
 *         indice 2 -> SCARTO                  -> TRASH
 *         indice 3 -> CONFORME materiale 'B'  -> Blrame
 *   - Ogni buffer ha un SensoreBuffer automatico (lo crea
 *     controllore_create per tutti i buffer già presenti nella cella in
 *     quel momento: per questo B1/B2/le 4 uscite vanno aggiunte PRIMA di
 *     chiamare controllore_create).
 *   - Il sensore di presenza (S_Presenza) è collegato solo a B1, l'unico
 *     punto di ingresso di nuovi oggetti dall'esterno (sez. 5.1 del
 *     progetto: "Sensore di presenza in B1"), tramite
 *     controllore_segnalaArrivo ad ogni passo in cui arriva un pezzo.
 */

#include <stdio.h>
#include <stdlib.h>

#include "object.h"
#include "errors.h"
#include "cell.h"
#include "Controllore.h"

/* Parametri di configurazione della cella (valori di esempio: da
 * spostare su file di configurazione quando il parser sarà pronto). */
#define CAPACITA_B1        60
#define CAPACITA_B2        60
#define CAPACITA_USCITE    60
#define TEMPO_ISP1         1     /* ISP1: solo tag materiale, veloce */
#define TEMPO_M             3
#define TEMPO_ISP2          2
#define CAPACITA_N1         2
#define VELOCITA_N1         2
#define VELOCITA_MOTORE_N1  5000
#define ACCEL_MOTORE_N1     2000
#define VELOCITA_MOTORE_M  5000
#define ACCEL_MOTORE_M     2000
#define TEMPO_MIN_COMMUT    3
#define SOGLIA_BUFFER       0.8
#define DIMENSIONX_TARGET_ISP2  80
#define RAGGIO_TARGET_ISP2      6
/* ISP1 non giudica la qualità: un target qualsiasi (>0) va bene, tanto
 * il suo esito non viene usato per instradare (ha una sola uscita). */
#define DIMENSIONX_TARGET_ISP1  100
#define RAGGIO_TARGET_ISP1      10

#define N_STEP_SIMULAZIONE  120

/* Numero di pezzi di prova generati in ingresso a B1. Se supera la
 * capacità di B1 (CAPACITA_B1), i pezzi in eccesso vengono scartati con
 * un messaggio "B1 pieno" (vedi genera_arrivi_esempio) - non è un errore
 * bloccante, ma un modo per osservare quel caso limite se lo si vuole. */
#define N_PEZZI_PROVA       30

static int costruisci_cella( cell_t *cell )
{
    short int err;

    if ( cell_addBuffer( cell, "B1", CAPACITA_B1, &err ) == NULL ) {
        fprintf( stderr, "Errore creazione B1: %d\n", err );
        return 0;
    }
    if ( cell_addISP( cell, "ISP1", TEMPO_ISP1, &err ) == NULL ) {
        fprintf( stderr, "Errore creazione ISP1: %d\n", err );
        return 0;
    }
    if ( cell_addNastro( cell, "N1", CAPACITA_N1, VELOCITA_N1, &err ) == NULL ) {
        fprintf( stderr, "Errore creazione N1: %d\n", err );
        return 0;
    }
    if ( cell_addMachine( cell, "M", TEMPO_M, &err ) == NULL ) {
        fprintf( stderr, "Errore creazione M: %d\n", err );
        return 0;
    }
    if ( cell_addBuffer( cell, "B2", CAPACITA_B2, &err ) == NULL ) {
        fprintf( stderr, "Errore creazione B2: %d\n", err );
        return 0;
    }
    if ( cell_addISP( cell, "ISP2", TEMPO_ISP2, &err ) == NULL ) {
        fprintf( stderr, "Errore creazione ISP2: %d\n", err );
        return 0;
    }
    if ( cell_addBuffer( cell, "B_Alacciaio", CAPACITA_USCITE, &err ) == NULL ) {
        fprintf( stderr, "Errore creazione B_Alacciaio: %d\n", err );
        return 0;
    }
    if ( cell_addBuffer( cell, "B_riqualifica", CAPACITA_USCITE, &err ) == NULL ) {
        fprintf( stderr, "Errore creazione B_riqualifica: %d\n", err );
        return 0;
    }
    if ( cell_addBuffer( cell, "B_TRASH", CAPACITA_USCITE, &err ) == NULL ) {
        fprintf( stderr, "Errore creazione B_TRASH: %d\n", err );
        return 0;
    }
    if ( cell_addBuffer( cell, "B_rame", CAPACITA_USCITE, &err ) == NULL ) {
        fprintf( stderr, "Errore creazione B_rame: %d\n", err );
        return 0;
    }

    /* Collegamenti: l'ordine con cui si collegano le 4 uscite di ISP2 è
     * significativo (vedi convenzione sopra e in Controllore.c). */
    if ( cell_connect( cell, "B1", "ISP1" ) != OP_SUCCESS ) { return 0; }
    if ( cell_connect( cell, "ISP1", "N1" ) != OP_SUCCESS ) { return 0; }
    if ( cell_connect( cell, "N1", "M" ) != OP_SUCCESS ) { return 0; }
    if ( cell_connect( cell, "M", "B2" ) != OP_SUCCESS ) { return 0; }
    if ( cell_connect( cell, "B2", "ISP2" ) != OP_SUCCESS ) { return 0; }
    if ( cell_connect( cell, "ISP2", "B_Alacciaio" ) != OP_SUCCESS ) { return 0; }    /* indice 0 */
    if ( cell_connect( cell, "ISP2", "B_riqualifica" ) != OP_SUCCESS ) { return 0; }  /* indice 1 */
    if ( cell_connect( cell, "ISP2", "B_TRASH" ) != OP_SUCCESS ) { return 0; }        /* indice 2 */
    if ( cell_connect( cell, "ISP2", "B_rame" ) != OP_SUCCESS ) { return 0; }         /* indice 3 */

    return OP_SUCCESS;
}

static int collega_attuatori( controllore_t *ctrl )
{
    short int err;
    const char *buffer_ids[] = { "B1", "B2", "B_Alacciaio", "B_riqualifica", "B_TRASH", "B_rame" };
    int nb = (int) ( sizeof( buffer_ids ) / sizeof( buffer_ids[0] ) );
    int bi;

    err = controllore_collegaMotore( ctrl, "N1", VELOCITA_MOTORE_N1, ACCEL_MOTORE_N1 );
    if ( err != OP_SUCCESS ) {
        fprintf( stderr, "Errore collegamento Motore su N1: %d\n", err );
        return 0;
    }
    
    err = controllore_collegaMotore( ctrl, "M", VELOCITA_MOTORE_M, ACCEL_MOTORE_M );
    if ( err != OP_SUCCESS ) {
        fprintf( stderr, "Errore collegamento Motore su M: %d\n", err );
        return 0;
    }

    err = controllore_collegaDeviatore( ctrl, "ISP2", TEMPO_MIN_COMMUT );
    if ( err != OP_SUCCESS ) {
        fprintf( stderr, "Errore collegamento Deviatore su ISP2: %d\n", err );
        return 0;
    }

    /* Sensore di qualità agganciato SOLO a ISP2: ISP1 resta una ISP
     * "passacarte" (puro timer, nessun giudizio di qualità), senza più
     * bisogno di nessun target fittizio - semplicemente non ha nessun
     * sensore agganciato. */
    err = controllore_collegaSensoreQualita( ctrl, "ISP2", DIMENSIONX_TARGET_ISP2, RAGGIO_TARGET_ISP2 );
    if ( err != OP_SUCCESS ) {
        fprintf( stderr, "Errore collegamento SensoreQualita su ISP2: %d\n", err );
        return 0;
    }
    
    err = controllore_collegaSensoreQualita( ctrl, "ISP1", DIMENSIONX_TARGET_ISP2, RAGGIO_TARGET_ISP2 );
    if ( err != OP_SUCCESS ) {
        fprintf( stderr, "Errore collegamento SensoreQualita su ISP1: %d\n", err );
        return 0;
    }


    /* Sensore buffer su tutti e 6 i buffer: prima della versione con
     * aggancio esplicito, il controllore ne creava uno in automatico
     * per ogni buffer già presente al momento di controllore_create -
     * ora va fatto qui, esplicitamente, uno per uno. */
    for ( bi = 0; bi < nb; bi++ ) {
        err = controllore_collegaSensoreBuffer( ctrl, buffer_ids[bi] );
        if ( err != OP_SUCCESS ) {
            fprintf( stderr, "Errore collegamento SensoreBuffer su %s: %d\n", buffer_ids[bi], err );
            return 0;
        }
    }

    /* Sensore di presenza su B1 (unico punto di ingresso dall'esterno,
     * sez. 5.1 del progetto): va agganciato PRIMA di poter chiamare
     * controllore_segnalaArrivo per lo stesso ID. */
    err = controllore_collegaSensorePresenza( ctrl, "B1" );
    if ( err != OP_SUCCESS ) {
        fprintf( stderr, "Errore collegamento SensorePresenza su B1: %d\n", err );
        return 0;
    }

    return OP_SUCCESS;
}

/* Genera qualche oggetto di esempio in ingresso a B1, alternando
 * materiale e "conformità" (vicinanza al target di ISP2), giusto per
 * vedere la linea muoversi. Il vero flusso di arrivi verrà dal file di
 * configurazione, quando il parser sarà pronto. */
static void genera_arrivi_esempio( cell_t *cell, controllore_t *ctrl, int step_arrivo,
                                    const char *id, char tipo, double dimensionX, double raggio )
{
    short int err;
    object_t *obj;
    buffer_t *b1;

    b1 = cell_getBuffer( cell, "B1" );
    if ( b1 == NULL || buffer_isFull( b1 ) ) {
        fprintf( stderr, "B1 pieno o inesistente: arrivo %s scartato\n", id );
        return;
    }
    short int priorita =  (short int)(rand()%11);
    obj = object_create( id, priorita, tipo, step_arrivo, dimensionX, raggio, &err );
    if ( obj == NULL ) {
        fprintf( stderr, "Errore creazione oggetto %s: %d\n", id, err );
        return;
    }

    if ( buffer_insertObject( b1, obj, true ) != OP_SUCCESS ) {
        fprintf( stderr, "Errore inserimento %s in B1\n", id );
        object_delete( obj );
        return;
    }
    object_setLocation( obj, "B1" );

    /* Sensore di presenza in B1 (sez. 5.1): segnaliamo l'arrivo come
     * fronte di salita (presenza=1) in questo passo. */
    controllore_segnalaArrivo( ctrl, "B1", 0, 1 );
    controllore_segnalaArrivo( ctrl, "B1", 0, 0 );
}


// INIZIO MAIN //

int main( void )
{
    cell_t *cell;
    controllore_t *ctrl;
    short int err;
    int step;

    cell = cell_create();
    if ( cell == NULL ) {
        fprintf( stderr, "Errore creazione cella\n" );
        return OP_SUCCESS;
    }

    if ( !costruisci_cella( cell ) ) {
        cell_destroy( cell );
        return OP_SUCCESS;
    }

    /* Il controllore va creato DOPO aver aggiunto tutti i buffer: crea
     * automaticamente un SensoreBuffer per ognuno di quelli già presenti
     * in questo momento (vedi Controllore.h). */
    ctrl = controllore_create( cell, SOGLIA_BUFFER, &err );
    if ( ctrl == NULL ) {
        fprintf( stderr, "Errore creazione controllore: %d\n", err );
        cell_destroy( cell );
        return OP_SUCCESS;
    }

    if ( !collega_attuatori( ctrl ) ) {
        controllore_destroy( ctrl );
        cell_destroy( cell );
        return OP_SUCCESS;
    }

    {
        char id[16];
        int idx;
        for ( idx = 0; idx < N_PEZZI_PROVA; idx++ ) {
            char tipo;
            if (rand() % 2 == 0) {
                tipo = 'A';
             } else {
                tipo = 'B';
              }
            double  scarto_pct = ((rand() % 5) - 2);
            double dimensionX = DIMENSIONX_TARGET_ISP1 + ((DIMENSIONX_TARGET_ISP1*scarto_pct )/100);
            double raggio     = RAGGIO_TARGET_ISP1 + ((RAGGIO_TARGET_ISP1 * scarto_pct )/100);

            snprintf( id, sizeof( id ), "P%d", idx + 1 );
            genera_arrivi_esempio( cell, ctrl, idx, id, tipo, dimensionX, raggio );
        }
    }

    printf( "=== Stato iniziale (dopo la creazione degli oggetti, prima di far girare la simulazione) ===\n" );
    controllore_print( ctrl );
    printf( "\n" );

    for ( step = 0; step < N_STEP_SIMULAZIONE; step++ ) {
        controllore_step( ctrl, step );
    }

    printf( "=== Stato finale (dopo %d passi) ===\n", N_STEP_SIMULAZIONE );
    controllore_print( ctrl );
    printf( "Completati: %ld, ancora in coda (pending): %d\n",
            controllore_getCompletati( ctrl ), controllore_getPendingCount( ctrl ) );

    // STATISTICHE //

    /* NB: la pulizia degli object_t inseriti resta da fare (nessun
     * modulo del progetto li libera automaticamente): qui li lasciamo
     * volutamente, dato che serve prima decidere chi ne è responsabile
     * a fine simulazione (o se il progetto li considera "persi" nei
     * buffer di uscita finché non c'è un log/registro dedicato). */

    controllore_destroy( ctrl );
    cell_destroy( cell );

    return 0;
}
