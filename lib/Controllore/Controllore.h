/**
 * @file Controllore.h
 * @brief Libreria di controllo: orchestra il flusso degli oggetti tra
 *        le entità della cella ad ogni passo di simulazione, includendo
 *        ora il comando di Motore/Deviatore e la lettura/sincronizzazione
 *        dei sensori S_Buffer/S_Presenza.
 *
 * COSA FA OGNI PASSO (controllore_step), in quest'ordine:
 *   1. Riprova a instradare gli oggetti rimasti bloccati al passo
 *      precedente (destinazione piena, o Deviatore non ancora in
 *      posizione), invece di perderli silenziosamente.
 *   2. ISP pronte: calcola l'esito (CONFORME/RIVALUTAZIONE/SCARTO). Se
 *      alla ISP è collegato un Deviatore (controllore_collegaDeviatore),
 *      lo comanda verso la posizione corrispondente all'esito e aspetta
 *      che sia fisicamente arrivato prima di instradare l'oggetto;
 *      altrimenti instrada subito in base al solo indice di uscita
 *      (vedi convenzione in cell.h).
 *   3. Macchine pronte: rilascia l'oggetto lavorato verso il suo output.
 *   4. Nastri pronti: se collegato un Motore (controllore_collegaMotore),
 *      lo accende/spegne in base al contenuto del nastro e lo aggiorna
 *      (rampa, temperatura); poi rilascia l'oggetto trasportato.
 *   5. Buffer non vuoti: preleva l'oggetto a priorità più alta e prova
 *      ad ammetterlo nella destinazione collegata, con ammissione
 *      "buffer-aware" (Strategia 1, sez. 4.1) basata sulla lettura del
 *      SensoreBuffer associato al buffer a valle di una macchina.
 *
 * SENSORI: nessuno viene creato automaticamente. Ogni sensore/attuatore
 * (Motore, Deviatore, SensoreBuffer, SensorePresenza, SensoreQualita) va
 * agganciato esplicitamente con la relativa funzione
 * controllore_collegaX, PRIMA di poterlo usare — un'entità senza nulla
 * agganciato funziona comunque (macchina/nastro/ISP restano puri timer,
 * un buffer senza sensore semplicemente non è monitorato). Vedi
 * controllore_collegaSensoreBuffer, controllore_collegaSensorePresenza,
 * controllore_collegaSensoreQualita, controllore_collegaMotore,
 * controllore_collegaDeviatore.
 *
 * Un SensoreBuffer agganciato viene aggiornato automaticamente
 * (aggiornamento_status) ad ogni inserimento/rimozione che passa
 * attraverso genericInsert/processBuffer — quindi resta sempre
 * sincronizzato col contenuto reale del buffer una volta agganciato,
 * senza bisogno che nessun altro modulo se ne ricordi. Vedi
 * controllore_getPercentualeBuffer/controllore_getStatoBuffer per
 * leggerlo dall'esterno (es. per log) — restituiscono ERR_NOT_FOUND se
 * nessun sensore è stato agganciato per quel buffer.
 *
 * S_Presenza: l'unica applicazione prevista dal progetto è rilevare
 * l'arrivo di un NUOVO oggetto al buffer di ingresso (sez. 5.1). Va
 * agganciato con controllore_collegaSensorePresenza PRIMA di chiamare
 * controllore_segnalaArrivo per lo stesso ID (che altrimenti fallisce
 * con ERR_NOT_FOUND) — pensato per essere richiamato dal main/parser una
 * volta scritta la generazione degli arrivi.
 */

#ifndef CONTROLLORE_H
#define CONTROLLORE_H

#include "object.h"
#include "errors.h"
#include "cell.h"
#include "S_Buffer.h"
#include "S_Presenza.h"
#include "log.h"

typedef struct controllore controllore_t;

/**
 * @brief Strategia di controllo usata da processBuffer per scegliere
 *        l'oggetto da prelevare da un buffer e per decidere se ritardare
 *        l'ammissione in base all'occupazione del buffer a valle (sez.
 *        4.1 del progetto preliminare, tabella "Alternative considerate").
 *
 * STRATEGIA_PRIORITA_BUFFER_AWARE ("Strategia 1", DEFAULT, comportamento
 * identico alle versioni precedenti di questo file):
 *   - Inserimento nei buffer e prelievo da un buffer ordinati per
 *     priorità decrescente (buffer_insertObject/buffer_removeObject con
 *     priority=true): a parità di condizioni, l'oggetto a priorità più
 *     alta viene sempre servito per primo, indipendentemente da quando è
 *     arrivato.
 *   - Ammissione "buffer-aware": se la destinazione è una macchina il cui
 *     buffer a valle ha superato la soglia c->soglia_buffer, l'ammissione
 *     viene ritardata di un passo (vedi processBuffer).
 *
 * STRATEGIA_FCFS ("Strategia 2", First-Come-First-Served):
 *   - Inserimento ordinato per arrivo e prelievo dalla testa della coda
 *     (buffer_insertObject/buffer_removeObject con priority=false): gli
 *     oggetti sono serviti nello stesso ordine in cui sono arrivati,
 *     ignorando completamente il campo priorità.
 *   - Nessun controllo di soglia sul buffer a valle: l'ammissione avviene
 *     appena la destinazione è libera, senza guardare l'occupazione dei
 *     buffer (vedi tabella 4.1: "nessuna garanzia su priorità o buffer").
 *
 * Impostata da controllore_create (default STRATEGIA_PRIORITA_BUFFER_AWARE,
 * per non cambiare il comportamento di codice esistente) e modificabile in
 * qualsiasi momento con controllore_impostaStrategia — tipicamente PRIMA
 * di avviare il ciclo di simulazione, per confrontare due esecuzioni sullo
 * stesso scenario con strategie diverse (sez. "confronto tra strategie").
 */
