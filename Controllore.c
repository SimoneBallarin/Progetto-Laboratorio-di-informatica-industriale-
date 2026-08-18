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

/* ------------------------------------------------------------------ */
/*  ASSOCIAZIONI INTERNE (attuatori/sensori collegati a un'entità)      */
/* ------------------------------------------------------------------ */

/** @brief Motore + MotorTime collegati a un nastro (via controllore_collegaMotore). */
typedef struct motoreAssoc {
    char nastroID[IDLENGTH];
    Motore motore;
    MotorTime motoreTime;
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
    struct bufferSensorAssoc *next;
} bufferSensorAssoc_t;

/** @brief SensorePresenza collegato a un ID di ingresso (creato al primo uso da controllore_segnalaArrivo). */
typedef struct presenceSensorAssoc {
    char ID[IDLENGTH];
    SensorePresenza sensore;
    struct presenceSensorAssoc *next;
} presenceSensorAssoc_t;

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
    long completati;
};

/* ------------------------------------------------------------------ */
/*  RICERCA ASSOCIAZIONI                                                */
/* ------------------------------------------------------------------ */

static motoreAssoc_t *findMotoreAssoc( controllore_t *c, const char *nastroID )
{
    motoreAssoc_t *cur;
    for ( cur = c->motori; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->nastroID, nastroID ) == 0 ) { return cur; }
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

static bufferSensorAssoc_t *findBufferSensorAssoc( controllore_t *c, const char *bufferID )
{
    bufferSensorAssoc_t *cur;
    for ( cur = c->sensoriBuffer; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->bufferID, bufferID ) == 0 ) { return cur; }
    }
    return NULL;
}

static presenceSensorAssoc_t *findOrCreatePresenceSensorAssoc( controllore_t *c, const char *ID )
{
    presenceSensorAssoc_t *cur;
    short int err;

    for ( cur = c->sensoriPresenza; cur != NULL; cur = cur->next ) {
        if ( strcmp( cur->ID, ID ) == 0 ) { return cur; }
    }

    cur = malloc( sizeof( presenceSensorAssoc_t ) );
    if ( cur == NULL ) { return NULL; }

    strncpy( cur->ID, ID, IDLENGTH - 1 );
    cur->ID[IDLENGTH - 1] = '\0';
    err = (short int) sensore_presenza_init( &cur->sensore, ID );
    if ( err != OP_SUCCESS ) { free( cur ); return NULL; }

    cur->next = c->sensoriPresenza;
    c->sensoriPresenza = cur;
    return cur;
}

/* ------------------------------------------------------------------ */
/*  DISPATCH GENERICO PER TIPO (stesso schema di cell.c)                */
/* ------------------------------------------------------------------ */

static short int genericInsert( controllore_t *c, entity_type_t type, const char *ID, object_t *obj, int step )
{
    switch ( type ) {
        case ENTITY_BUFFER: {
            buffer_t *b = cell_getBuffer( c->cell, ID );
            bufferSensorAssoc_t *sAssoc;
            short int result;

            if ( b == NULL ) { return ERR_NOT_FOUND; }
            if ( buffer_isFull( b ) ) { return ERR_FULL; }

            result = (short int) buffer_insertObject( b, obj, true );
            if ( result == OP_SUCCESS ) {
                sAssoc = findBufferSensorAssoc( c, ID );
                if ( sAssoc != NULL ) { aggiornamento_status( &sAssoc->sensore, 1 ); }
            }
            return result;
        }
        case ENTITY_MACHINE: {
            machine_t *m = cell_getMachine( c->cell, ID );
            if ( m == NULL ) { return ERR_NOT_FOUND; }
            if ( machine_isBusy( m ) ) { return ERR_FULL; }
            return machine_admit( m, obj, step );
        }
        case ENTITY_ISP: {
            isp_t *i = cell_getISP( c->cell, ID );
            if ( i == NULL ) { return ERR_NOT_FOUND; }
            if ( isp_isBusy( i ) ) { return ERR_FULL; }
            return isp_admit( i, obj, step );
        }
        case ENTITY_NASTRO: {
            nastro_t *n = cell_getNastro( c->cell, ID );
            if ( n == NULL ) { return ERR_NOT_FOUND; }
            if ( nastro_isFull( n ) ) { return ERR_FULL; }
            return (short int) nastro_insertObject( n, obj, step );
        }
        default:
            return ERR_NOT_SUPPORTED;
    }
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
    deviatoreAssoc_t *dev;
    int outIndex;
    int outCount;

    i = cell_getISP( c->cell, ID );
    if ( i == NULL || !isp_isReady( i, step ) ) {
        return;
    }

    obj = isp_tryRelease( i, step, &esito );
    if ( obj == NULL ) {
        return;
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
    } else if ( esito == CONFORME && outCount >= 4 ) {
        /* Con 4 uscite, l'esito CONFORME da solo non basta a scegliere
         * tra le due uscite "pezzo conforme" (una per materiale): si usa
         * get_Material (calcolo su densita'/geometria, non il campo
         * object->type impostato a mano alla creazione) per decidere. */
        char materiale = get_Material( obj, &i->sensore );
        if ( materiale != 'A' && materiale != 'B' ) {
            /* get_Material non ha riconosciuto il materiale entro
             * tolleranza per nessuna delle due densita': usiamo
             * object->type come ripiego, per non perdere l'oggetto. */
            materiale = object_getType( obj );
        }
        outIndex = ( materiale == 'B' ) ? 3 : 0;
    }

    routeObject( c, ENTITY_ISP, ID, obj, outIndex, step, dev );
}

