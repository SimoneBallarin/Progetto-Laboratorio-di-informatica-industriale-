/**
 * @file statistiche.c
 * @brief Implementazione del modulo statistiche.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

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

/** @brief Aggregazione per una singola classe di priorità (0..PRIORITY_MAX).
 *
 * NOMENCLATURA (resa esplicita per evitare l'ambiguità che il campo
 * originale "somma_tempo_attraversamento" causava - il nome suggeriva
 * "solo attraversamento", ma il valore era sempre stato il tempo TOTALE
 * in sistema, coda inclusa):
 *   - "tempo_sistema"    = stepOut - stepCreation: tempo TOTALE, dalla
 *     coda di ingresso all'uscita da un buffer terminale (coda + processo).
 *   - "tempo_processo"   = stepOut - stepPartial: SOLO la pipeline vera e
 *     propria (ISP1->N1->M->B2->ISP2), ESCLUSA l'attesa in coda su B1.
 *   - "tempo_attesa"     = stepPartial - stepCreation: SOLO l'attesa in
 *     coda su B1, prima che l'oggetto inizi ad essere processato.
 * Vale sempre: tempo_sistema = tempo_attesa + tempo_processo.
 * La "scadenza" (statistiche_registraCompletamento) viene confrontata
 * con ENTRAMBI tempo_sistema e tempo_processo, tenuti come due contatori
 * separati (completati_entro_scadenza_sistema/_processo): non esiste una
 * "scadenza" unica implicita, il chiamante/lettore decide quale delle
 * due gli interessa.
 */
