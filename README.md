# Simulazione e controllo di una cella meccatronica flessibile

Progetto Finale — Gruppo I MEJO

Simulazione a passi discreti di una cella composta da nastri trasportatori, buffer a
capacità limitata, una stazione di lavorazione (M), due stazioni di controllo qualità
(ISP1/ISP2) e uno smistamento finale a quattro vie. Il layout, gli obiettivi e le scelte
progettuali sono descritti in dettaglio in `progetto preliminare gruppo I MEJO.pdf`.

Flusso della cella:

```
[ingresso] -> B1 -> ISP1 -[N1]-> M -> B2 -> ISP2 -+-> B_Alacciaio  (conforme, materiale A)
                                                    +-> B_riqualifica (rivalutazione)
                                                    +-> B_TRASH       (scarto)
                                                    +-> B_rame        (conforme, materiale B)
```

## Struttura del progetto

Tutti i moduli (`.c`/`.h`) si trovano direttamente nella cartella principale del
repository, senza sottocartelle `lib/`/`app/`/`src/`: una scelta pensata per restare
semplice con un progetto di queste dimensioni.

```
main.c                    punto d'ingresso della simulazione
Controllore.c/.h          orchestrazione: ammissione, instradamento, strategie di controllo
cell.c/.h                 struttura della cella (buffer, nastri, macchine, ISP collegati)
buffer.c/.h               buffer a capacità limitata (liste concatenate)
nastro.c/.h               nastro trasportatore
machine.c/.h              stazione di lavorazione (M)
isp.c/.h                  stazione di controllo qualità
Motore.c/.h               attuatore motore (nastri/macchine)
Deviatore.c/.h            attuatore di smistamento (ISP a più uscite)
S_Presenza.c/.h           sensore di presenza (arrivo in B1)
S_Buffer.c/.h             sensore di livello buffer
S_Qualita.c/.h            sensore di qualità (classificazione + guasto simulato)
object.c/.h               oggetto/pezzo che attraversa la cella
parser.c/.h               lettura dei file di configurazione/scenario/oggetti
statistiche.c/.h          raccolta e stampa delle metriche di simulazione
log.c/.h                  log eventi su file (ed eventualmente su stdout)
registry.c/.h             registro globale delle entità (una sola cella per esecuzione)
idlist.c/.h               lista concatenata generica di ID
errors.h                  codici di errore condivisi
test/                     test unitari Unity (vedi sotto)
CMakeLists.txt            build alternativa via CMake (vedi sotto)
plant_config_valid.txt          file di configurazione impianto di default
plant_config_layout1.txt        configurazione impianto — layout 1 (vedi PDF, sez. 1.1)
plant_config_layout2.txt        configurazione impianto — layout 2 (vedi PDF, sez. 1.1)
scenario_nominale.txt           scenario di default (nessun guasto)
scenario_nominale_layout1.txt   scenario nominale — layout 1
scenario_nominale_layout2.txt   scenario nominale — layout 2
scenario_difficile.txt          scenario con carico maggiore + guasto sensore qualità
scenario_difficile_layout1.txt  scenario difficile — layout 1
scenario_difficile_layout2.txt  scenario difficile — layout 2
scenario_doppio_guasto.txt  scenario con guasto simultaneo su ISP1 e ISP2
oggetti_esempio.txt       file oggetti di esempio per il backlog di B1
oggetti_b2_esempio.txt    file oggetti di esempio per il pre-caricamento di B2
```

## Come compilare

Due modi equivalenti:

**1. Compilazione diretta con gcc**, stessa convenzione usata da `test/run_tests.sh`
(tutti i `.c` della cartella principale, include `-I.`):

```bash
gcc -g -Wall -Wextra -std=c11 -DUNITY_INCLUDE_DOUBLE -I. \
    main.c Controllore.c Deviatore.c Motore.c S_Buffer.c S_Presenza.c S_Qualita.c \
    buffer.c cell.c idlist.c isp.c log.c machine.c nastro.c object.c parser.c \
    registry.c statistiche.c \
    -o programma -lm
```

**2. Con CMake** (produce anche i target di test, vedi sotto):

```bash
cmake -S . -B build_cmake
cmake --build build_cmake
# eseguibile in build_cmake/main
```

## Come eseguire

```bash
./programma [config_path] [oggetti_path] [scenario_path] [oggetti_b2_path]
```

Tutti e quattro gli argomenti sono opzionali:

- `config_path`: default `plant_config_valid.txt`.
- `oggetti_path`: **nessun default** — se omesso (o passato come `-`), gli arrivi in B1
  sono generati casualmente (`SIM_PEZZI` pezzi, tutti a step 0, vedi il file di
  configurazione). Se specificato, i pezzi arrivano da quel file, ciascuno al proprio
  `ARRIVAL_STEP`.
- `scenario_path`: default `scenario_nominale.txt`.
- `oggetti_b2_path`: **nessun default** — stesso meccanismo di `oggetti_path` ma per
  pre-caricare **B2** invece di B1. Se omesso, ricade su `SIM_PEZZI_B2` (generatore
  casuale, default `0` = nessuno).

Esempi:
```bash
./programma                                                              # tutto default
./programma plant_config_valid.txt - scenario_difficile.txt              # solo scenario diverso
./programma plant_config_valid.txt oggetti_esempio.txt scenario_nominale.txt              # da file oggetti (B1)
./programma plant_config_valid.txt oggetti_esempio.txt scenario_nominale.txt oggetti_b2_esempio.txt  # da file oggetti (B1 + B2)
```