typedef enum {
    STRATEGIA_PRIORITA_BUFFER_AWARE = 0,   /**< Strategia 1 (default). */
    STRATEGIA_FCFS = 1                     /**< Strategia 2. */
} strategia_controllo_t;

/**
 * @brief Cambia la strategia di controllo usata da controllore_step per
 *        tutti i buffer della cella (vedi strategia_controllo_t).
 *
 * Può essere chiamata anche a controllore già creato e in uso: il
 * cambio si applica dal PROSSIMO controllore_step in poi (non riordina
 * retroattivamente gli oggetti già presenti nei buffer, che restano
 * nell'ordine in cui erano stati inseriti finora - per un confronto
 * pulito tra strategie, impostarla prima di inserire il primo oggetto).
 * @param c Puntatore al controllore.
 * @param strategia La nuova strategia da usare.
 * @return OP_SUCCESS, o ERR_NULL_PTR se c è NULL.
 */
short int controllore_impostaStrategia( controllore_t *c, strategia_controllo_t strategia );

/**
 * @brief Strategia di controllo attualmente in uso (vedi
 *        controllore_impostaStrategia).
 * @param c Puntatore al controllore.
 * @return La strategia corrente, o STRATEGIA_PRIORITA_BUFFER_AWARE se
 *         c è NULL (valore di default, per sicurezza in contesti dove
 *         l'errore non può essere propagato con un codice ERR_*).
 */
strategia_controllo_t controllore_getStrategia( const controllore_t *c );

/**
 * @brief Crea un controllore agganciato a una cella già costruita.
 *
 * Il controllore NON diventa proprietario della cella: cell_destroy va
 * chiamata a parte da chi ha creato la cella. Crea automaticamente un
 * SensoreBuffer per ogni buffer già presente nella cella in questo
 * momento (i buffer aggiunti alla cella DOPO questa chiamata non hanno
 * un sensore associato: da tenere presente se si costruisce la cella in
 * più fasi).
 * @param cell Puntatore alla cella da orchestrare (deve esistere già).
 * @param soglia_buffer Soglia di occupazione (0.0-1.0, es. 0.8) oltre la
 *        quale l'ammissione in una macchina viene ritardata se il
 *        buffer a valle della macchina è troppo pieno (Strategia 1).
 * @param errCode puntatore opzionale (può essere NULL) in cui viene
 *        scritto OP_SUCCESS oppure un codice ERR_* (vedi errors.h).
 * @return Puntatore al controllore allocato, o NULL in caso di errore.
 */
controllore_t *controllore_create( cell_t *cell, double soglia_buffer, short int *errCode );

/**
 * @brief Distrugge il controllore: libera le associazioni interne
 *        (sensori, Motore, Deviatore), non la cella.
 *
 * Libera ANCHE gli object_t ancora nella coda pending (chiamando
 * object_delete su ciascuno): a differenza di buffer/nastro/macchina/ISP,
 * dove il payload resta sempre a carico del chiamante perché quelle
 * strutture sono pubbliche e iterabili da fuori (vedi buffer_delete/
 * nastro_delete/machine_delete/isp_delete), la coda pending è privata di
 * Controllore.c: nessun codice esterno potrebbe mai raggiungerla per
 * liberarla, quindi lo fa questa funzione. Gli oggetti ancora dentro
 * buffer/nastro/macchine/ISP della cella restano invece a carico del
 * chiamante: vanno liberati PRIMA di questa chiamata (vedi il main per
 * un esempio di pulizia completa).
 * @param c Puntatore al controllore.
 */
void controllore_destroy( controllore_t *c );

/**
 * @brief Collega (facoltativamente) un log eventi al controllore.
 *
 * Da qui in poi controllore_step registra nel log, NEL MOMENTO ESATTO
 * in cui accadono, gli eventi rilevanti che il controllore osserva
 * direttamente durante l'orchestrazione (oggi: il completamento di un
 * oggetto in un buffer terminale, vedi segnaCompletatoSeTerminale) -
 * a differenza di un log "a posteriori" costruito scandendo lo stato
 * finale della cella, questo mantiene l'ordine cronologico reale degli
 * eventi nel file di log.
 *
 * Facoltativo: se non chiamata (o chiamata con log == NULL), il
 * controllore funziona esattamente come prima, senza loggare nulla
 * (log_evento con puntatore NULL non fa nulla, vedi log.h).
 * @param c Puntatore al controllore.
 * @param log Puntatore al log (vedi log.h), o NULL per scollegarlo.
 * @return OP_SUCCESS, o ERR_NULL_PTR se c è NULL.
 */
