#ifndef S_QUALITA_H
#define S_QUALITA_H

#include <stdbool.h>

/* Rinominato da 'BufferStatus' (nome che collideva con l'enum omonimo nel
 * modulo Sensore Buffer) a 'StatoSensoreQualita'. */
typedef enum {
    QUALITA_OK     = 10,
    QUALITA_NOT_OK = 11
} StatoSensoreQualita;

typedef enum {
    CONFORME      = 0,
    RIVALUTAZIONE = 1,
    SCARTO        = 2
} TipoQualita;

/* Rinominato da 'SensoreQualità' (accento nel nome del tipo: funziona per
 * caso con UTF-8 ma non e' C portabile) a 'SensoreQualita'. */
typedef struct {
    char ID[20];
    int  target[3];                  /* valore di riferimento per materiale, indice = get_Material() */
    StatoSensoreQualita status;
    int  risultato_ultima_lettura;   /* ultimo TipoQualita calcolato */
    long letture_totali;
    long type_letture_totali[3];     /* conteggio letture per CONFORME/RIVALUTAZIONE/SCARTO */
    long anomalie_rilevate;          /* letture avvenute durante un malfunzionamento (sez. 2.1) */
} SensoreQualita;

/* Rinominato da 'MulfuctionSensor' (refuso) a 'MalfunzionamentoSensore'.
 * Rinominato il campo 'time_since_last_MulFunction'/'MultiFunction'
 * (i due file usavano due nomi diversi, incompatibili) a
 * 'time_since_last_change'. */
typedef struct {
    long last_lettura;
    int  time_since_last_change;
    int  time_error;               /* durata di funzionamento OK prima del guasto */
    int  time_ok;                  /* durata del guasto prima di tornare OK */
    bool is_malfunzionante;
    bool malfunzionamento_abilitato;
} MalfunzionamentoSensore;

/* target: array di 3 valori di riferimento (uno per materiale). Viene
 * copiato internamente. ID viene copiato internamente (max 19 caratteri). */
int  sensore_qualita_init(SensoreQualita *s, const char *ID,
                           MalfunzionamentoSensore *m,
                           bool malfunzionamento_abilitato,
                           const int target[3]);

/* Configura la durata (in passi di simulazione) dei cicli OK/guasto. */
int  sensore_qualita_imposta_guasto(MalfunzionamentoSensore *m,
                                     int time_error, int time_ok);

/* Aggiorna lo stato OK/NOT_OK del sensore in base al tempo trascorso. */
int  update_status(SensoreQualita *s, MalfunzionamentoSensore *m, int time_current);

int  get_status_qualita(const SensoreQualita *s);

/* Esegue la lettura vera e propria. oggetto_presente deve essere true
 * (altrimenti non c'e' nulla da classificare). Ritorna un TipoQualita
 * (CONFORME/RIVALUTAZIONE/SCARTO) oppure un codice ERR_* in caso di errore.
 * Se il sensore e' in malfunzionamento, la lettura viene comunque
 * prodotta (per restare fedeli a un guasto realistico) ma viene anche
 * contata come anomalia in s->anomalie_rilevate. */
int  get_qualita(SensoreQualita *s, MalfunzionamentoSensore *m,
                  int time_current, int object, bool oggetto_presente);

/* Copia i 3 contatori (CONFORME/RIVALUTAZIONE/SCARTO) in type_letture_totali,
 * che deve essere un array di almeno 3 long allocato dal chiamante. */
void get_type_letture_totali(const SensoreQualita *s, long type_letture_totali[3]);

long get_letture_totali_qualita(const SensoreQualita *s);
long get_anomalie_rilevate(const SensoreQualita *s);

/* Determina l'indice di materiale (0,1,2) usato per selezionare la
 * soglia target[] corretta. */
int  get_Material(int object);

#endif
