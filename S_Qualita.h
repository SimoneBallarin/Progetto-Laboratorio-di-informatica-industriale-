#ifndef S_Qualita_H
#define S_Qualita_H

#include <stdbool.h>

typedef struct {
    int target[3];
    int ID;
    int status; //
    bool isempty; //presenza dell'oggetto o meno
    int risult; 
    long letture_totali;
    long type_letture_totali[3]; /* numero di letture per ogni tipo di qualità */
}SensoreQualità;


typedef struct { // struttura per Malfunzionamento del sensore di qualità
    int last_letterua;
    int time_since_last_MulFunction;
    int time_error;
    int time_ok;
    bool isMulFunction;
    bool enabledMultiFunction;
}MulfuctionSensor;

typedef enum { 
    OK = 10,
    NOT_OK = 11,
}BufferStatus;

typedef enum {
    CONFORME = 0,
    RIVALUTAZIONE = 1,
    SCARTO = 2,
}TipoQualità;


void sensore_qualita_init(SensoreQualità *s, int ID, MulfuctionSensor *m, bool enableMultiFunction);
void update_status(SensoreQualità *s, MulfuctionSensor *m, int time_current );
int get_status_sensore(SensoreQualità *s);
int get_qualita(SensoreQualità *s, MulfuctionSensor *m, int time_current, int object);
void get_type_letture_totali(SensoreQualità *s, long *type_letture_totali);
long get_letture_totali(SensoreQualità *s);
int get_Material(int object);
int get_peso(int object);

#endif