short int controllore_collegaLog( controllore_t *c, log_t *log );

/**
 * @brief Collega un Motore a un nastro O a una macchina già presente
 *        nella cella.
 *
 * Da qui in poi controllore_step accenderà/spegnerà questo motore e lo
 * aggiornerà ogni passo (rampa, temperatura):
 *   - su un NASTRO, acceso quando c'è qualcosa da trasportare, spento
 *     quando è vuoto — e finché è spento, blocca i nuovi ingressi
 *     (vedi processNastro).
 *   - su una MACCHINA, acceso mentre sta lavorando un pezzo, spento
 *     quando è libera — qui invece NON blocca la lavorazione: riflette
 *     solo lo stato della macchina, non lo condiziona.
 * Un'entità senza motore collegato continua a funzionare come prima
 * (nessun obbligo di collegarne uno).
 * @param c Puntatore al controllore.
 * @param targetID ID del nastro o della macchina (deve già esistere nella cella).
 * @param velocita_target Velocità target del motore (vedi Motore.h).
 * @return OP_SUCCESS se collegato, un codice ERR_* (vedi errors.h) altrimenti,
 *         incluso ERR_NOT_FOUND se targetID non esiste nella cella,
 *         ERR_NOT_SUPPORTED se targetID non è né un nastro né una macchina.
 */
short int controllore_collegaMotore( controllore_t *c, const char *targetID, int velocita_target, int accelerazione_target );

/**
 * @brief Collega un Deviatore a una ISP già presente nella cella.
 *
 * Da qui in poi, quando questa ISP rilascia un oggetto, il controllore
 * comanda il Deviatore verso la posizione corrispondente all'esito e
 * aspetta che sia fisicamente in posizione (rispettando il tempo minimo
 * tra commutazioni) prima di instradare l'oggetto. Una ISP senza
 * Deviatore collegato instrada subito, senza aspettare nessuna commutazione.
 * @param c Puntatore al controllore.
 * @param ispID ID della ISP (deve già esistere nella cella).
 * @param tempo_minimo_commutazioni Tempo minimo (passi di simulazione)
 *        tra due commutazioni consecutive del Deviatore (sez. 5.2).
 * @return OP_SUCCESS se collegato, un codice ERR_* (vedi errors.h) altrimenti,
 *         incluso ERR_NOT_FOUND se ispID non esiste nella cella.
 */
short int controllore_collegaDeviatore( controllore_t *c, const char *ispID, int tempo_minimo_commutazioni );

/**
 * @brief Aggancia un sensore buffer a un buffer già presente nella cella.
 *
 * Senza questa chiamata, il buffer non ha nessun sensore:
 * controllore_getPercentualeBuffer/getStatoBuffer restituiscono
 * ERR_NOT_FOUND per quel buffer, e l'ammissione "buffer-aware"
 * (Strategia 1) salta il controllo di soglia per lui (non blocca comunque
 * l'ammissione, la tratta semplicemente come "nessun limite noto").
 * @param c Puntatore al controllore.
 * @param bufferID ID del buffer (deve già esistere nella cella).
 * @return OP_SUCCESS se agganciato, un codice ERR_* (vedi errors.h) altrimenti,
 *         incluso ERR_NOT_FOUND se bufferID non esiste nella cella, ERR_DUPLICATE
 *         se questo buffer ha già un sensore agganciato.
 */
short int controllore_collegaSensoreBuffer( controllore_t *c, const char *bufferID );

/**
 * @brief Aggancia un sensore di presenza a un ID di ingresso.
 *
 * Va chiamata PRIMA di usare controllore_segnalaArrivo per lo stesso ID:
 * senza un sensore agganciato, controllore_segnalaArrivo fallisce con
 * ERR_NOT_FOUND. L'ID non deve necessariamente esistere già nella cella
 * come buffer (il sensore è indipendente dal buffer vero e proprio).
 * @param c Puntatore al controllore.
 * @param ID Identificativo del punto di ingresso da monitorare (es. "B1").
 * @return OP_SUCCESS se agganciato, un codice ERR_* (vedi errors.h) altrimenti,
 *         incluso ERR_DUPLICATE se questo ID ha già un sensore agganciato.
 */
short int controllore_collegaSensorePresenza( controllore_t *c, const char *ID );