typedef struct {
    long completati;
    long completati_entro_scadenza_sistema;    /**< tempo_sistema <= scadenza_passi. */
    long somma_tempo_sistema;                   /**< Somma di tempo_sistema (TOTALE: coda + processo). */
    int  tempo_sistema_minimo;   /**< -1 se nessun oggetto ancora registrato per questa priorità. */
    int  tempo_sistema_massimo;
    /* Scomposizione del tempo totale (vedi object_getStepPartial):
     * quanto di quel tempo e' stato speso in coda nel buffer di ingresso
     * prima di iniziare la lavorazione, e quanto nella pipeline vera e
     * propria. "conteggiati" puo' differire da "completati" se per
     * qualche oggetto stepPartial non risultasse mai impostato
     * (non dovrebbe succedere per un oggetto arrivato a un buffer
     * terminale, ma viene gestito comunque per robustezza). */
    long completati_con_scomposizione;
    long completati_entro_scadenza_processo;    /**< tempo_processo <= scadenza_passi (solo tra i "con_scomposizione"). */
    long somma_tempo_attesa;                     /**< Somma di tempo_attesa (SOLO coda su B1). */
    long somma_tempo_processo;                   /**< Somma di tempo_processo (SOLO pipeline, ESCLUSA coda). */
    int  tempo_processo_minimo;                  /**< -1 se nessun oggetto ancora "con_scomposizione". */
    int  tempo_processo_massimo;
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
        s->priorita[p].completati_entro_scadenza_sistema = 0;
        s->priorita[p].somma_tempo_sistema = 0;
        s->priorita[p].tempo_sistema_minimo = -1;
        s->priorita[p].tempo_sistema_massimo = -1;
        s->priorita[p].completati_con_scomposizione = 0;
        s->priorita[p].completati_entro_scadenza_processo = 0;
        s->priorita[p].somma_tempo_attesa = 0;
        s->priorita[p].somma_tempo_processo = 0;
        s->priorita[p].tempo_processo_minimo = -1;
        s->priorita[p].tempo_processo_massimo = -1;
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

short int statistiche_campiona( statistiche_t *s, controllore_t *ctrl )
{
    bufferStat_t *cur;
    int perc;
    int perc_picco;

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

        /* Per la "massima" usiamo il picco TRANSITORIO (vedi
         * controllore_getPercentualePiccoBuffer), non il livello
         * istantaneo: un buffer riempito e svuotato nello stesso passo
         * (es. tra macchina e ISP successiva) risulterebbe sempre 0% se
         * letto con un singolo poll a fine passo, anche se per un
         * istante ha davvero contenuto un oggetto. Questa chiamata
         * resetta la finestra di osservazione: va fatta una volta sola
         * per campionamento (qui), non ripetuta altrove. */
        perc_picco = controllore_getPercentualePiccoBuffer( ctrl, cur->ID );
        if ( perc_picco > cur->massimo_percentuale ) {
            cur->massimo_percentuale = perc_picco;
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
    int stepPartial;
    int tempo_sistema;
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
    tempo_sistema = stepOut - stepCreation;   /* TOTALE: coda + processo, vedi doc di prioritaStat_t */

    priorita = object_getPriority( obj );
    if ( priorita < 0 || priorita > PRIORITY_MAX ) {
        return ERR_OUT_OF_RANGE; /* priorita' fuori dal range atteso, object_create non dovrebbe permetterlo */
    }

    ps = &s->priorita[priorita];
    ps->completati++;
    ps->somma_tempo_sistema += tempo_sistema;
    if ( ps->tempo_sistema_minimo == -1 || tempo_sistema < ps->tempo_sistema_minimo ) { ps->tempo_sistema_minimo = tempo_sistema; }
    if ( ps->tempo_sistema_massimo == -1 || tempo_sistema > ps->tempo_sistema_massimo ) { ps->tempo_sistema_massimo = tempo_sistema; }

    if ( scadenza_passi > 0 && tempo_sistema <= scadenza_passi ) {
        ps->completati_entro_scadenza_sistema++;
    }

    /* Scomposizione tempo di attesa (coda nel buffer di ingresso) /
     * tempo di processo (pipeline vera e propria), vedi
     * object_getStepPartial. Se per qualche motivo non risulta
     * mai impostato (STEP_OUT_NONE), l'oggetto viene escluso solo da
     * QUESTA scomposizione: resta comunque conteggiato in "completati"
     * e nel tempo di sistema sopra.
     *
     * La stessa scadenza_passi viene confrontata ANCHE col solo
     * tempo_processo (senza l'attesa in coda): un oggetto può quindi
     * essere "entro scadenza" secondo una definizione e non secondo
     * l'altra - vedi statistiche_riepilogo_t in statistiche.h per come
     * i due contatori vengono esposti separatamente. */
    stepPartial = object_getStepPartial( obj );
    if ( stepPartial != STEP_OUT_NONE ) {
        int tempo_processo = stepOut - stepPartial;

        ps->completati_con_scomposizione++;
        ps->somma_tempo_attesa   += ( stepPartial - stepCreation );
        ps->somma_tempo_processo += tempo_processo;
        if ( ps->tempo_processo_minimo == -1 || tempo_processo < ps->tempo_processo_minimo ) { ps->tempo_processo_minimo = tempo_processo; }
        if ( ps->tempo_processo_massimo == -1 || tempo_processo > ps->tempo_processo_massimo ) { ps->tempo_processo_massimo = tempo_processo; }

        if ( scadenza_passi > 0 && tempo_processo <= scadenza_passi ) {
            ps->completati_entro_scadenza_processo++;
        }
    }

    return OP_SUCCESS;
}

/* ------------------------------------------------------------------ */
/*  STAMPA                                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Scrive in stile printf su file (se f non è NULL) e, se
 *        anche_su_stdout è true, anche su stdout - stessa riga, stesso
 *        contenuto, fino a due destinazioni. Helper interno usato da
 *        statistiche_stampa per non raddoppiare ogni singola chiamata.
 */
static void stampa_dual( bool anche_su_stdout, FILE *f, const char *formato, ... )
{
    va_list args;

    if ( anche_su_stdout ) {
        va_start( args, formato );
        vprintf( formato, args );
        va_end( args );
    }

    if ( f != NULL ) {
        va_start( args, formato );
        vfprintf( f, formato, args );
        va_end( args );
    }
}

void statistiche_stampa( const statistiche_t *s, const controllore_t *ctrl, int n_step_simulazione,
                          const char *path_output, bool anche_su_stdout )
{
    bufferStat_t *bCur;
    motoreStat_t *mCur;
    long totale_completati = 0;
    int p;
    FILE *f = NULL;

    if ( path_output != NULL ) {
        f = fopen( path_output, "w" );
        if ( f == NULL ) {
            fprintf( stderr, "statistiche_stampa: impossibile aprire '%s'%s\n", path_output,
                     anche_su_stdout ? ", si procede solo su stdout" : "" );
        }
    }

    if ( s == NULL ) {
        stampa_dual( anche_su_stdout, f, "statistiche_stampa: statistiche NULL\n" );
        if ( f != NULL ) { fclose( f ); }
        return;
    }

    stampa_dual( anche_su_stdout, f, "\n=== STATISTICHE ===\n" );

    stampa_dual( anche_su_stdout, f, "\n-- Tempo --\n" );
    stampa_dual( anche_su_stdout, f, "  Tempo totale di simulazione: %d passi\n", n_step_simulazione );

    stampa_dual( anche_su_stdout, f, "\n-- Occupazione buffer (media/massima nel tempo, su tutti i campioni raccolti) --\n" );
    if ( s->buffer == NULL ) {
        stampa_dual( anche_su_stdout, f, "  (nessun buffer monitorato: vedi statistiche_monitoraBuffer)\n" );
    }
    for ( bCur = s->buffer; bCur != NULL; bCur = bCur->next ) {
        double media = ( bCur->campioni > 0 ) ? ( (double) bCur->somma_percentuale / bCur->campioni ) : 0.0;
        stampa_dual( anche_su_stdout, f, "  %-16s media=%5.1f%%  massima=%3d%%  campioni=%-4d  blocchi=%ld\n",
                bCur->ID, media, bCur->massimo_percentuale, bCur->campioni, bCur->blocchi );
    }
    if ( s->blocchi_non_monitorati > 0 ) {
        stampa_dual( anche_su_stdout, f, "  (altri %ld blocchi su buffer non monitorati)\n", s->blocchi_non_monitorati );
    }

    stampa_dual( anche_su_stdout, f, "\n-- Motori (tempo cumulativo ON/OFF, in passi di simulazione) --\n" );
    if ( s->motori == NULL ) {
        stampa_dual( anche_su_stdout, f, "  (nessun motore monitorato: vedi statistiche_monitoraMotore)\n" );
    }
    for ( mCur = s->motori; mCur != NULL; mCur = mCur->next ) {
        if ( ctrl == NULL ) {
            stampa_dual( anche_su_stdout, f, "  %-8s (controllore non passato a statistiche_stampa: impossibile leggere il tempo)\n", mCur->ID );
            continue;
        }
        long on = controllore_getTempoMotoreOn( ctrl, mCur->ID );
        long off = controllore_getTempoMotoreOff( ctrl, mCur->ID );
        stampa_dual( anche_su_stdout, f, "  %-8s ON=%-5ld OFF=%-5ld totale=%ld\n", mCur->ID, on, off, on + off );
    }

    stampa_dual( anche_su_stdout, f, "\n-- Sensori di qualità (per ISP monitorata) --\n" );
    if ( s->isp == NULL ) {
        stampa_dual( anche_su_stdout, f, "  (nessuna ISP monitorata: vedi statistiche_monitoraISP)\n" );
    }
    for ( ispStat_t *iCur = s->isp; iCur != NULL; iCur = iCur->next ) {
        if ( ctrl == NULL ) {
            stampa_dual( anche_su_stdout, f, "  %-8s (controllore non passato a statistiche_stampa: impossibile leggere)\n", iCur->ID );
            continue;
        }
        long letture = controllore_getLettureQualita( ctrl, iCur->ID );
        if ( letture < 0 ) {
            stampa_dual( anche_su_stdout, f, "  %-8s nessun sensore di qualità agganciato (vedi controllore_collegaSensoreQualita)\n", iCur->ID );
            continue;
        }
        long anomalie = controllore_getAnomalieQualita( ctrl, iCur->ID );
        long stepBlocco = controllore_getStepBloccoGuasto( ctrl, iCur->ID );
        long tipi[3] = { 0, 0, 0 };
        long matA = controllore_getMaterialeA( ctrl, iCur->ID );
        long matB = controllore_getMaterialeB( ctrl, iCur->ID );
        long matNonClass = controllore_getMaterialeNonClassificato( ctrl, iCur->ID );
        controllore_getTipoLettureQualita( ctrl, iCur->ID, tipi );
        stampa_dual( anche_su_stdout, f, "  %-8s letture=%-4ld anomalie=%-4ld step_bloccata_per_guasto=%-4ld  CONFORME=%ld RIVALUTAZIONE=%ld SCARTO=%ld  materiale A=%ld B=%ld non_classificato=%ld\n",
                iCur->ID, letture, anomalie, stepBlocco, tipi[0], tipi[1], tipi[2], matA, matB, matNonClass );
    }

    stampa_dual( anche_su_stdout, f, "\n-- Sensori di presenza (per ID monitorato) --\n" );
    if ( s->presenza == NULL ) {
        stampa_dual( anche_su_stdout, f, "  (nessun sensore di presenza monitorato: vedi statistiche_monitoraSensorePresenza)\n" );
    }
    for ( presenzaStat_t *pCur = s->presenza; pCur != NULL; pCur = pCur->next ) {
        if ( ctrl == NULL ) {
            stampa_dual( anche_su_stdout, f, "  %-8s (controllore non passato a statistiche_stampa: impossibile leggere)\n", pCur->ID );
            continue;
        }
        long letture = controllore_getLetturePresenza( ctrl, pCur->ID );
        if ( letture < 0 ) {
            stampa_dual( anche_su_stdout, f, "  %-8s nessun sensore di presenza agganciato (vedi controllore_collegaSensorePresenza)\n", pCur->ID );
            continue;
        }
        long rilevamenti = controllore_getRilevamentiPresenza( ctrl, pCur->ID );
        stampa_dual( anche_su_stdout, f, "  %-8s letture=%-4ld  rilevamenti (nuovi arrivi distinti)=%ld\n", pCur->ID, letture, rilevamenti );
    }

    stampa_dual( anche_su_stdout, f, "\n-- Tempo in sistema e completamento, per classe di priorita' --\n" );
    stampa_dual( anche_su_stdout, f, "   (SISTEMA = coda di ingresso + pipeline; PROCESSO = solo pipeline, ESCLUSA la coda)\n" );
    for ( p = 0; p <= PRIORITY_MAX; p++ ) {
        const prioritaStat_t *ps = &s->priorita[p];
        if ( ps->completati == 0 ) {
            continue; /* nessun oggetto di questa priorita': non stampiamo una riga vuota */
        }
        double media_sistema = (double) ps->somma_tempo_sistema / ps->completati;
        double perc_entro_scadenza_sistema = 100.0 * ps->completati_entro_scadenza_sistema / ps->completati;
        stampa_dual( anche_su_stdout, f, "  priorita' %2d: completati=%-4ld  tempo SISTEMA(min/media/max)=%d/%.1f/%d  entro scadenza(SISTEMA)=%.0f%%\n",
                p, ps->completati, ps->tempo_sistema_minimo, media_sistema, ps->tempo_sistema_massimo, perc_entro_scadenza_sistema );
        if ( ps->completati_con_scomposizione > 0 ) {
            double media_attesa = (double) ps->somma_tempo_attesa   / ps->completati_con_scomposizione;
            double media_processo = (double) ps->somma_tempo_processo / ps->completati_con_scomposizione;
            double perc_entro_scadenza_processo = 100.0 * ps->completati_entro_scadenza_processo / ps->completati_con_scomposizione;
            stampa_dual( anche_su_stdout, f, "                 di cui (media): attesa in coda=%.1f + PROCESSO=%.1f = %.1f  |  "
                             "PROCESSO(min/max)=%d/%d  entro scadenza(PROCESSO)=%.0f%%\n",
                    media_attesa, media_processo, media_attesa + media_processo,
                    ps->tempo_processo_minimo, ps->tempo_processo_massimo, perc_entro_scadenza_processo );
        }
        totale_completati += ps->completati;
    }
    stampa_dual( anche_su_stdout, f, "  TOTALE completati (con tempo in sistema registrato): %ld\n", totale_completati );

    if ( f != NULL ) {
        fclose( f );
    }
}

short int statistiche_getRiepilogo( const statistiche_t *s, const controllore_t *ctrl, statistiche_riepilogo_t *out )
{
    bufferStat_t *bCur;
    ispStat_t *iCur;
    int p;
    long somma_tempo_sistema_pesata = 0;
    long somma_entro_scadenza_sistema = 0;
    long somma_tempo_processo_pesata = 0;
    long somma_entro_scadenza_processo = 0;
    long totale_con_scomposizione = 0;
    double somma_medie_buffer = 0.0;
    int n_buffer_monitorati = 0;

    if ( s == NULL || ctrl == NULL || out == NULL ) {
        return ERR_NULL_PTR;
    }

    out->totale_completati = 0;
    out->perc_entro_scadenza_tempo_sistema = 0.0;
    out->tempo_medio_sistema = 0.0;
    out->perc_entro_scadenza_tempo_processo = 0.0;
    out->tempo_medio_processo = 0.0;
    out->occupazione_media_buffer = 0.0;
    out->occupazione_massima_buffer = 0;
    out->totale_blocchi = s->blocchi_non_monitorati;
    out->totale_anomalie_qualita = 0;
    out->pending_finale = controllore_getPendingCount( ctrl );
    out->completati_alta_priorita = 0;
    out->perc_entro_scadenza_alta_priorita = 0.0;
    out->completati_bassa_priorita = 0;
    out->perc_entro_scadenza_bassa_priorita = 0.0;

    {
        long entro_scadenza_alta = 0;
        long entro_scadenza_bassa = 0;

        /* Stesse somme di statistiche_stampa (sezione "Tempo in sistema e
         * completamento"), ma aggregate su tutte le priorità invece che
         * stampate una riga alla volta - più la scomposizione alta/bassa
         * priorità (vedi doc del campo in statistiche.h). La
         * scomposizione alta/bassa priorità resta sul tempo SISTEMA
         * (coerente con l'obiettivo "rispetto delle priorità" della sez.
         * 2.1 del progetto, che riguarda l'attesa vissuta dall'oggetto,
         * non solo il tempo di pipeline). */
        for ( p = 0; p <= PRIORITY_MAX; p++ ) {
            const prioritaStat_t *ps = &s->priorita[p];
            if ( ps->completati == 0 ) {
                continue;
            }
            out->totale_completati += ps->completati;
            somma_tempo_sistema_pesata += ps->somma_tempo_sistema;
            somma_entro_scadenza_sistema += ps->completati_entro_scadenza_sistema;

            totale_con_scomposizione += ps->completati_con_scomposizione;
            somma_tempo_processo_pesata += ps->somma_tempo_processo;
            somma_entro_scadenza_processo += ps->completati_entro_scadenza_processo;

            if ( p >= 7 ) {
                out->completati_alta_priorita += ps->completati;
                entro_scadenza_alta += ps->completati_entro_scadenza_sistema;
            } else if ( p <= 3 ) {
                out->completati_bassa_priorita += ps->completati;
                entro_scadenza_bassa += ps->completati_entro_scadenza_sistema;
            }
        }
        if ( out->completati_alta_priorita > 0 ) {
            out->perc_entro_scadenza_alta_priorita = 100.0 * (double) entro_scadenza_alta / out->completati_alta_priorita;
        }
        if ( out->completati_bassa_priorita > 0 ) {
            out->perc_entro_scadenza_bassa_priorita = 100.0 * (double) entro_scadenza_bassa / out->completati_bassa_priorita;
        }
    }
    if ( out->totale_completati > 0 ) {
        out->tempo_medio_sistema = (double) somma_tempo_sistema_pesata / out->totale_completati;
        out->perc_entro_scadenza_tempo_sistema = 100.0 * (double) somma_entro_scadenza_sistema / out->totale_completati;
    }
    if ( totale_con_scomposizione > 0 ) {
        out->tempo_medio_processo = (double) somma_tempo_processo_pesata / totale_con_scomposizione;
        out->perc_entro_scadenza_tempo_processo = 100.0 * (double) somma_entro_scadenza_processo / totale_con_scomposizione;
    }

    /* Stesse somme di statistiche_stampa (sezione "Occupazione buffer"):
     * media delle medie e massimo dei massimi, sui buffer monitorati. */
    for ( bCur = s->buffer; bCur != NULL; bCur = bCur->next ) {
        double media = ( bCur->campioni > 0 ) ? ( (double) bCur->somma_percentuale / bCur->campioni ) : 0.0;
        somma_medie_buffer += media;
        n_buffer_monitorati++;
        if ( bCur->massimo_percentuale > out->occupazione_massima_buffer ) {
            out->occupazione_massima_buffer = bCur->massimo_percentuale;
        }
        out->totale_blocchi += bCur->blocchi;
    }
    if ( n_buffer_monitorati > 0 ) {
        out->occupazione_media_buffer = somma_medie_buffer / n_buffer_monitorati;
    }

    /* Stessa fonte di statistiche_stampa (sezione "Sensori di qualità"):
     * somma delle anomalie di ogni ISP monitorata. Valori negativi
     * (ERR_*, ISP monitorata ma senza sensore agganciato) non vengono
     * sommati, coerentemente con statistiche_stampa che li segnala e
     * salta invece di stamparli come numero. */
    for ( iCur = s->isp; iCur != NULL; iCur = iCur->next ) {
        long anomalie = controllore_getAnomalieQualita( ctrl, iCur->ID );
        if ( anomalie >= 0 ) {
            out->totale_anomalie_qualita += anomalie;
        }
    }

    return OP_SUCCESS;
}