static void processMachine( controllore_t *c, const char *ID, int step )
{
    machine_t *m;
    object_t *obj;

    m = cell_getMachine( c->cell, ID );
    if ( m == NULL || !machine_isReady( m, step ) ) {
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

static short int creaSensoriBuffer( controllore_t *c )
{
    int n;
    int idx;
    char ID[IDLENGTH];
    buffer_t *b;
    bufferSensorAssoc_t *node;
    short int err;

    n = cell_getBufferCount( c->cell );
    for ( idx = 0; idx < n; idx++ ) {
        if ( cell_getBufferIDAt( c->cell, idx, ID ) != OP_SUCCESS ) { continue; }
        b = cell_getBuffer( c->cell, ID );
        if ( b == NULL ) { continue; }

        node = malloc( sizeof( bufferSensorAssoc_t ) );
        if ( node == NULL ) { return ERR_ALLOC; }

        strncpy( node->bufferID, ID, IDLENGTH - 1 );
        node->bufferID[IDLENGTH - 1] = '\0';

        err = (short int) sensore_Buffer_init( &node->sensore, ID, buffer_getCapacity( b ) );
        if ( err != OP_SUCCESS ) { free( node ); return err; }

        node->next = c->sensoriBuffer;
        c->sensoriBuffer = node;
    }

    return OP_SUCCESS;
}

controllore_t *controllore_create( cell_t *cell, double soglia_buffer, short int *errCode )
{
    controllore_t *c;
    short int err;

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
    c->completati = 0;

    err = creaSensoriBuffer( c );
    if ( err != OP_SUCCESS ) {
        controllore_destroy( c );
        if ( errCode != NULL ) { *errCode = err; }
        return NULL;
    }

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

    if ( c == NULL ) {
        return;
    }

    /* Libera solo i nodi delle liste interne: gli object_t puntati dalla
     * coda pending e la cell_t associata NON vengono liberati (il
     * controllore non ne e' proprietario). */
    for ( pCur = c->pending; pCur != NULL; pCur = pNext ) { pNext = pCur->next; free( pCur ); }
    for ( mCur = c->motori; mCur != NULL; mCur = mNext ) { mNext = mCur->next; free( mCur ); }
    for ( dCur = c->deviatori; dCur != NULL; dCur = dNext ) { dNext = dCur->next; free( dCur ); }
    for ( bCur = c->sensoriBuffer; bCur != NULL; bCur = bNext ) { bNext = bCur->next; free( bCur ); }
    for ( sCur = c->sensoriPresenza; sCur != NULL; sCur = sNext ) { sNext = sCur->next; free( sCur ); }

    free( c );
}

short int controllore_collegaMotore( controllore_t *c, const char *nastroID, int velocita_target, int accelerazione_target )
{
    motoreAssoc_t *node;
    short int err;

    if ( c == NULL || nastroID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( !cell_hasNastro( c->cell, nastroID ) ) {
        return ERR_NOT_FOUND;
    }
    if ( findMotoreAssoc( c, nastroID ) != NULL ) {
        return ERR_DUPLICATE;
    }

    node = malloc( sizeof( motoreAssoc_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    strncpy( node->nastroID, nastroID, IDLENGTH - 1 );
    node->nastroID[IDLENGTH - 1] = '\0';

    /* Bug: la versione precedente chiamava motore_init con soli 3
     * argomenti, ma la firma richiede anche accelerazione_desiderata
     * (aggiunta di recente a Motore.h) - non compilava. */
    err = (short int) motore_init( &node->motore, nastroID, velocita_target, accelerazione_target );
    if ( err != OP_SUCCESS ) {
        free( node );
        return err;
    }
    node->motoreTime.time_on = 0;
    node->motoreTime.time_off = 0;
    node->motoreTime.time_start = 0;
    node->motoreTime.time_stop = 0;

    node->next = c->motori;
    c->motori = node;

    return OP_SUCCESS;
}

short int controllore_collegaDeviatore( controllore_t *c, const char *ispID, int tempo_minimo_commutazioni )
{
    deviatoreAssoc_t *node;
    short int err;

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

int controllore_segnalaArrivo( controllore_t *c, const char *bufferIngressoID, int time_on, int presenza )
{
    presenceSensorAssoc_t *sAssoc;

    if ( c == NULL || bufferIngressoID == NULL ) {
        return ERR_NULL_PTR;
    }

    sAssoc = findOrCreatePresenceSensorAssoc( c, bufferIngressoID );
    if ( sAssoc == NULL ) {
        return ERR_ALLOC;
    }

    return get_status_presenza( &sAssoc->sensore, time_on, presenza );
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