/**
 * @brief Aggancia un sensore di qualità a una ISP già presente nella cella.
 *
 * Senza questa chiamata, una ISP è un puro timer (isp_admit/
 * isp_tryRelease) e non giudica mai la qualità di quello che passa: una
 * ISP davvero "passacarte" (nessun controllo qualità, nemmeno solo per
 * le statistiche) può restare senza sensore agganciato, senza bisogno
 * di nessun target fittizio. Se invece serve solo instradare sempre
 * sull'unica uscita pur continuando a registrare l'esito nelle
 * statistiche, agganciare comunque il sensore e lasciare il criterio di
 * smistamento sul default (vedi tipo_smistamento_t/SMISTAMENTO_AUTO).
 * @param c Puntatore al controllore.
 * @param ispID ID della ISP (deve già esistere nella cella).
 * @param dimensionX_target Valore di riferimento per la dimensione, vedi S_Qualita.h.
 * @param raggio_target Valore di riferimento per il raggio, vedi S_Qualita.h.
 * @return OP_SUCCESS se agganciato, un codice ERR_* (vedi errors.h) altrimenti,
 *         incluso ERR_NOT_FOUND se ispID non esiste nella cella, ERR_DUPLICATE
 *         se questa ISP ha già un sensore agganciato.
 */
short int controllore_collegaSensoreQualita( controllore_t *c, const char *ispID,
                                              int dimensionX_target, int raggio_target );

/**
 * @brief Abilita/disabilita e configura il guasto simulato del sensore
 *        di qualità agganciato a una ISP (sez. 5.3 del progetto).
 * @param c Puntatore al controllore.
 * @param ispID ID della ISP il cui sensore va configurato.
 * @param abilitato true per abilitare il guasto.
 * @param time_error Passi di funzionamento OK prima del guasto (deve essere > 0).
 * @param time_ok Passi di guasto prima di tornare OK (deve essere > 0).
 * @return OP_SUCCESS se impostato, ERR_NOT_FOUND se questa ISP non ha nessun
 *         sensore agganciato (vedi controllore_collegaSensoreQualita),
 *         un altro codice ERR_* (vedi errors.h) altrimenti.
 */
short int controllore_impostaGuastoQualita( controllore_t *c, const char *ispID,
                                             bool abilitato, int time_error, int time_ok );

/**
 * @brief Configura le soglie di classificazione (CONFORME/RIVALUTAZIONE/
 *        SCARTO) del sensore di qualità agganciato a una ISP, in
 *        percentuale di scostamento dal target (vedi
 *        sensore_qualita_imposta_tolleranze in S_Qualita.h).
 *
 * Non obbligatoria: controllore_collegaSensoreQualita imposta già i
 * default storici (5% CONFORME, 10% RIVALUTAZIONE). Va chiamata solo se
 * serve una tolleranza diversa per una specifica ISP (es. da
 * TOLLERANZA_CONFORME/TOLLERANZA_RIVALUTAZIONE nel plant_config, vedi
 * parser_collegaSensoriQualita).
 * @param c Puntatore al controllore.
 * @param ispID ID della ISP il cui sensore va configurato.
 * @param tolleranza_conforme_pct Soglia (%) sotto cui un pezzo è
 *        CONFORME (deve essere > 0).
 * @param tolleranza_rivalutazione_pct Soglia (%) sotto cui un pezzo è
 *        RIVALUTAZIONE invece di SCARTO (deve essere >=
 *        tolleranza_conforme_pct).
 * @return OP_SUCCESS se impostate, ERR_NOT_FOUND se questa ISP non ha
 *         nessun sensore agganciato (vedi controllore_collegaSensoreQualita),
 *         ERR_OUT_OF_RANGE se i valori non rispettano i vincoli sopra,
 *         un altro codice ERR_* (vedi errors.h) altrimenti.
 */
short int controllore_impostaToleranzaQualita( controllore_t *c, const char *ispID,
                                                int tolleranza_conforme_pct, int tolleranza_rivalutazione_pct );

/**
 * @brief Criterio con cui una ISP sceglie l'uscita su cui instradare un
 *        pezzo appena controllato, tra quelle collegate (vedi
 *        cell_connect) e l'eventuale Deviatore agganciato (vedi
 *        controllore_collegaDeviatore).
 *
 * Pensato per riusare la STESSA logica di instradamento (in
 * Controllore.c, non duplicata) qualunque sia il layout descritto dal
 * plant_config: cambia solo il numero di uscite collegate via CONNECT e
 * il tipo di smistamento dichiarato per l'ISP (vedi
 * controllore_impostaSmistamentoQualita / campo SMISTAMENTO nel
 * plant_config), senza bisogno di toccare il codice per adattarsi a un
 * layout diverso (es. layout 1 vs layout 2 del progetto preliminare).
 */
