/**
 * @file S_Qualita.h
 * @brief Sensore di controllo qualita': agganciato a una ISP, classifica
 *        un pezzo come CONFORME/RIVALUTAZIONE/SCARTO confrontando le sue
 *        dimensioni con un target, e ne riconosce il materiale (A/B) in
 *        base a densita' e geometria (sez. 1.2 e 5.1 del progetto
 *        preliminare). Supporta anche un guasto temporizzato
 *        (MalfunzionamentoSensore, sez. 2.1/5.3).
 */
#ifndef S_QUALITA_H
#define S_QUALITA_H
#include "object.h"
#include <stdbool.h>

/** @brief Stato operativo del sensore (guasto/funzionante), vedi update_status. */
typedef enum {
    QUALITA_OK     = 10,
    QUALITA_NOT_OK = 11
} StatoSensoreQualita;

/** @brief Esito della classificazione di un pezzo, vedi get_qualita. */
typedef enum {
    CONFORME      = 0,
    RIVALUTAZIONE = 1,
    SCARTO        = 2
} TipoQualita;

/** @brief Stato di un sensore di qualita' agganciato a una ISP. */
typedef struct {
    char ID[IDLENGTH];
    double  dimensionX_target;
    double  raggio_target;
    StatoSensoreQualita status;
    int  risultato_ultima_lettura;   /**< Ultimo TipoQualita calcolato. */
    long letture_totali;
    long A;
    long B;
    long non_classificato;           /**< get_Material chiamata ma ne' A ne' B entro tolleranza (vedi get_Material). */
    long type_letture_totali[3];     /**< Conteggio letture per CONFORME/RIVALUTAZIONE/SCARTO. */
    long anomalie_rilevate;          /**< Letture avvenute durante un malfunzionamento (sez. 2.1). */
    int  tolleranza_conforme_pct;    /**< Scostamento massimo (%) da dimensionX_target/raggio_target
                                       *   per classificare CONFORME (vedi get_qualita, default 5). */
    int  tolleranza_rivalutazione_pct; /**< Scostamento massimo (%) per classificare RIVALUTAZIONE invece
                                       *   di SCARTO (vedi get_qualita, default 10). Deve restare >=
                                       *   tolleranza_conforme_pct (vedi sensore_qualita_imposta_tolleranze). */
} SensoreQualita;

/** @brief Stato del guasto temporizzato di un sensore di qualita' (sez. 2.1/5.3). */
typedef struct {
    long last_lettura;
    int  time_since_last_change;
    int  time_error;               /**< Durata di funzionamento OK prima del guasto. */
    int  time_ok;                  /**< Durata del guasto prima di tornare OK. */
    bool is_malfunzionante;
    bool malfunzionamento_abilitato;
} MalfunzionamentoSensore;

/**
 * @brief Inizializza un sensore di qualita' e il suo stato di guasto.
 * @param s Puntatore al sensore.
 * @param ID Identificativo del sensore (max 19 caratteri), copiato internamente.
 * @param m Puntatore allo stato di guasto da inizializzare insieme al sensore.
 * @param malfunzionamento_abilitato Se true, il guasto temporizzato e' attivo
 *        (vedi sensore_qualita_imposta_guasto); se false, il sensore resta
 *        sempre QUALITA_OK.
 * @param dimensionX_target Target di dimensionX per la classificazione.
 * @param raggio_target Target di raggio per la classificazione.
 * @return OP_SUCCESS se inizializzato, un codice ERR_* (vedi errors.h) altrimenti.
 */
int  sensore_qualita_init(SensoreQualita *s, const char *ID,
                           MalfunzionamentoSensore *m,
                           bool malfunzionamento_abilitato,
                           const int dimensionX_target,
                           const int raggio_target);

/**
 * @brief Configura la durata (in passi di simulazione) dei cicli OK/guasto.
 * @param m Puntatore allo stato di guasto.
 * @param time_error Passi di funzionamento OK prima del prossimo guasto (> 0).
 * @param time_ok Passi di guasto prima di tornare OK (> 0).
 * @return OP_SUCCESS se impostata, un codice ERR_* altrimenti.
 */
int  sensore_qualita_imposta_guasto(MalfunzionamentoSensore *m,
                                     int time_error, int time_ok);

/**
 * @brief Configura le soglie di classificazione (CONFORME/RIVALUTAZIONE/
 *        SCARTO) del sensore, in percentuale di scostamento da
 *        dimensionX_target/raggio_target (vedi get_qualita).
 *
 * Non obbligatoria: sensore_qualita_init imposta già i default storici
 * (5% CONFORME, 10% RIVALUTAZIONE). Va chiamata solo se serve una
 * tolleranza diversa per una specifica ISP.
 * @param s Puntatore al sensore.
 * @param tolleranza_conforme_pct Soglia (%) sotto cui un pezzo è
 *        CONFORME (deve essere > 0).
 * @param tolleranza_rivalutazione_pct Soglia (%) sotto cui un pezzo è
 *        RIVALUTAZIONE invece di SCARTO (deve essere >=
 *        tolleranza_conforme_pct).
 * @return OP_SUCCESS se impostate, ERR_OUT_OF_RANGE se i valori non
 *         rispettano i vincoli sopra, ERR_NULL_PTR se s è NULL.
 */
