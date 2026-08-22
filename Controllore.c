/**
 * @file Controllore.c
 * @brief Implementazione della libreria di controllo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Controllore.h"
#include "registry.h"
#include "Motore.h"
#include "Deviatore.h"
#include "S_Qualita.h"

/* ------------------------------------------------------------------ */
/*  ASSOCIAZIONI INTERNE (attuatori/sensori collegati a un'entità)      */
/* ------------------------------------------------------------------ */

/** @brief Motore + MotorTime collegati a un nastro O a una macchina (via
 *         controllore_collegaMotore). targetType dice quale dei due,
 *         cosi' processNastro/processMachine sanno come determinare se
 *         il motore deve essere acceso (nastro_isEmpty per un nastro,
 *         machine_isBusy per una macchina). */
typedef struct motoreAssoc {
    char targetID[IDLENGTH];
    entity_type_t targetType;   /**< ENTITY_NASTRO o ENTITY_MACHINE. */
    Motore motore;
    MotorTime motoreTime;
    long tempoOnTotale;          /**< Passi cumulativi con motore ON, su tutta la simulazione. */
    long tempoOffTotale;         /**< Passi cumulativi con motore OFF, su tutta la simulazione.
                                   *   NB: MotorTime.time_on/time_off tengono solo la durata del
                                   *   tratto continuo CORRENTE (si azzerano ad ogni cambio di
                                   *   stato) - questi due invece sono un totale che non si azzera
                                   *   mai, incrementati di 1 ad ogni passo in processMachine/
                                   *   processNastro. */
    struct motoreAssoc *next;
} motoreAssoc_t;

/** @brief Deviatore + DeviatoreTime collegati a una ISP (via controllore_collegaDeviatore). */
typedef struct deviatoreAssoc {
    char ispID[IDLENGTH];
    Deviatore deviatore;
    DeviatoreTime deviatoreTime;
    struct deviatoreAssoc *next;
} deviatoreAssoc_t;

/** @brief SensoreBuffer collegato a un buffer (creato automaticamente da controllore_create). */
typedef struct bufferSensorAssoc {
    char bufferID[IDLENGTH];
    SensoreBuffer sensore;
    /* Picco di livello_attuale osservato dall'ultima chiamata a
     * controllore_getPercentualePiccoBuffer (o da quando il sensore e'
     * stato creato, se mai chiamata). Aggiornato ad OGNI vero
     * inserimento/rimozione (vedi le due chiamate ad
     * aggiornamento_status in questo file), non solo al campionamento
     * periodico: un buffer riempito e svuotato all'interno dello STESSO
     * passo di simulazione (es. B2, svuotato verso ISP2 nella stessa
     * fase "Buffer" del passo in cui M lo ha appena riempito) risulta
     * sempre 0% se letto con un singolo poll a fine passo - questo
     * campo cattura invece il picco transitorio anche se non e' mai
     * "visibile" nell'istante del poll. Vedi
     * controllore_getPercentualePiccoBuffer/statistiche_campiona. */
    int picco_transitorio;
    struct bufferSensorAssoc *next;
} bufferSensorAssoc_t;

/** @brief SensorePresenza collegato a un ID di ingresso (creato al primo uso da controllore_segnalaArrivo). */
typedef struct presenceSensorAssoc {
    char ID[IDLENGTH];
    SensorePresenza sensore;
    struct presenceSensorAssoc *next;
} presenceSensorAssoc_t;

/** @brief SensoreQualita + MalfunzionamentoSensore collegati a una ISP
 *         (via controllore_collegaSensoreQualita). A differenza di
 *         Motore/Deviatore/SensoreBuffer, questo NON viene creato
 *         automaticamente da nessuno: se una ISP non ha nessun
 *         qualitaSensorAssoc_t, semplicemente non fa controllo qualità
 *         (isp_tryRelease resta un puro timer). */
typedef struct qualitaSensorAssoc {
    char ispID[IDLENGTH];
    SensoreQualita sensore;
    MalfunzionamentoSensore guasto;
    struct qualitaSensorAssoc *next;
} qualitaSensorAssoc_t;

/**
 * @brief Nodo della coda di oggetti "pronti ma non ancora instradabili".
 *
 * Due motivi per finire in questa coda: la destinazione era piena
 * (deviatoreAssoc == NULL), oppure serve aspettare che un Deviatore
 * raggiunga fisicamente la posizione richiesta (deviatoreAssoc != NULL).
 * In entrambi i casi l'oggetto resta referenziato qui finché non trova
 * posto, riprovato ad ogni passo — senza questa coda verrebbe perso
 * (rimosso dalla sorgente ma mai inserito da nessuna parte).
 */
typedef struct pendingNode {
    object_t *obj;
    entity_type_t destType;
    char destID[IDLENGTH];
    deviatoreAssoc_t *deviatore;   /**< NULL se non c'e' un Deviatore da aspettare. */
    int posizioneRichiesta;        /**< Significativo solo se deviatore != NULL. */
    struct pendingNode *next;
} pendingNode_t;

struct controllore {
    cell_t *cell;                       /**< Non posseduta dal controllore. */
    double soglia_buffer;
    pendingNode_t *pending;
    motoreAssoc_t *motori;
    deviatoreAssoc_t *deviatori;
    bufferSensorAssoc_t *sensoriBuffer;
    presenceSensorAssoc_t *sensoriPresenza;
    qualitaSensorAssoc_t *sensoriQualita;
    long completati;
    log_t *log;                         /**< Facoltativo, vedi controllore_collegaLog. Non posseduto. */
};

/* ------------------------------------------------------------------ */
/*  RICERCA ASSOCIAZIONI                                                */
/* ------------------------------------------------------------------ */

static motoreAssoc_t *findMotoreAssoc( controllore_t *c, const char *targetID )
{
    motoreAssoc_t *cur;
    for ( cur = c->motori; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->targetID, targetID ) == 0 ) { return cur; }
    }
    return NULL;
}

static deviatoreAssoc_t *findDeviatoreAssoc( controllore_t *c, const char *ispID )
{
    deviatoreAssoc_t *cur;
    for ( cur = c->deviatori; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->ispID, ispID ) == 0 ) { return cur; }
    }
    return NULL;
}

static qualitaSensorAssoc_t *findQualitaSensorAssoc( controllore_t *c, const char *ispID )
{
    qualitaSensorAssoc_t *cur;
    for ( cur = c->sensoriQualita; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->ispID, ispID ) == 0 ) { return cur; }
    }
    return NULL;
}

static bufferSensorAssoc_t *findBufferSensorAssoc( controllore_t *c, const char *bufferID )
{
    bufferSensorAssoc_t *cur;
    for ( cur = c->sensoriBuffer; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->bufferID, bufferID ) == 0 ) { return cur; }
    }
    return NULL;
}