typedef enum {
    /** @brief Dedotto automaticamente dal numero di uscite collegate
     *  (comportamento storico, sempre corretto per 1 o per 4+ uscite -
     *  vedi SMISTAMENTO_PASSACARTE/SMISTAMENTO_MATERIALE_E_QUALITA
     *  sotto). Con un numero di uscite diverso (2 o 3) l'inferenza
     *  automatica è ambigua: viene trattato come SMISTAMENTO_QUALITA
     *  (il caso più comune per un'ISP con più uscite dopo una
     *  macchina), ma è consigliato dichiarare il tipo esplicitamente
     *  in quei casi invece di affidarsi al default. È il valore
     *  impostato di default da controllore_collegaSensoreQualita. */
    SMISTAMENTO_AUTO = 0,
    /** @brief Nessuna decisione: instrada sempre sull'unica uscita
     *  collegata (indice 0), a prescindere dall'esito del controllo -
     *  es. la prima ISP di un layout a più stadi, che serve solo a
     *  far scorrere il pezzo verso la stazione successiva. L'esito
     *  viene comunque calcolato e registrato nelle statistiche, solo
     *  non usato per instradare. */
    SMISTAMENTO_PASSACARTE = 1,
    /** @brief Instrada SOLO in base al materiale riconosciuto (vedi
     *  get_Material in S_Qualita.h), ignorando l'esito qualità:
     *  indice 0 = materiale 'A', indice 1 = materiale 'B'. Un pezzo
     *  non classificabile va sull'ultima uscita collegata se ce ne
     *  sono almeno 3 (una dedicata ai non classificati), altrimenti
     *  (solo 2 uscite) resta sull'uscita 0. Tipico di un'ISP posta
     *  PRIMA della lavorazione, per smistare il flusso su linee
     *  dedicate per materiale (es. ISP0 del layout 2). */
    SMISTAMENTO_MATERIALE = 2,
    /** @brief Instrada SOLO in base all'esito qualità (CONFORME/
     *  RIVALUTAZIONE/SCARTO), ignorando il materiale: indice = esito
     *  diretto (0/1/2, vedi TipoQualita in S_Qualita.h). Tipico di
     *  un'ISP posta DOPO la lavorazione su una linea già dedicata a
     *  un solo materiale (il materiale è già implicito nella linea,
     *  non serve ridistinguerlo qui - es. le ISP dopo M nel layout 2). */
    SMISTAMENTO_QUALITA = 3,
    /** @brief Instrada in base a ENTRAMBI materiale e qualità insieme,
     *  su 4 uscite: indice 0 = CONFORME+materiale 'A', 1 =
     *  RIVALUTAZIONE (anche per un pezzo CONFORME ma non
     *  classificabile per materiale), 2 = SCARTO, 3 = CONFORME+
     *  materiale 'B'. Comportamento storico dell'unica ISP del layout
     *  1 con smistamento finale (es. ISP2). */
    SMISTAMENTO_MATERIALE_E_QUALITA = 4
} tipo_smistamento_t;

/**
 * @brief Configura il criterio di smistamento del sensore di qualità
 *        agganciato a una ISP (vedi tipo_smistamento_t).
 *
 * Non obbligatoria: controllore_collegaSensoreQualita imposta già
 * SMISTAMENTO_AUTO, che copre correttamente i due casi storici (1
 * uscita o 4+ uscite) senza bisogno di questa chiamata. Va chiamata
 * esplicitamente solo per i casi ambigui per il numero di uscite (2 o 3
 * uscite, vedi SMISTAMENTO_MATERIALE/SMISTAMENTO_QUALITA) o per
 * dichiarare comunque l'intento in modo esplicito.
 * @param c Puntatore al controllore.
 * @param ispID ID della ISP il cui sensore va configurato.
 * @param tipo Il criterio di smistamento da usare.
 * @return OP_SUCCESS se impostato, ERR_NOT_FOUND se questa ISP non ha
 *         nessun sensore agganciato (vedi controllore_collegaSensoreQualita),
 *         un altro codice ERR_* (vedi errors.h) altrimenti.
 */
short int controllore_impostaSmistamentoQualita( controllore_t *c, const char *ispID, tipo_smistamento_t tipo );

/**
 * @brief Esegue un passo di controllo: scandisce tutte le entità della
 *        cella e fa avanzare gli oggetti pronti verso la destinazione
 *        successiva (vedi ordine dettagliato in testa al file).
 * @param c Puntatore al controllore.
 * @param step_corrente Step di simulazione corrente (tempo globale).
 * @return OP_SUCCESS se il passo è stato eseguito, ERR_NULL_PTR se c è NULL.
 */
short int controllore_step( controllore_t *c, int step_corrente );

/**
 * @brief Percentuale di occupazione (0-100) del buffer, letta dal
 *        SensoreBuffer associato (creato automaticamente da
 *        controllore_create).
 * @param c Puntatore al controllore.
 * @param bufferID ID del buffer.
 * @return Percentuale di occupazione, oppure ERR_NULL_PTR/ERR_NOT_FOUND
 *         (vedi errors.h) se non trovato.
 */
int controllore_getPercentualeBuffer( const controllore_t *c, const char *bufferID );

/**
 * @brief Percentuale di occupazione MASSIMA transitoria (0-100) toccata
 *        dal buffer da quando questa funzione è stata chiamata l'ultima
 *        volta (o da quando il sensore è stato creato, la prima volta).
 *
 * A differenza di controllore_getPercentualeBuffer (che legge il livello
 * ISTANTANEO al momento della chiamata), questa cattura anche i picchi
 * transitori che si verificano e si esauriscono nello stesso passo di
 * simulazione - es. un buffer riempito da una macchina e subito dopo
 * svuotato verso l'ISP successiva nella stessa chiamata a
 * controllore_step, che un singolo poll a fine passo (come fa
 * statistiche_campiona) non vedrebbe mai, leggendo sempre 0%.
 *
 * Ogni chiamata RESETTA la finestra di osservazione: il prossimo picco
 * riparte dal livello attuale del buffer (non da 0), pensata per essere
 * chiamata una volta per passo di simulazione (vedi statistiche_campiona).
 * @param c Puntatore al controllore (non const: la chiamata resetta lo
 *        stato interno di tracciamento del picco).
 * @param bufferID ID del buffer.
 * @return Percentuale di picco osservata nella finestra appena chiusa,
 *         oppure ERR_NULL_PTR/ERR_NOT_FOUND (vedi errors.h).
 */