int  sensore_qualita_imposta_tolleranze(SensoreQualita *s,
                                         int tolleranza_conforme_pct,
                                         int tolleranza_rivalutazione_pct);

/**
 * @brief Aggiorna lo stato OK/NOT_OK del sensore in base al tempo trascorso.
 * @param s Puntatore al sensore.
 * @param m Puntatore allo stato di guasto.
 * @param time_current Step di simulazione corrente.
 * @return OP_SUCCESS se aggiornato, un codice ERR_* altrimenti.
 */
int  update_status(SensoreQualita *s, MalfunzionamentoSensore *m, int time_current);

/**
 * @brief Stato operativo attuale del sensore.
 * @param s Puntatore al sensore.
 * @return QUALITA_OK o QUALITA_NOT_OK, oppure ERR_NULL_PTR se s e' NULL.
 */
int  get_status_qualita(const SensoreQualita *s);

/**
 * @brief Classifica un pezzo confrontando le sue dimensioni col target
 *        del sensore.
 *
 * Se il sensore e' in malfunzionamento, la lettura viene comunque
 * prodotta (per restare fedeli a un guasto realistico, vedi
 * TipoQualita/RIVALUTAZIONE) ma viene anche contata come anomalia in
 * s->anomalie_rilevate.
 * @param s Puntatore al sensore.
 * @param m Puntatore allo stato di guasto.
 * @param time_current Step di simulazione corrente.
 * @param object Oggetto da classificare.
 * @param oggetto_presente Deve essere true (altrimenti non c'e' nulla
 *        da classificare).
 * @return Un TipoQualita (CONFORME/RIVALUTAZIONE/SCARTO), oppure un
 *         codice ERR_* in caso di errore o se oggetto_presente e' false.
 */
int  get_qualita(SensoreQualita *s, MalfunzionamentoSensore *m,
                  int time_current, object_t *object, bool oggetto_presente);

/**
 * @brief Copia i 3 contatori di letture (CONFORME/RIVALUTAZIONE/SCARTO).
 * @param s Puntatore al sensore.
 * @param type_letture_totali Array di almeno 3 long, allocato dal chiamante,
 *        in cui vengono copiati i contatori.
 */
void get_type_letture_totali(const SensoreQualita *s, long type_letture_totali[3]);

/**
 * @brief Numero totale di letture effettuate dal sensore.
 * @param s Puntatore al sensore.
 * @return Numero di letture, oppure ERR_NULL_PTR se s e' NULL.
 */
long get_letture_totali_qualita(const SensoreQualita *s);

/**
 * @brief Numero di letture avvenute durante un malfunzionamento del sensore.
 * @param s Puntatore al sensore.
 * @return Numero di anomalie, oppure ERR_NULL_PTR se s e' NULL.
 */
long get_anomalie_rilevate(const SensoreQualita *s);

/**
 * @brief Conteggio di quante volte get_Material ha riconosciuto il pezzo
 *        come materiale 'A' entro tolleranza.
 * @param s Puntatore al sensore.
 * @return Conteggio, oppure ERR_NULL_PTR se s e' NULL.
 */
long get_ConteggioMaterialeA(const SensoreQualita *s);

/**
 * @brief Conteggio di quante volte get_Material ha riconosciuto il pezzo
 *        come materiale 'B' entro tolleranza.
 * @param s Puntatore al sensore.
 * @return Conteggio, oppure ERR_NULL_PTR se s e' NULL.
 */
long get_ConteggioMaterialeB(const SensoreQualita *s);

/**
 * @brief Conteggio di quante volte get_Material e' stata chiamata ma NON
 *        ha riconosciuto il pezzo entro tolleranza ne' come 'A' ne' come
 *        'B' (vedi get_Material: ritorna 0 in questo caso, invece di
 *        'A'/'B'). Utile per accorgersi se dimensionX/raggio si
 *        discostano troppo dal target configurato per il materiale
 *        dichiarato dell'oggetto.
 * @param s Puntatore al sensore.
 * @return Conteggio, oppure ERR_NULL_PTR se s e' NULL.
 */
long get_ConteggioNonClassificato(const SensoreQualita *s);

/**
 * @brief Determina, in base a densita' e geometria (non al campo
 *        object->type impostato a mano alla creazione), se il pezzo e'
 *        riconoscibile come materiale 'A' o 'B' entro tolleranza.
 * @param object Oggetto da classificare.
 * @param s Puntatore al sensore (i cui contatori A/B/non_classificato
 *        vengono aggiornati).
 * @return 'A' o 'B' se riconosciuto entro tolleranza, 0 se non
 *         classificabile o su puntatore NULL.
 */
char  get_Material(object_t *object , SensoreQualita *s);

#endif