static presenceSensorAssoc_t *findPresenceSensorAssoc( controllore_t *c, const char *ID )
{
    presenceSensorAssoc_t *cur;
    for ( cur = c->sensoriPresenza; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->ID, ID ) == 0 ) { return cur; }
    }
    return NULL;
}

/**
 * @brief Aggancia un sensore di presenza a un ID di ingresso.
 *
 * PRIMA di questa versione, il sensore veniva creato "al volo" alla
 * prima chiamata di controllore_segnalaArrivo per un dato ID -
 * incoerente con Motore/Deviatore/SensoreQualita/SensoreBuffer (sempre
 * agganciati esplicitamente PRIMA di essere usati). Ora
 * controllore_segnalaArrivo fallisce con ERR_NOT_FOUND se nessuno ha
 * prima chiamato questa funzione per quell'ID.
 *
 * L'ID non deve necessariamente esistere già nella cella come buffer
 * (per design: "il sensore è indipendente dal buffer vero e proprio"):
 * se il collegamento a un'entità reale fallisce, il sensore resta
 * comunque valido, solo non tracciato su nessuna entità.
 */
short int controllore_collegaSensorePresenza( controllore_t *c, const char *ID )
{
    presenceSensorAssoc_t *cur;
    short int err;
    char sensorID[IDLENGTH];

    if ( c == NULL || ID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( findPresenceSensorAssoc( c, ID ) != NULL ) {
        return ERR_DUPLICATE;
    }

    cur = malloc( sizeof( presenceSensorAssoc_t ) );
    if ( cur == NULL ) { return ERR_ALLOC; }

    strncpy( cur->ID, ID, IDLENGTH - 1 );
    cur->ID[IDLENGTH - 1] = '\0';
    err = (short int) sensore_presenza_init( &cur->sensore, ID );
    if ( err != OP_SUCCESS ) { free( cur ); return err; }

    cur->next = c->sensoriPresenza;
    c->sensoriPresenza = cur;

    /* Tracciabilità (registry): come per gli altri sensori/attuatori. */
    snprintf( sensorID, IDLENGTH, "%.16s_SP", ID );
    if ( registry_add( sensorID, ENTITY_SENSOR_PRESENZA, &cur->sensore ) == OP_SUCCESS ) {
        cell_attachSensor( c->cell, ID, sensorID );
    }

    return OP_SUCCESS;
}

/* ------------------------------------------------------------------ */
/*  DISPATCH GENERICO PER TIPO (stesso schema di cell.c)                */
/* ------------------------------------------------------------------ */

static short int genericInsert( controllore_t *c, entity_type_t type, const char *ID, object_t *obj, int step )
{
    short int result;

    switch ( type ) {
        case ENTITY_BUFFER: {
            buffer_t *b = cell_getBuffer( c->cell, ID );
            bufferSensorAssoc_t *sAssoc;

            if ( b == NULL ) { return ERR_NOT_FOUND; }
            if ( buffer_isFull( b ) ) { return ERR_FULL; }

            result = (short int) buffer_insertObject( b, obj, true );
            if ( result == OP_SUCCESS ) {
                sAssoc = findBufferSensorAssoc( c, ID );
                if ( sAssoc != NULL ) {
                    aggiornamento_status( &sAssoc->sensore, 1 );
                    if ( sAssoc->sensore.livello_attuale > sAssoc->picco_transitorio ) {
                        sAssoc->picco_transitorio = (int) sAssoc->sensore.livello_attuale;
                    }
                }
            }
            /* Un buffer e' una coda di attesa, non una stazione di
             * lavorazione: l'ingresso in un buffer NON conta come
             * "inizio processo" (vedi object_setStepPartial), a
             * differenza dei case sotto. Ritorna subito, senza passare
             * dall'aggiornamento comune a fine funzione. */
            return result;
        }
        case ENTITY_MACHINE: {
            machine_t *m = cell_getMachine( c->cell, ID );
            if ( m == NULL ) { return ERR_NOT_FOUND; }
            if ( machine_isBusy( m ) ) { return ERR_FULL; }
            result = machine_admit( m, obj, step );
            break;
        }
        case ENTITY_ISP: {
            isp_t *i = cell_getISP( c->cell, ID );
            if ( i == NULL ) { return ERR_NOT_FOUND; }
            if ( isp_isBusy( i ) ) { return ERR_FULL; }
            result = isp_admit( i, obj, step );
            break;
        }
        case ENTITY_NASTRO: {
            nastro_t *n = cell_getNastro( c->cell, ID );
            if ( n == NULL ) { return ERR_NOT_FOUND; }
            if ( nastro_isFull( n ) ) { return ERR_FULL; }
            result = (short int) nastro_insertObject( n, obj, step );
            break;
        }
        default:
            return ERR_NOT_SUPPORTED;
    }

    /* Prima ammissione riuscita in una stazione vera (macchina/ISP/
     * nastro, non un buffer): segna l'inizio del processo, UNA SOLA
     * VOLTA per oggetto (object_setStepPartial rifiuta con
     * ERR_DUPLICATE le chiamate successive, es. quando lo stesso
     * oggetto entra in una stazione successiva della pipeline - qui
     * ignoriamo volutamente l'esito, non e' un errore da propagare). */
    if ( result == OP_SUCCESS ) {
        object_setStepPartial( obj, step );
    }

    return result;
}

static int genericOutputCount( controllore_t *c, entity_type_t type, const char *ID )
{
    switch ( type ) {
        case ENTITY_BUFFER:  return buffer_getOutputCount( cell_getBuffer( c->cell, ID ) );
        case ENTITY_MACHINE: return machine_getOutputCount( cell_getMachine( c->cell, ID ) );
        case ENTITY_ISP:     return isp_getOutputCount( cell_getISP( c->cell, ID ) );
        case ENTITY_NASTRO:  return nastro_getOutputCount( cell_getNastro( c->cell, ID ) );
        default:              return 0;
    }
}

static short int genericOutputAt( controllore_t *c, entity_type_t type, const char *ID, int index, char outID[IDLENGTH] )
{
    switch ( type ) {
        case ENTITY_BUFFER:  return buffer_getOutputAt( cell_getBuffer( c->cell, ID ), index, outID );
        case ENTITY_MACHINE: return machine_getOutputAt( cell_getMachine( c->cell, ID ), index, outID );
        case ENTITY_ISP:     return isp_getOutputAt( cell_getISP( c->cell, ID ), index, outID );
        case ENTITY_NASTRO:  return nastro_getOutputAt( cell_getNastro( c->cell, ID ), index, outID );
        default:              return ERR_NOT_SUPPORTED;
    }
}

/* ------------------------------------------------------------------ */
/*  CODA "PENDING"                                                      */
/* ------------------------------------------------------------------ */

static short int pendingAdd( controllore_t *c, object_t *obj, entity_type_t destType, const char *destID,
                              deviatoreAssoc_t *dev, int posizioneRichiesta )
{
    pendingNode_t *node;

    node = malloc( sizeof( pendingNode_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    node->obj = obj;
    node->destType = destType;
    strncpy( node->destID, destID, IDLENGTH - 1 );
    node->destID[IDLENGTH - 1] = '\0';
    node->deviatore = dev;
    node->posizioneRichiesta = posizioneRichiesta;
    node->next = c->pending;
    c->pending = node;

    return OP_SUCCESS;
}

/**
 * @brief Se destID (di tipo destType) non ha nessuna uscita configurata,
 *        l'oggetto appena arrivato lì non si muoverà mai più: lo
 *        contiamo come completato in questo momento.
 *
 * Bug corretto: 'completati' veniva incrementato SOLO nel caso limite
 * di routeObject in cui l'indice di uscita richiesto non esiste (esito
 * di una ISP senza abbastanza uscite collegate) - ma un inserimento
 * RIUSCITO in un buffer terminale (es. uno dei buffer finali del
 * layout, che hanno 0 output per design) non veniva mai contato:
 * 'completati' restava sempre a 0 anche a simulazione conclusa con
 * successo.
 */
static void segnaCompletatoSeTerminale( controllore_t *c, entity_type_t destType,
                                         const char *destID, object_t *obj, int step )
{
    if ( genericOutputCount( c, destType, destID ) == 0 ) {
        int stepPartial;

        object_setStepOut( obj, step );
        c->completati++;

        stepPartial = object_getStepPartial( obj );
        if ( stepPartial != STEP_OUT_NONE ) {
            int attesa = stepPartial - object_getStepCreation( obj );
            int attraversamento = step - stepPartial;
            log_evento( c->log, step, LOG_INFO,
                        "Completamento %s in %s: attesa=%d, attraversamento=%d (totale=%d passi)",
                        object_getID( obj ), destID, attesa, attraversamento, attesa + attraversamento );
        } else {
            /* Non dovrebbe succedere per un oggetto arrivato a un buffer
             * terminale (deve essere passato per almeno una stazione),
             * ma gestito comunque per robustezza: nessuna scomposizione
             * disponibile, solo il tempo totale. */
            log_evento( c->log, step, LOG_INFO,
                        "Completamento %s in %s (tempo totale=%d passi)",
                        object_getID( obj ), destID, step - object_getStepCreation( obj ) );
        }
    }
}

static void retryPending( controllore_t *c, int step )
{
    pendingNode_t *cur;
    pendingNode_t *prev;
    pendingNode_t *next;
    short int result;

    prev = NULL;
    cur = c->pending;
    while ( cur != NULL ) {
        next = cur->next;

        if ( cur->deviatore != NULL ) {
            /* Il Deviatore non aveva ancora raggiunto la posizione: lo
             * ricomandiamo (no-op se il target e' gia' quello giusto,
             * vedi deviatore_imposta_target) e lo facciamo avanzare di
             * un altro passo. */
            deviatore_imposta_target( &cur->deviatore->deviatore, &cur->deviatore->deviatoreTime,
                                       cur->posizioneRichiesta, step );
            posizionamento_deviatore( &cur->deviatore->deviatore );

            if ( !( get_inPosizione( &cur->deviatore->deviatore ) &&
                    get_posizione( &cur->deviatore->deviatore ) == cur->posizioneRichiesta ) ) {
                prev = cur;
                cur = next;
                continue; /* ancora non in posizione: riprova al prossimo passo */
            }
            /* Arrivato in posizione: da qui in poi conta solo se la
             * destinazione ha posto (come un normale pending). */
            cur->deviatore = NULL;
        }

        result = genericInsert( c, cur->destType, cur->destID, cur->obj, step );
        if ( result == OP_SUCCESS ) {
            object_setLocation( cur->obj, cur->destID );
            segnaCompletatoSeTerminale( c, cur->destType, cur->destID, cur->obj, step );
            if ( prev == NULL ) { c->pending = next; } else { prev->next = next; }
            free( cur );
        } else {
            prev = cur;
        }

        cur = next;
    }
}

/**
 * @brief Instrada un oggetto appena rilasciato da fromID verso l'uscita
 *        di indice outIndex. Se dev != NULL, prima comanda il Deviatore
 *        verso outIndex e aspetta che sia in posizione. Se l'uscita non
 *        esiste, l'oggetto esce dalla linea (sink).
 */
static short int routeObject( controllore_t *c, entity_type_t fromType, const char *fromID,
                               object_t *obj, int outIndex, int step, deviatoreAssoc_t *dev )
{
    int outCount;
    char destID[IDLENGTH];
    entity_type_t destType;
    short int result;

    outCount = genericOutputCount( c, fromType, fromID );
    if ( outIndex < 0 || outIndex >= outCount ) {
        object_setStepOut( obj, step );
        c->completati++;
        return OP_SUCCESS;
    }

    if ( genericOutputAt( c, fromType, fromID, outIndex, destID ) != OP_SUCCESS ) {
        return ERR_NOT_FOUND;
    }
    if ( registry_getType( destID, &destType ) != OP_SUCCESS ) {
        return ERR_NOT_FOUND;
    }

    if ( dev != NULL ) {
        /* Vincolo attuatore (sez. 5.2): il Deviatore deve prima
         * raggiungere fisicamente la posizione richiesta. */
        deviatore_imposta_target( &dev->deviatore, &dev->deviatoreTime, outIndex, step );
        posizionamento_deviatore( &dev->deviatore );

        if ( !( get_inPosizione( &dev->deviatore ) && get_posizione( &dev->deviatore ) == outIndex ) ) {
            return pendingAdd( c, obj, destType, destID, dev, outIndex );
        }
    }

    result = genericInsert( c, destType, destID, obj, step );
    if ( result == OP_SUCCESS ) {
        object_setLocation( obj, destID );
        segnaCompletatoSeTerminale( c, destType, destID, obj, step );
        return OP_SUCCESS;
    }
    if ( result == ERR_FULL ) {
        return pendingAdd( c, obj, destType, destID, NULL, 0 ); /* Deviatore gia' in posizione, non serve piu' */
    }

    return result;
}

/* ------------------------------------------------------------------ */
/*  ELABORAZIONE PER TIPO DI ENTITÀ                                     */
/* ------------------------------------------------------------------ */

static void processISP( controllore_t *c, const char *ID, int step )
{
    isp_t *i;
    object_t *obj;
    TipoQualita esito;
    qualitaSensorAssoc_t *qAssoc;
    deviatoreAssoc_t *dev;
    int outIndex;
    int outCount;

    i = cell_getISP( c->cell, ID );
    if ( i == NULL || !isp_isReady( i, step ) ) {
        return;
    }

    /* isp_tryRelease non calcola piu' nessun esito: la ISP di per se'
     * e' un puro timer. L'esito (se serve) si calcola qui, usando il
     * sensore agganciato con controllore_collegaSensoreQualita. */
    obj = isp_tryRelease( i, step );
    if ( obj == NULL ) {
        return;
    }

    qAssoc = findQualitaSensorAssoc( c, ID );
    if ( qAssoc != NULL ) {
        int esitoGrezzo = get_qualita( &qAssoc->sensore, &qAssoc->guasto, step, obj, true );
        /* get_qualita puo' restituire un codice ERR_* (negativo) solo in
         * casi anomali di configurazione (es. target non impostato per
         * questo materiale): per non bloccare la ISP l'oggetto viene
         * comunque rilasciato, trattato convenzionalmente come SCARTO. */
        esito = ( esitoGrezzo >= 0 ) ? (TipoQualita) esitoGrezzo : SCARTO;
    } else {
        /* Nessun sensore agganciato a questa ISP (es. una ISP
         * "passacarte" con una sola uscita, che non deve giudicare la
         * qualita'): l'esito non verra' comunque usato per instradare
         * se ha una sola uscita (vedi outCount==1 sotto), quindi un
         * valore neutro qui va bene. Se invece l'ISP ha piu' uscite ma
         * nessun sensore agganciato, e' una configurazione incompleta:
         * trattiamo tutto come SCARTO, scelta conservativa. */
        esito = SCARTO;
    }

    dev = findDeviatoreAssoc( c, ID );

    /* L'indice di uscita corrisponde di norma all'esito (convenzione in
     * cell.h: CONFORME=0, RIVALUTAZIONE=1, SCARTO=2). Con 4 uscite
     * collegate (layout con doppio esito "conforme", uno per materiale:
     * es. Alacciaio/Blrame), l'esito CONFORME da solo non basta a
     * scegliere tra le due: usiamo object->type per decidere tra
     * l'indice 0 (materiale 'A') e l'ultimo indice, 3 (materiale 'B'/
     * qualunque altro), lasciando RIVALUTAZIONE=1 e SCARTO=2 invariati. */
    outCount = genericOutputCount( c, ENTITY_ISP, ID );
    outIndex = (int) esito;

    /* ISP "passacarte" con una sola uscita (es. il primo ISP di un
     * layout che serve solo a taggare il materiale, non a giudicare la
     * qualita'): l'esito calcolato da get_qualita non ha alcun indice
     * valido a cui corrispondere se non lo 0, quindi va sempre e
     * comunque instradato li', a prescindere da quale sia. Senza questo
     * caso, un esito RIVALUTAZIONE/SCARTO su una ISP a singola uscita
     * farebbe uscire l'oggetto dalla linea per errore (trattato come
     * "nessuna uscita a quell'indice"). */
    if ( outCount == 1 ) {
        outIndex = 0;
    }

    /* get_Material (calcolo su densita'/geometria, non il campo
     * object->type impostato a mano alla creazione) va chiamata ogni
     * volta che questa ISP ha un sensore di qualita' agganciato, non
     * solo quando serve per instradare (outCount>=4): e' l'unico modo
     * per popolare i contatori A/B del sensore (letti da
     * controllore_getMaterialeA/B, usati dalle statistiche - vedi
     * statistiche_stampa) anche per un'ISP "passacarte" a singola
     * uscita come ISP1, che classifica comunque il materiale ma non ha
     * bisogno di scegliere tra due uscite diverse per instradarlo. */
    if ( qAssoc != NULL ) {
        char materiale = get_Material( obj, &qAssoc->sensore );

        if ( esito == CONFORME && outCount >= 4 ) {
            /* Con 4 uscite, l'esito CONFORME da solo non basta a
             * scegliere tra le due uscite "pezzo conforme" (una per
             * materiale): si usa il materiale appena calcolato per
             * decidere l'indice 0 (materiale 'A') o l'ultimo indice, 3
             * (materiale 'B'). */
            if ( materiale != 'A' && materiale != 'B' ) {
                /* get_Material non ha riconosciuto il materiale entro
                 * tolleranza per nessuna delle due densita': usiamo
                 * object->type come ripiego, per non perdere l'oggetto. */
                materiale = object_getType( obj );
            }
            outIndex = ( materiale == 'B' ) ? 3 : 0;
        }
    }

    routeObject( c, ENTITY_ISP, ID, obj, outIndex, step, dev );
}

static void processMachine( controllore_t *c, const char *ID, int step )
{
    machine_t *m;
    object_t *obj;
    motoreAssoc_t *mot;

    m = cell_getMachine( c->cell, ID );
    if ( m == NULL ) {
        return;
    }

    mot = findMotoreAssoc( c, ID );
    if ( mot != NULL ) {
        /* A differenza del nastro (dove il motore spento BLOCCA il
         * trasporto), qui il motore riflette soltanto lo stato della
         * macchina: acceso finche' sta lavorando un pezzo, spento
         * quando e' libera. Non condiziona machine_isReady/tryRelease:
         * e' tracciato per le sue statistiche (rampa, temperatura),
         * non governa i tempi di lavorazione (quelli restano decisi da
         * machine_t stessa, come per il modello aggregato del nastro). */
        MotorState comando = machine_isBusy( m ) ? MOTORE_ON : MOTORE_OFF;
        motore_update( &mot->motore, &mot->motoreTime, comando, step );
        if ( motore_get_status( &mot->motore ) ) { mot->tempoOnTotale++; } else { mot->tempoOffTotale++; }
    }

    if ( !machine_isReady( m, step ) ) {
        return;
    }

    obj = machine_tryRelease( m, step );
    if ( obj == NULL ) {
        return;
    }

    routeObject( c, ENTITY_MACHINE, ID, obj, 0, step, NULL );
}

static void processNastro( controllore_t *c, const char *ID, int step )
{
    nastro_t *n;
    object_t *obj;
    motoreAssoc_t *mot;

    n = cell_getNastro( c->cell, ID );
    if ( n == NULL ) {
        return;
    }

    mot = findMotoreAssoc( c, ID );
    if ( mot != NULL ) {
        /* Il motore va acceso solo quando c'e' qualcosa da trasportare
         * (comando semplice, non e' richiesta una politica piu'
         * sofisticata dal progetto). Il tempo di attraversamento resta
         * comunque governato dal modello aggregato del nastro (sez. 2.2):
         * il motore e' tracciato per le sue statistiche (rampa,
         * temperatura), non ricalcola la fisica del trasporto. */
        MotorState comando = nastro_isEmpty( n ) ? MOTORE_OFF : MOTORE_ON;
        motore_update( &mot->motore, &mot->motoreTime, comando, step );
        if ( motore_get_status( &mot->motore ) ) { mot->tempoOnTotale++; } else { mot->tempoOffTotale++; }
        if ( !motore_get_status( &mot->motore ) ) {
            return; /* motore spento: nessun trasporto questo passo */
        }
    }

    if ( !nastro_isReady( n, step ) ) {
        return;
    }

    obj = nastro_removeReadyObject( n, step );
    if ( obj == NULL ) {
        return;
    }

    routeObject( c, ENTITY_NASTRO, ID, obj, 0, step, NULL );
}

static void processBuffer( controllore_t *c, const char *ID, int step )
{
    buffer_t *b;
    char destID[IDLENGTH];
    entity_type_t destType;
    object_t *obj;
    short int result;
    bufferSensorAssoc_t *sAssoc;

    b = cell_getBuffer( c->cell, ID );
    if ( b == NULL || buffer_isEmpty( b ) ) {
        return;
    }
    if ( buffer_getOutputCount( b ) == 0 ) {
        return; /* buffer terminale: nessuno smaltimento automatico */
    }
    if ( buffer_getOutputAt( b, 0, destID ) != OP_SUCCESS ) {
        return;
    }
    if ( registry_getType( destID, &destType ) != OP_SUCCESS ) {
        return;
    }

    if ( destType == ENTITY_MACHINE ) {
        machine_t *m = cell_getMachine( c->cell, destID );
        char mOutID[IDLENGTH];
        entity_type_t mOutType;

        if ( m == NULL || machine_isBusy( m ) ) {
            return; /* macchina occupata: niente da fare questo passo */
        }

        /* Ammissione "buffer-aware" (Strategia 1, sez. 4.1): se il buffer
         * a valle della macchina e' oltre soglia, ritarda l'ammissione.
         * La lettura viene dal SensoreBuffer associato, non da
         * buffer_getCount/getCapacity direttamente. */
        if ( machine_getOutputCount( m ) > 0 &&
             machine_getOutputAt( m, 0, mOutID ) == OP_SUCCESS &&
             registry_getType( mOutID, &mOutType ) == OP_SUCCESS &&
             mOutType == ENTITY_BUFFER ) {
            bufferSensorAssoc_t *downstreamSensor = findBufferSensorAssoc( c, mOutID );
            if ( downstreamSensor != NULL ) {
                int perc = get_percentuale_livello( &downstreamSensor->sensore );
                if ( perc >= (int) ( c->soglia_buffer * 100.0 ) ) {
                    return; /* buffer a valle troppo pieno: ritarda */
                }
            }
        }
    }

    /* Un solo oggetto per volta puo' essere prelevato da una risorsa
     * (sez. 2.2 del progetto): preleva quello a priorita' piu' alta. */
    obj = buffer_removeObject( b, true );
    if ( obj == NULL ) {
        return;
    }

    sAssoc = findBufferSensorAssoc( c, ID );
    if ( sAssoc != NULL ) { aggiornamento_status( &sAssoc->sensore, -1 ); }

    result = genericInsert( c, destType, destID, obj, step );
    if ( result == OP_SUCCESS ) {
        object_setLocation( obj, destID );
        segnaCompletatoSeTerminale( c, destType, destID, obj, step );
    } else {
        /* Non dovrebbe succedere (isBusy era gia' stato controllato per
         * il caso macchina), ma per sicurezza mettiamo in coda invece di
         * perdere l'oggetto appena rimosso dal buffer. */
        pendingAdd( c, obj, destType, destID, NULL, 0 );
    }
}

/* ------------------------------------------------------------------ */
/*  API PUBBLICA                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Aggancia un sensore buffer a un buffer già presente nella cella.
 *
 * PRIMA di questa versione, un SensoreBuffer veniva creato in automatico
 * da controllore_create per OGNI buffer già presente nella cella in quel
 * momento, senza possibilità di scegliere - incoerente con
 * Motore/Deviatore/SensoreQualita (sempre agganciati esplicitamente).
 * Ora un buffer senza questa chiamata semplicemente non ha nessun
 * sensore: controllore_getPercentualeBuffer/getStatoBuffer restituiscono
 * ERR_NOT_FOUND, e l'ammissione "buffer-aware" (Strategia 1) non ha
 * nulla da leggere per quel buffer specifico (non blocca comunque
 * l'ammissione, la salta soltanto).
 */
short int controllore_collegaSensoreBuffer( controllore_t *c, const char *bufferID )
{
    buffer_t *b;
    bufferSensorAssoc_t *node;
    short int err;
    char sensorID[IDLENGTH];

    if ( c == NULL || bufferID == NULL ) {
        return ERR_NULL_PTR;
    }
    b = cell_getBuffer( c->cell, bufferID );
    if ( b == NULL ) {
        return ERR_NOT_FOUND;
    }
    if ( findBufferSensorAssoc( c, bufferID ) != NULL ) {
        return ERR_DUPLICATE;
    }

    node = malloc( sizeof( bufferSensorAssoc_t ) );
    if ( node == NULL ) { return ERR_ALLOC; }

    strncpy( node->bufferID, bufferID, IDLENGTH - 1 );
    node->bufferID[IDLENGTH - 1] = '\0';

    err = (short int) sensore_Buffer_init( &node->sensore, bufferID, buffer_getCapacity( b ) );
    if ( err != OP_SUCCESS ) { free( node ); return err; }
    node->picco_transitorio = 0;

    node->next = c->sensoriBuffer;
    c->sensoriBuffer = node;

    /* Tracciabilità (registry): come per gli altri sensori/attuatori. */
    snprintf( sensorID, IDLENGTH, "%.16s_SB", bufferID );
    if ( registry_add( sensorID, ENTITY_SENSOR_BUFFER, &node->sensore ) == OP_SUCCESS ) {
        cell_attachSensor( c->cell, bufferID, sensorID );
    }

    return OP_SUCCESS;
}

controllore_t *controllore_create( cell_t *cell, double soglia_buffer, short int *errCode )
{
    controllore_t *c;

    if ( cell == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_NULL_PTR; }
        return NULL;
    }
    if ( soglia_buffer <= 0.0 || soglia_buffer > 1.0 ) {
        if ( errCode != NULL ) { *errCode = ERR_OUT_OF_RANGE; }
        return NULL;
    }

    c = malloc( sizeof( controllore_t ) );
    if ( c == NULL ) {
        if ( errCode != NULL ) { *errCode = ERR_ALLOC; }
        return NULL;
    }

    c->cell = cell;
    c->soglia_buffer = soglia_buffer;
    c->pending = NULL;
    c->motori = NULL;
    c->deviatori = NULL;
    c->sensoriBuffer = NULL;
    c->sensoriPresenza = NULL;
    c->sensoriQualita = NULL;
    c->completati = 0;
    c->log = NULL;

    /* Nessun sensore creato automaticamente qui: a differenza della
     * versione precedente (che creava un SensoreBuffer per OGNI buffer
     * già presente nella cella), ora ogni sensore va agganciato
     * esplicitamente con controllore_collegaSensoreBuffer, coerente con
     * Motore/Deviatore/SensoreQualita. */

    if ( errCode != NULL ) { *errCode = OP_SUCCESS; }
    return c;
}

void controllore_destroy( controllore_t *c )
{
    pendingNode_t *pCur, *pNext;
    motoreAssoc_t *mCur, *mNext;
    deviatoreAssoc_t *dCur, *dNext;
    bufferSensorAssoc_t *bCur, *bNext;
    presenceSensorAssoc_t *sCur, *sNext;
    qualitaSensorAssoc_t *qCur, *qNext;

    if ( c == NULL ) {
        return;
    }

    /* Libera i nodi delle liste interne. La coda pending e' un caso a
     * parte rispetto a buffer/nastro/macchina/ISP: quelli sono
     * strutture PUBBLICHE (esposte via cell.h/buffer.h/ecc.), quindi il
     * chiamante (main) puo' iterarle da fuori e liberare i propri
     * object_t esplicitamente. pendingNode_t invece e' privato di
     * questo file (mai esposto in Controllore.h): nessun codice esterno
     * potrebbe mai raggiungere quegli object_t per liberarli, quindi -
     * a differenza degli altri moduli, che lasciano sempre il payload a
     * carico del chiamante - qui la liberazione la fa direttamente
     * questa funzione, come UNICO punto che ha davvero accesso alla
     * lista. */
    for ( pCur = c->pending; pCur != NULL; pCur = pNext ) {
        pNext = pCur->next;
        object_delete( pCur->obj );
        free( pCur );
    }
    for ( mCur = c->motori; mCur != NULL; mCur = mNext ) { mNext = mCur->next; free( mCur ); }
    for ( dCur = c->deviatori; dCur != NULL; dCur = dNext ) { dNext = dCur->next; free( dCur ); }
    for ( bCur = c->sensoriBuffer; bCur != NULL; bCur = bNext ) { bNext = bCur->next; free( bCur ); }
    for ( sCur = c->sensoriPresenza; sCur != NULL; sCur = sNext ) { sNext = sCur->next; free( sCur ); }
    for ( qCur = c->sensoriQualita; qCur != NULL; qCur = qNext ) { qNext = qCur->next; free( qCur ); }

    free( c );
}

short int controllore_collegaLog( controllore_t *c, log_t *log )
{
    if ( c == NULL ) {
        return ERR_NULL_PTR;
    }
    c->log = log;
    return OP_SUCCESS;
}

short int controllore_collegaMotore( controllore_t *c, const char *targetID, int velocita_target, int accelerazione_target )
{
    motoreAssoc_t *node;
    short int err;
    char attuatoreID[IDLENGTH];
    entity_type_t targetType;

    if ( c == NULL || targetID == NULL ) {
        return ERR_NULL_PTR;
    }
    /* Un Motore puo' essere collegato a un nastro (aziona il trasporto)
     * O a una macchina (motore della lavorazione, acceso mentre M e'
     * occupata - vedi processMachine): qualunque altro tipo non e'
     * supportato. */
    if ( registry_getType( targetID, &targetType ) != OP_SUCCESS ) {
        return ERR_NOT_FOUND;
    }
    if ( targetType != ENTITY_NASTRO && targetType != ENTITY_MACHINE ) {
        return ERR_NOT_SUPPORTED;
    }
    if ( findMotoreAssoc( c, targetID ) != NULL ) {
        return ERR_DUPLICATE;
    }

    node = malloc( sizeof( motoreAssoc_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    strncpy( node->targetID, targetID, IDLENGTH - 1 );
    node->targetID[IDLENGTH - 1] = '\0';
    node->targetType = targetType;

    /* Bug: la versione precedente chiamava motore_init con soli 3
     * argomenti, ma la firma richiede anche accelerazione_desiderata
     * (aggiunta di recente a Motore.h) - non compilava. */
    err = (short int) motore_init( &node->motore, targetID, velocita_target, accelerazione_target );
    if ( err != OP_SUCCESS ) {
        free( node );
        return err;
    }
    node->motoreTime.time_on = 0;
    node->motoreTime.time_off = 0;
    node->motoreTime.time_start = 0;
    node->motoreTime.time_stop = 0;
    node->tempoOnTotale = 0;
    node->tempoOffTotale = 0;

    node->next = c->motori;
    c->motori = node;

    /* Tracciabilità (registry): il Motore diventa un'entità a sé
     * (ENTITY_ACTUATOR_MOTORE) collegata al nastro/alla macchina tramite
     * cell_attachActuator, così actuatorList lo mostra (nastro_print /
     * machine_print) invece di restare sempre "(nessuno)". */
    snprintf( attuatoreID, IDLENGTH, "%.16s_MO", targetID );
    if ( registry_add( attuatoreID, ENTITY_ACTUATOR_MOTORE, &node->motore ) == OP_SUCCESS ) {
        cell_attachActuator( c->cell, targetID, attuatoreID );
    }

    return OP_SUCCESS;
}

short int controllore_collegaDeviatore( controllore_t *c, const char *ispID, int tempo_minimo_commutazioni )
{
    deviatoreAssoc_t *node;
    short int err;
    char attuatoreID[IDLENGTH];

    if ( c == NULL || ispID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( !cell_hasISP( c->cell, ispID ) ) {
        return ERR_NOT_FOUND;
    }
    if ( findDeviatoreAssoc( c, ispID ) != NULL ) {
        return ERR_DUPLICATE;
    }

    node = malloc( sizeof( deviatoreAssoc_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    strncpy( node->ispID, ispID, IDLENGTH - 1 );
    node->ispID[IDLENGTH - 1] = '\0';

    err = (short int) deviatore_init( &node->deviatore, ispID );
    if ( err != OP_SUCCESS ) {
        free( node );
        return err;
    }
    err = (short int) deviatoretime_init( &node->deviatoreTime, tempo_minimo_commutazioni );
    if ( err != OP_SUCCESS ) {
        free( node );
        return err;
    }
    /* Il Deviatore deve essere acceso per potersi muovere. */
    set_deviatore( &node->deviatore, DEVIATORE_ON );

    node->next = c->deviatori;
    c->deviatori = node;

    /* Tracciabilità (registry): come per il Motore, ma su ENTITY_ISP. */
    snprintf( attuatoreID, IDLENGTH, "%.16s_DE", ispID );
    if ( registry_add( attuatoreID, ENTITY_ACTUATOR_DEVIATORE, &node->deviatore ) == OP_SUCCESS ) {
        cell_attachActuator( c->cell, ispID, attuatoreID );
    }

    return OP_SUCCESS;
}

short int controllore_collegaSensoreQualita( controllore_t *c, const char *ispID,
                                              int dimensionX_target, int raggio_target )
{
    qualitaSensorAssoc_t *node;
    short int err;
    char sensorID[IDLENGTH];

    if ( c == NULL || ispID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( !cell_hasISP( c->cell, ispID ) ) {
        return ERR_NOT_FOUND;
    }
    if ( findQualitaSensorAssoc( c, ispID ) != NULL ) {
        return ERR_DUPLICATE;
    }

    node = malloc( sizeof( qualitaSensorAssoc_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    strncpy( node->ispID, ispID, IDLENGTH - 1 );
    node->ispID[IDLENGTH - 1] = '\0';

    /* Guasto disabilitato di default: si attiva esplicitamente con
     * controllore_impostaGuastoQualita (sez. 5.3 del progetto). */
    err = (short int) sensore_qualita_init( &node->sensore, ispID, &node->guasto, false,
                                             dimensionX_target, raggio_target );
    if ( err != OP_SUCCESS ) {
        free( node );
        return err;
    }

    node->next = c->sensoriQualita;
    c->sensoriQualita = node;

    /* Tracciabilità (registry): come per il SensoreBuffer, ma su ENTITY_ISP. */
    snprintf( sensorID, IDLENGTH, "%.16s_SQ", ispID );
    if ( registry_add( sensorID, ENTITY_SENSOR_QUALITA, &node->sensore ) == OP_SUCCESS ) {
        cell_attachSensor( c->cell, ispID, sensorID );
    }

    return OP_SUCCESS;
}

short int controllore_impostaGuastoQualita( controllore_t *c, const char *ispID,
                                             bool abilitato, int time_error, int time_ok )
{
    qualitaSensorAssoc_t *node;
    short int result;

    if ( c == NULL || ispID == NULL ) {
        return ERR_NULL_PTR;
    }

    node = findQualitaSensorAssoc( c, ispID );
    if ( node == NULL ) {
        return ERR_NOT_FOUND; /* nessun sensore agganciato a questa ISP */
    }

    result = sensore_qualita_imposta_guasto( &node->guasto, time_error, time_ok );
    if ( result != OP_SUCCESS ) {
        return result;
    }

    node->guasto.malfunzionamento_abilitato = abilitato;

    return OP_SUCCESS;
}

short int controllore_step( controllore_t *c, int step_corrente )
{
    int n;
    int idx;
    char ID[IDLENGTH];

    if ( c == NULL ) {
        return ERR_NULL_PTR;
    }

    /* 1. Sblocca prima gli oggetti rimasti in attesa dal passo precedente. */
    retryPending( c, step_corrente );

    /* 2. ISP pronte: calcola l'esito e instrada (eventualmente via Deviatore). */
    n = cell_getISPCount( c->cell );
    for ( idx = 0; idx < n; idx++ ) {
        if ( cell_getISPIDAt( c->cell, idx, ID ) == OP_SUCCESS ) {
            processISP( c, ID, step_corrente );
        }
    }

    /* 3. Macchine pronte: rilascia verso il loro output. */
    n = cell_getMachineCount( c->cell );
    for ( idx = 0; idx < n; idx++ ) {
        if ( cell_getMachineIDAt( c->cell, idx, ID ) == OP_SUCCESS ) {
            processMachine( c, ID, step_corrente );
        }
    }

    /* 4. Nastri pronti: comanda il motore (se collegato) e rilascia. */
    n = cell_getNastroCount( c->cell );
    for ( idx = 0; idx < n; idx++ ) {
        if ( cell_getNastroIDAt( c->cell, idx, ID ) == OP_SUCCESS ) {
            processNastro( c, ID, step_corrente );
        }
    }

    /* 5. Buffer: ammissione a valle, priorita' + buffer-aware. Fatta per
     *    ultima cosi' da usare lo stato di M/ISP/nastro gia' aggiornato
     *    (liberati) ai passi precedenti in questo stesso step. */
    n = cell_getBufferCount( c->cell );
    for ( idx = 0; idx < n; idx++ ) {
        if ( cell_getBufferIDAt( c->cell, idx, ID ) == OP_SUCCESS ) {
            processBuffer( c, ID, step_corrente );
        }
    }

    return OP_SUCCESS;
}

int controllore_getPercentualeBuffer( const controllore_t *c, const char *bufferID )
{
    bufferSensorAssoc_t *sAssoc;

    if ( c == NULL || bufferID == NULL ) {
        return ERR_NULL_PTR;
    }
    sAssoc = findBufferSensorAssoc( (controllore_t *) c, bufferID );
    if ( sAssoc == NULL ) {
        return ERR_NOT_FOUND;
    }
    return get_percentuale_livello( &sAssoc->sensore );
}

int controllore_getPercentualePiccoBuffer( controllore_t *c, const char *bufferID )
{
    bufferSensorAssoc_t *sAssoc;
    int perc;
    long capacita;

    if ( c == NULL || bufferID == NULL ) {
        return ERR_NULL_PTR;
    }
    sAssoc = findBufferSensorAssoc( c, bufferID );
    if ( sAssoc == NULL ) {
        return ERR_NOT_FOUND;
    }

    capacita = sAssoc->sensore.livello_massimo;
    perc = ( capacita <= 0 ) ? 0 : (int) ( ( (long) sAssoc->picco_transitorio * 100 ) / capacita );

    /* Reset della finestra di osservazione: il prossimo picco riparte dal
     * livello ATTUALE (non da 0), cosi' un buffer che resta pieno tra due
     * campionamenti continua a risultare "pieno" anche nella finestra
     * successiva, invece di sembrare svuotato fino al prossimo vero
     * incremento. */
    sAssoc->picco_transitorio = (int) sAssoc->sensore.livello_attuale;

    return perc;
}

int controllore_getStatoBuffer( const controllore_t *c, const char *bufferID )
{
    bufferSensorAssoc_t *sAssoc;

    if ( c == NULL || bufferID == NULL ) {
        return ERR_NULL_PTR;
    }
    sAssoc = findBufferSensorAssoc( (controllore_t *) c, bufferID );
    if ( sAssoc == NULL ) {
        return ERR_NOT_FOUND;
    }
    return get_status_buffer( &sAssoc->sensore );
}

/**
 * @brief Tempo cumulativo (in passi di simulazione) in cui il motore
 *        collegato a targetID (un nastro o una macchina) è stato ACCESO,
 *        su tutta la simulazione da quando è stato agganciato.
 * @param c Puntatore al controllore.
 * @param targetID ID del nastro o della macchina.
 * @return Il tempo cumulativo, oppure ERR_NULL_PTR/ERR_NOT_FOUND (vedi
 *         errors.h) se non trovato.
 */
long controllore_getTempoMotoreOn( const controllore_t *c, const char *targetID )
{
    motoreAssoc_t *mot;

    if ( c == NULL || targetID == NULL ) {
        return ERR_NULL_PTR;
    }
    mot = findMotoreAssoc( (controllore_t *) c, targetID );
    if ( mot == NULL ) {
        return ERR_NOT_FOUND;
    }
    return mot->tempoOnTotale;
}

/**
 * @brief Come controllore_getTempoMotoreOn, ma per il tempo cumulativo
 *        in cui il motore è stato SPENTO.
 */
long controllore_getTempoMotoreOff( const controllore_t *c, const char *targetID )
{
    motoreAssoc_t *mot;

    if ( c == NULL || targetID == NULL ) {
        return ERR_NULL_PTR;
    }
    mot = findMotoreAssoc( (controllore_t *) c, targetID );
    if ( mot == NULL ) {
        return ERR_NOT_FOUND;
    }
    return mot->tempoOffTotale;
}

long controllore_getLettureQualita( const controllore_t *c, const char *ispID )
{
    qualitaSensorAssoc_t *qAssoc;

    if ( c == NULL || ispID == NULL ) {
        return ERR_NULL_PTR;
    }
    qAssoc = findQualitaSensorAssoc( (controllore_t *) c, ispID );
    if ( qAssoc == NULL ) {
        return ERR_NOT_FOUND;
    }
    return get_letture_totali_qualita( &qAssoc->sensore );
}

long controllore_getAnomalieQualita( const controllore_t *c, const char *ispID )
{
    qualitaSensorAssoc_t *qAssoc;

    if ( c == NULL || ispID == NULL ) {
        return ERR_NULL_PTR;
    }
    qAssoc = findQualitaSensorAssoc( (controllore_t *) c, ispID );
    if ( qAssoc == NULL ) {
        return ERR_NOT_FOUND;
    }
    return get_anomalie_rilevate( &qAssoc->sensore );
}

short int controllore_getTipoLettureQualita( const controllore_t *c, const char *ispID, long out[3] )
{
    qualitaSensorAssoc_t *qAssoc;

    if ( c == NULL || ispID == NULL || out == NULL ) {
        return ERR_NULL_PTR;
    }
    qAssoc = findQualitaSensorAssoc( (controllore_t *) c, ispID );
    if ( qAssoc == NULL ) {
        return ERR_NOT_FOUND;
    }
    get_type_letture_totali( &qAssoc->sensore, out );
    return OP_SUCCESS;
}

long controllore_getMaterialeA( const controllore_t *c, const char *ispID )
{
    qualitaSensorAssoc_t *qAssoc;

    if ( c == NULL || ispID == NULL ) {
        return ERR_NULL_PTR;
    }
    qAssoc = findQualitaSensorAssoc( (controllore_t *) c, ispID );
    if ( qAssoc == NULL ) {
        return ERR_NOT_FOUND;
    }
    return get_ConteggioMaterialeA( &qAssoc->sensore );
}

long controllore_getMaterialeB( const controllore_t *c, const char *ispID )
{
    qualitaSensorAssoc_t *qAssoc;

    if ( c == NULL || ispID == NULL ) {
        return ERR_NULL_PTR;
    }
    qAssoc = findQualitaSensorAssoc( (controllore_t *) c, ispID );
    if ( qAssoc == NULL ) {
        return ERR_NOT_FOUND;
    }
    return get_ConteggioMaterialeB( &qAssoc->sensore );
}

long controllore_getLetturePresenza( const controllore_t *c, const char *ID )
{
    presenceSensorAssoc_t *pAssoc;

    if ( c == NULL || ID == NULL ) {
        return ERR_NULL_PTR;
    }
    pAssoc = findPresenceSensorAssoc( (controllore_t *) c, ID );
    if ( pAssoc == NULL ) {
        return ERR_NOT_FOUND;
    }
    return get_letture_totali_presenza( &pAssoc->sensore );
}

long controllore_getRilevamentiPresenza( const controllore_t *c, const char *ID )
{
    presenceSensorAssoc_t *pAssoc;

    if ( c == NULL || ID == NULL ) {
        return ERR_NULL_PTR;
    }
    pAssoc = findPresenceSensorAssoc( (controllore_t *) c, ID );
    if ( pAssoc == NULL ) {
        return ERR_NOT_FOUND;
    }
    return get_rilevamenti_totali_presenza( &pAssoc->sensore );
}

int controllore_segnalaArrivo( controllore_t *c, const char *bufferIngressoID, int time_on, int presenza )
{
    presenceSensorAssoc_t *sAssoc;

    if ( c == NULL || bufferIngressoID == NULL ) {
        return ERR_NULL_PTR;
    }

    sAssoc = findPresenceSensorAssoc( c, bufferIngressoID );
    if ( sAssoc == NULL ) {
        return ERR_NOT_FOUND; /* nessun sensore agganciato con controllore_collegaSensorePresenza */
    }

    return get_status_presenza( &sAssoc->sensore, time_on, presenza );
}

short int controllore_ammettiArrivo( controllore_t *c, const char *bufferID, object_t *obj, int step )
{
    if ( c == NULL || bufferID == NULL || obj == NULL ) {
        return ERR_NULL_PTR;
    }
    /* Stesso percorso (genericInsert) usato per ogni movimento interno
     * alla cella: inserisce nel buffer E aggiorna il SensoreBuffer
     * eventualmente agganciato, in un'unica operazione atomica - vedi
     * doc in Controllore.h sul perche' questo va preferito a
     * buffer_insertObject diretto per gli arrivi dall'esterno. */
    return genericInsert( c, ENTITY_BUFFER, bufferID, obj, step );
}

long controllore_getCompletati( const controllore_t *c )
{
    if ( c == NULL ) {
        return ERR_NULL_PTR;
    }
    return c->completati;
}

int controllore_getPendingCount( const controllore_t *c )
{
    pendingNode_t *cur;
    int count;

    if ( c == NULL ) {
        return ERR_NULL_PTR;
    }

    count = 0;
    for ( cur = c->pending; cur != NULL; cur = cur->next ) {
        count++;
    }

    return count;
}

void controllore_print( const controllore_t *c )
{
    if ( c == NULL ) {
        printf( "controllore_print: controllore NULL\n" );
        return;
    }

    cell_print( c->cell );
    printf( "Controllore[completati=%ld, pending=%d]\n",
            c->completati, controllore_getPendingCount( c ) );
}