int controllore_getPercentualePiccoBuffer( controllore_t *c, const char *bufferID );

/**
 * @brief Stato sintetico (BUFFER_EMPTY/BUFFER_FULL, vedi S_Buffer.h) del
 *        buffer, letto dal SensoreBuffer associato.
 * @param c Puntatore al controllore.
 * @param bufferID ID del buffer.
 * @return Lo stato, oppure ERR_NULL_PTR/ERR_NOT_FOUND se non trovato.
 */
int controllore_getStatoBuffer( const controllore_t *c, const char *bufferID );

/**
 * @brief Controlla se un Motore è agganciato a targetID (vedi
 *        controllore_collegaMotore), senza doverne leggere i tempi.
 *
 * Utile per decidere dinamicamente quali entità monitorare nelle
 * statistiche (statistiche_monitoraMotore) senza dover conoscere in
 * anticipo i nomi usati dal plant_config - importante per non legare il
 * codice a un layout specifico (vedi discussione in README, sezione
 * "Layout 2").
 * @param c Puntatore al controllore.
 * @param targetID ID del nastro o della macchina.
 * @return true se un Motore è agganciato a targetID, false altrimenti
 *         (anche se c o targetID sono NULL).
 */
bool controllore_haMotoreCollegato( const controllore_t *c, const char *targetID );

/**
 * @brief Tempo cumulativo (in passi di simulazione) in cui il motore
 *        collegato a targetID (un nastro o una macchina, vedi
 *        controllore_collegaMotore) è stato ACCESO, su tutta la
 *        simulazione da quando è stato agganciato.
 * @param c Puntatore al controllore.
 * @param targetID ID del nastro o della macchina.
 * @return Il tempo cumulativo, oppure ERR_NULL_PTR/ERR_NOT_FOUND (vedi
 *         errors.h) se nessun motore è agganciato a targetID.
 */
long controllore_getTempoMotoreOn( const controllore_t *c, const char *targetID );

/**
 * @brief Come controllore_getTempoMotoreOn, ma per il tempo cumulativo
 *        in cui il motore è stato SPENTO.
 */
long controllore_getTempoMotoreOff( const controllore_t *c, const char *targetID );

/**
 * @brief Letture totali del sensore di qualità agganciato a un'ISP
 *        (vedi controllore_collegaSensoreQualita).
 * @param c Puntatore al controllore.
 * @param ispID ID dell'ISP.
 * @return Il conteggio, oppure ERR_NULL_PTR/ERR_NOT_FOUND (vedi
 *         errors.h) se nessun sensore di qualità è agganciato a ispID.
 */
long controllore_getLettureQualita( const controllore_t *c, const char *ispID );

/**
 * @brief Anomalie rilevate dal sensore di qualità agganciato a un'ISP
 *        (letture avvenute durante un malfunzionamento simulato, vedi
 *        controllore_impostaGuastoQualita). Con la regola attuale
 *        "guasto = stazione indisponibile" (vedi processISP in
 *        Controllore.c) questo contatore resta quasi sempre a 0: durante
 *        il guasto la ISP non produce più letture (le trattiene), non ne
 *        produce di sbagliate. Per osservare l'effetto del guasto usare
 *        controllore_getStepBloccoGuasto.
 */
long controllore_getAnomalieQualita( const controllore_t *c, const char *ispID );

/**
 * @brief Numero di passi di simulazione in cui questa ISP era pronta a
 *        rilasciare un pezzo (tempo di controllo scaduto) ma lo ha
 *        TRATTENUTO perché il sensore di qualità era in quel momento
 *        guasto (vedi processISP/genericIsAvailable in Controllore.c:
 *        durante il guasto la ISP non accetta nuovi pezzi e non rilascia
 *        quello che ha in controllo, "stazione indisponibile" invece di
 *        "lettura sbagliata").
 * @param c Puntatore al controllore.
 * @param ispID ID dell'ISP.
 * @return Il conteggio, oppure ERR_NULL_PTR/ERR_NOT_FOUND (vedi
 *         errors.h) se nessun sensore di qualità è agganciato a ispID.
 */
long controllore_getStepBloccoGuasto( const controllore_t *c, const char *ispID );

/**
 * @brief Copia in out[3] i conteggi CONFORME/RIVALUTAZIONE/SCARTO del
 *        sensore di qualità agganciato a un'ISP.
 * @param c Puntatore al controllore.
 * @param ispID ID dell'ISP.
 * @param out Array di almeno 3 long, allocato dal chiamante.
 * @return OP_SUCCESS se copiato, ERR_NULL_PTR/ERR_NOT_FOUND (vedi
 *         errors.h) altrimenti (out non viene toccato in quel caso).
 */