Ogni esecuzione fa **sempre**, in un solo comando:

1. Simula con la **Strategia 1** (priorità + ammissione buffer-aware) e ne stampa il
   resoconto completo (`statistiche.txt`, `simulazione.log`).
2. Rilancia automaticamente la stessa identica configurazione (stessi file, stesso seed)
   con la **Strategia 2** (FCFS), e stampa un confronto affiancato tra le due
   (`confronto_strategie.txt`, dettaglio completo in `statistiche_strategia2.txt`).

## Come lanciare i test unitari (Unity)

```bash
./test/run_tests.sh
```

Compila e lancia in sequenza tutti i file `test/test_*.c` (framework
[Unity](https://github.com/ThrowTheSwitch/Unity), vendorizzato in `test/unity/`),
riportando un riepilogo finale.

Equivalente con CMake/CTest (registra ogni file di test come test CTest indipendente):

```bash
cmake -S . -B build_cmake
cmake --build build_cmake
ctest --test-dir build_cmake --output-on-failure
```

121 test su 12 file (`test_object.c`, `test_buffer.c`, `test_statistiche.c`,
`test_controllore_strategia.c`, `test_isp_guasto.c`, `test_sensore_qualita.c`,
`test_isp_routing_materiale.c`, `test_arrivi_schedulati.c`, `test_parser.c`,
`test_machine.c`, `test_tolleranza_qualita.c`, `test_smistamento_generalizzato.c`).

## Formato dei file di ingresso

### File di configurazione impianto (es. `plant_config_valid.txt`)

Formato chiave=valore separato da virgole, una riga per elemento:

```
BUFFER,ID=B1,CAPACITY=60
INGRESSO,ID=B1
ISP,ID=ISP1,TEMPO=1,DIMX_TARGET=100,RAGGIO_TARGET=10
NASTRO,ID=N1,CAPACITY=2,VELOCITA=2
MACCHINA,ID=M,TEMPO=3
CONNECT,FROM=B1,TO=ISP1
MOTORE,NASTRO=N1,VELOCITA=5000,ACCEL=2000
DEVIATORE,ISP=ISP2,TEMPO_MIN_COMMUT=3
```

Più i parametri globali di simulazione in coda al file: `SIM_STEPS`, `SIM_PEZZI`
(usato solo se non passi un file oggetti da riga di comando), `SIM_PEZZI_B2`
(pre-caricamento di B2), `SOGLIA_BUFFER`, `GEN_TARGET_DIMENSIONX`, `GEN_TARGET_RAGGIO`.

### File di scenario (es. `scenario_nominale.txt`)

```
SCENARIO_NAME=nominale
LOAD_MULTIPLIER=1.0
FAULT_ENABLED=0
FAULT_ISP=ISP2
FAULT_TIME_ERROR=20
```

`FAULT_ISP` accetta anche più ISP separate da virgola senza spazi (es.
`FAULT_ISP=ISP1,ISP2`, vedi `scenario_doppio_guasto.txt`), fino a un massimo di 4
(vedi "Limitazioni note" sotto).

### File oggetti in ingresso (es. `oggetti_esempio.txt`)

Formato CSV con header:

```
ID,PRIORITY,TYPE,ARRIVAL_STEP,DIMENSIONX,RAGGIO
P001,8,A,0,100.0,10.0
P003,5,A,3,98.5,9.8
```

Ogni pezzo entra nella cella esattamente al proprio `ARRIVAL_STEP`, non tutto insieme
a step 0 come nel generatore casuale (`SIM_PEZZI`). Stesso formato per il file di
pre-caricamento di B2 (quarto argomento da riga di comando).

## Limitazioni note

Vincoli tecnici/numerici del programma (non scelte di design, solo cosa il codice
accetta o rifiuta concretamente):

- **Lunghezza di un identificativo (ID)**: 19 caratteri utili + terminatore
  (`IDLENGTH=20` in `object.h`), usata per ogni entità — buffer, ISP, macchina, nastro,
  motore, deviatore, sensore, oggetto. Un ID più lungo viene troncato in fase di lettura.
- **Priorità di un oggetto**: intera, da `PRIORITY_MIN=0` a `PRIORITY_MAX=10`
  (`object.h`). Un valore fuori range viene rifiutato da `object_create`.
- **ISP guaste contemporaneamente in un singolo scenario**: al massimo 4
  (`MAX_GUASTO_ISP` in `parser.h`). Oltre la quarta, le entrate in più vengono
  scartate con un avviso su stderr, senza bloccare le altre.
- **Lunghezza di una riga nei file di configurazione/scenario/oggetti**: 255 caratteri
  (`MAX_LINE_LEN=256` in `parser.c`). Una riga più lunga viene troncata da `fgets`.
- **Numero di buffer, ISP, macchine, nastri, oggetti**: nessun limite fisso — sono
  tutte liste concatenate allocate dinamicamente (`cell.c`, `registry.c`), limitate solo
  dalla memoria disponibile.

## Note per chi contribuisce al progetto

- I file compilati (`programma`, eseguibili di test) e i file generati da ogni run
  (`simulazione.log`, `statistiche.txt`, `confronto_strategie.txt`) **non vanno
  committati**: sono già esclusi da `.gitignore`. Se `git status` te li segnala come
  "untracked" o "modified" dopo una compilazione/esecuzione locale, è normale — non
  vanno aggiunti.
- Per compilare e testare rapidamente le modifiche prima di ogni commit:
  `./test/run_tests.sh` (oppure `cmake --build build_cmake && ctest --test-dir build_cmake`).