short int controllore_getTipoLettureQualita( const controllore_t *c, const char *ispID, long out[3] );

/**
 * @brief Numero di pezzi riconosciuti come materiale 'A' (rispettivamente
 *        'B') da get_Material, sul sensore di qualità agganciato a
 *        un'ISP (vedi Controllore.c/processISP). get_Material viene
 *        chiamata per ogni oggetto rilasciato da una ISP con sensore
 *        agganciato, indipendentemente dal numero di uscite: su una ISP
 *        a singola uscita (es. ISP1) serve solo a popolare questi
 *        contatori, mentre su una ISP con 4 uscite (es. ISP2) il
 *        risultato viene usato anche per scegliere tra le due uscite
 *        "conforme".
 */
long controllore_getMaterialeA( const controllore_t *c, const char *ispID );
long controllore_getMaterialeB( const controllore_t *c, const char *ispID );

/**
 * @brief Numero di volte in cui get_Material e' stata chiamata per
 *        questa ISP ma NON ha riconosciuto il pezzo entro tolleranza
 *        ne' come materiale A ne' come B (vedi S_Qualita.h). Prima
 *        dell'introduzione di questo contatore, questi casi restavano
 *        invisibili: get_Material restituiva 0 ma nessuno lo contava.
 *        Un valore alto segnala pezzi le cui dimensioni si discostano
 *        troppo dal target configurato per ENTRAMBI i materiali noti
 *        (o, se il file oggetti in ingresso permette tipi diversi da
 *        'A'/'B', un tipo non gestito da get_Material).
 */
long controllore_getMaterialeNonClassificato( const controllore_t *c, const char *ispID );

/**
 * @brief Letture totali del sensore di presenza agganciato a un ID
 *        (vedi controllore_collegaSensorePresenza).
 * @param c Puntatore al controllore.
 * @param ID ID a cui è agganciato il sensore (es. "B1").
 * @return Il conteggio, oppure ERR_NULL_PTR/ERR_NOT_FOUND (vedi
 *         errors.h) se nessun sensore di presenza è agganciato a ID.
 */
long controllore_getLetturePresenza( const controllore_t *c, const char *ID );

/**
 * @brief Rilevamenti totali (fronti di salita, cioè nuovi arrivi
 *        distinti) del sensore di presenza agganciato a un ID.
 */
long controllore_getRilevamentiPresenza( const controllore_t *c, const char *ID );

/**
 * @brief Inoltra una lettura al sensore di presenza associato a
 *        bufferIngressoID (creato al primo utilizzo per quell'ID).
 *
 * Pensata per essere chiamata dal main/parser una volta scritta la
 * generazione degli arrivi: rileva se time_on/presenza corrispondono a
 * un nuovo oggetto arrivato (fronte di salita), coerentemente con
 * S_Presenza.h.
 * @param c Puntatore al controllore.
 * @param bufferIngressoID ID del buffer di ingresso a cui è associato
 *        il sensore (non deve necessariamente esistere già nella cella:
 *        il sensore è indipendente dal buffer vero e proprio).
 * @param time_on Vedi get_status_presenza in S_Presenza.h.
 * @param presenza Vedi get_status_presenza in S_Presenza.h.
 * @return Uno tra ENTRATA/USCITA/PRESENZA_PROLUNGATA/ASSENZA_PROLUNGATA
 *         (vedi S_Presenza.h), oppure un codice ERR_* in caso di errore.
 */
int controllore_segnalaArrivo( controllore_t *c, const char *bufferIngressoID, int time_on, int presenza );

/**
 * @brief Inserisce un oggetto arrivato dall'ESTERNO della cella in un
 *        buffer di ingresso, aggiornando correttamente anche l'eventuale
 *        SensoreBuffer agganciato (stesso percorso di sincronizzazione
 *        usato internamente da controllore_step per ogni movimento tra
 *        entità della cella).
 *
 * Va usata al posto di chiamare buffer_insertObject direttamente su un
 * buffer di ingresso: un inserimento diretto bypassa il SensoreBuffer,
 * che resterebbe permanentemente disallineato dal reale livello del
 * buffer (letture di occupazione sempre a 0%, indipendentemente da
 * quanti oggetti siano davvero in coda - vedi
 * controllore_getPercentualeBuffer/statistiche_campiona).
 * @param c Puntatore al controllore.
 * @param bufferID ID del buffer di ingresso (deve già esistere nella cella).
 * @param obj Puntatore all'oggetto da inserire.
 * @param step Step di simulazione corrente.
 * @return OP_SUCCESS se inserito, ERR_FULL se il buffer è pieno,
 *         ERR_NOT_FOUND se bufferID non esiste, un altro codice ERR_*
 *         (vedi errors.h) altrimenti.
 */
short int controllore_ammettiArrivo( controllore_t *c, const char *bufferID, object_t *obj, int step );

/**
 * @brief Schedula l'ammissione di un oggetto in un buffer di ingresso
 *        per uno step FUTURO (o passato/corrente: vedi sotto), invece di
 *        inserirlo subito - pensata per gli arrivi letti da un file
 *        oggetti (vedi parser_caricaOggetti in parser.c), dove ogni riga
 *        ha un proprio ARRIVAL_STEP diverso, a differenza del backlog di
 *        prova generato in main.c (che inserisce tutto allo step 0).
 *
 * L'oggetto NON entra subito nella cella: resta in una coda interna del
 * controllore finche' controllore_step non raggiunge arrival_step, poi
 * viene ammesso automaticamente con lo stesso percorso di
 * controllore_ammettiArrivo (SensoreBuffer aggiornato correttamente). Se
 * il buffer e' pieno esattamente ad arrival_step, l'ammissione viene
 * ritentata ai passi successivi finche' non trova posto - non viene mai
 * perso ne' saltato. Se arrival_step e' <= allo step corrente al momento
 * della chiamata (arrivo "nel passato" o "adesso"), verra' ammesso al
 * PROSSIMO controllore_step (non immediatamente da questa funzione).
 *
 * L'object_t passato diventa di proprieta' del controllore: se la
 * simulazione termina prima che l'oggetto riesca ad essere ammesso (mai
 * raggiunto il suo arrival_step, o buffer sempre pieno), viene liberato
 * automaticamente da controllore_destroy - non liberarlo separatamente.
 *
 * @param c Puntatore al controllore.
 * @param bufferID ID del buffer di ingresso (deve già esistere nella
 *        cella al momento in cui arrival_step viene raggiunto, non
 *        necessariamente ora).
 * @param obj Puntatore all'oggetto da schedulare (proprietà trasferita
 *        al controllore, vedi sopra).
 * @param arrival_step Step di simulazione da cui l'oggetto diventa
 *        ammissibile.
 * @return OP_SUCCESS se schedulato, ERR_NULL_PTR se c/bufferID/obj sono
 *         NULL, ERR_ID_INVALID se bufferID è troppo lungo, ERR_ALLOC se
 *         la malloc interna fallisce.
 */
short int controllore_schedulaArrivo( controllore_t *c, const char *bufferID, object_t *obj, int arrival_step );

/**
 * @brief Numero di oggetti usciti dalla linea (arrivati a un'entità
 *        senza nessuna uscita collegata) da quando il controllore esiste.
 * @param c Puntatore al controllore.
 * @return Numero di oggetti completati, oppure ERR_NULL_PTR se c è NULL.
 */
long controllore_getCompletati( const controllore_t *c );

/**
 * @brief Numero di oggetti attualmente bloccati in coda "pending"
 *        (pronti per essere instradati ma con la destinazione piena, o
 *        in attesa che un Deviatore raggiunga la posizione richiesta).
 * @param c Puntatore al controllore.
 * @return Numero di oggetti in coda, oppure ERR_NULL_PTR se c è NULL.
 */
int controllore_getPendingCount( const controllore_t *c );

/**
 * @brief Numero di arrivi ESTERNI ancora schedulati (vedi
 *        controllore_schedulaArrivo): oggetti letti da un file oggetti
 *        il cui ARRIVAL_STEP non è ancora stato raggiunto, o è stato
 *        raggiunto ma il buffer di ingresso era pieno.
 * @param c Puntatore al controllore.
 * @return Numero di arrivi ancora in coda, oppure ERR_NULL_PTR se c è NULL.
 */
int controllore_getArriviSchedulatiCount( const controllore_t *c );

/**
 * @brief Ammette subito nei rispettivi buffer tutti gli arrivi schedulati
 *        (vedi controllore_schedulaArrivo) il cui arrival_step è <= step.
 *
 * E' lo stesso passo "1ter" che controllore_step esegue automaticamente
 * a inizio di ogni passo di simulazione (ammissione arrivi + segnalazione
 * al sensore di presenza) - esposta qui a parte per poter far entrare
 * fisicamente nei buffer gli oggetti con ARRIVAL_STEP=0 PRIMA di stampare
 * lo "stato iniziale" in main.c, senza dover eseguire anche il resto
 * della logica di controllore_step (ISP/macchine/nastri), che
 * avanzerebbe la simulazione vera e propria.
 *
 * Chiamarla con step=0 prima del ciclo di simulazione e poi lasciare che
 * il ciclo parta comunque da step=0 è sicuro: gli oggetti già ammessi
 * qui vengono rimossi dalla coda interna, quindi il primo
 * controllore_step(ctrl, 0) li ritrova già fuori dalla coda e non li
 * riammette una seconda volta (idempotente).
 * @param c Puntatore al controllore.
 * @param step Step di simulazione da usare come riferimento (tipicamente 0).
 */
void controllore_ammettiArriviSchedulati( controllore_t *c, int step );

/**
 * @brief Stampa lo stato della cella orchestrata e le statistiche del
 *        controllore (completati, pending).
 * @param c Puntatore al controllore.
 */
void controllore_print( const controllore_t *c );

#endif /* CONTROLLORE_H */
