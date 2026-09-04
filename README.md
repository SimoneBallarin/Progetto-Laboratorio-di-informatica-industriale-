# Simulazione e controllo di una cella meccatronica flessibile

Progetto Finale — Gruppo I MEJO

Simulazione a passi discreti di una cella composta da nastri trasportatori, buffer a
capacità limitata, una o più stazioni di lavorazione e stazioni di controllo qualità
(ISP), con smistamento finale a tre o quattro vie a seconda del layout (vedi sotto). Il
layout, gli obiettivi e le scelte progettuali sono descritti in dettaglio in
`progetto preliminare gruppo I MEJO.pdf`.

Flusso della cella (layout 1, default — vedi "Layout 2" più sotto per l'alternativa):

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
plant_config_layout1.txt        configurazione impianto — layout 1 (default, vedi PDF sez. 1.1)
plant_config_layout2.txt        configurazione impianto — layout 2 (vedi PDF sez. 1.1)
scenario_nominale_layout1.txt   scenario nominale (nessun guasto) — layout 1 (default)
scenario_nominale_layout2.txt   scenario nominale — layout 2
scenario_difficile_layout1.txt  scenario con carico maggiore + guasto sensore qualità — layout 1
scenario_difficile_layout2.txt  scenario con carico maggiore + guasto sensore qualità — layout 2
scenario_doppio_guasto_layout1.txt  scenario con guasto simultaneo su ISP1 e ISP2 — layout 1
oggetti_esempio.txt       file oggetti di esempio per il backlog di B1 (usato di default se presente)
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

- `config_path`: default `plant_config_layout1.txt`.
- `oggetti_path`: **nessun default fisso** — se il file `oggetti_esempio.txt` esiste e non
  hai passato un percorso esplicito, viene usato automaticamente (metodo primario). Se
  omesso e assente, o passato esplicitamente come `-`, gli arrivi in B1 sono generati
  casualmente (`SIM_PEZZI` pezzi, tutti a step 0, vedi il file di configurazione). Se
  specificato un file, i pezzi arrivano da quel file, ciascuno al proprio `ARRIVAL_STEP`.
- `scenario_path`: default `scenario_nominale_layout1.txt`.
- `oggetti_b2_path`: **nessun default** — stesso meccanismo di `oggetti_path` ma per
  pre-caricare **B2** invece di B1. Se omesso, ricade su `SIM_PEZZI_B2` (generatore
  casuale, default `0` = nessuno).

Esempi:
```bash
./programma                                                                          # tutto default (layout 1)
./programma plant_config_layout1.txt - scenario_difficile_layout1.txt                # solo scenario diverso
./programma plant_config_layout1.txt oggetti_esempio.txt scenario_nominale_layout1.txt              # da file oggetti (B1)
./programma plant_config_layout1.txt oggetti_esempio.txt scenario_nominale_layout1.txt oggetti_b2_esempio.txt  # da file oggetti (B1 + B2)
./programma plant_config_layout2.txt oggetti_esempio.txt scenario_nominale_layout2.txt -             # layout 2 (vedi sez. dedicata sotto)
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

### File di configurazione impianto (es. `plant_config_layout1.txt`)

Formato chiave=valore separato da virgole, una riga per elemento (esempio
semplificato solo per mostrare la sintassi — per l'elenco completo delle voci
realmente presenti in `plant_config_layout1.txt`/`plant_config_layout2.txt`, con
tutti i campi opzionali usati, vedi le sezioni "Layout 1"/"Layout 2" più sotto):

```
BUFFER,ID=B1,CAPACITY=60
INGRESSO,ID=B1
ISP,ID=ISP1,TEMPO=1,DIMX_TARGET=100,RAGGIO_TARGET=10,TOLLERANZA_CONFORME=5,TOLLERANZA_RIVALUTAZIONE=10,SMISTAMENTO=PASSACARTE
NASTRO,ID=N1,CAPACITY=2,VELOCITA=2
MACCHINA,ID=M,TEMPO=3,TOLLERANZA=0.02
BUFFER,ID=B2,CAPACITY=60
ISP,ID=ISP2,TEMPO=2,DIMX_TARGET=80,RAGGIO_TARGET=6,TOLLERANZA_CONFORME=5,TOLLERANZA_RIVALUTAZIONE=10,SMISTAMENTO=ENTRAMBI
CONNECT,FROM=B1,TO=ISP1
CONNECT,FROM=ISP1,TO=N1
CONNECT,FROM=N1,TO=M
CONNECT,FROM=M,TO=B2
CONNECT,FROM=B2,TO=ISP2
MOTORE,NASTRO=N1,VELOCITA=5000,ACCEL=2000
MOTORE,NASTRO=M,VELOCITA=5000,ACCEL=2000
```

Campi facoltativi mostrati sopra (se assenti, si ricade sui default):

- `TOLLERANZA_CONFORME`/`TOLLERANZA_RIVALUTAZIONE` (su `ISP`, percentuale di
  scostamento da `DIMX_TARGET`/`RAGGIO_TARGET`): sotto la prima soglia il pezzo è
  CONFORME, sotto la seconda RIVALUTAZIONE, oltre SCARTO. Default `5`/`10` se assenti;
  vanno indicate entrambe o nessuna delle due.
- `SMISTAMENTO` (su `ISP`, vedi sezione "Smistamento generalizzato delle ISP" più
  sotto): se assente, dedotto automaticamente (`AUTO`) dal numero di uscite collegate
  nei `CONNECT`. `ISP1` sopra usa `PASSACARTE` (un'unica uscita, verso `N1`), `ISP2`
  usa `ENTRAMBI` — nel file reale ha altre 4 uscite collegate (verso i buffer finali,
  omesse qui per brevità: vedi "Layout 1" più sotto per l'elenco completo) e i target
  `DIMX_TARGET`/`RAGGIO_TARGET` più bassi perché post-lavorazione, non grezzi come su
  ISP1.
- `TOLLERANZA` (su `MACCHINA`, frazione, es. `0.02` = 2%): rumore casuale massimo
  applicato a dimensione/raggio al rilascio del pezzo lavorato. Default `0.02` se
  assente.

`DEVIATORE,ISP=<id>,TEMPO_MIN_COMMUT=<n>` è un'altra riga valida (obbligatoria per
ogni ISP con più di un'uscita collegata — qui servirebbe per `ISP2`, che nel file
reale ha altre 4 uscite verso i buffer finali, omesse sopra per brevità): per
l'esempio completo vedi le sezioni "Layout 1"/"Layout 2" più sotto (es.
`DEVIATORE,ISP=ISP2,TEMPO_MIN_COMMUT=3` nel layout 1).

Più i parametri globali di simulazione in coda al file: `SIM_STEPS`, `SIM_PEZZI`
(usato solo se non passi un file oggetti da riga di comando), `SIM_PEZZI_B2`
(pre-caricamento di B2), `SOGLIA_BUFFER`, `GEN_TARGET_DIMENSIONX`, `GEN_TARGET_RAGGIO`,
`GEN_ERRORE_PCT` (rumore % applicato dal generatore casuale ai pezzi), `SCADENZA_STEP`
(facoltativo, default 40 — vedi "Limitazioni note" per gli altri default).

### File di scenario (es. `scenario_nominale_layout1.txt`)

```
SCENARIO_NAME=nominale
LOAD_MULTIPLIER=1.0
FAULT_ENABLED=0
FAULT_ISP=ISP2
FAULT_TIME_ERROR=20
```

`FAULT_ISP` accetta anche più ISP separate da virgola senza spazi (es.
`FAULT_ISP=ISP1,ISP2`, vedi `scenario_doppio_guasto_layout1.txt`), fino a un massimo di
4 (vedi "Limitazioni note" sotto).

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

## Smistamento generalizzato delle ISP (SMISTAMENTO)

Ogni ISP può instradare in uno dei seguenti modi (`tipo_smistamento_t` in
`Controllore.h`, campo facoltativo `SMISTAMENTO=...` nel file di configurazione,
impostabile anche via `controllore_impostaSmistamentoQualita`):

- `AUTO` (default se il campo è assente): dedotto automaticamente dal numero di uscite
  collegate all'ISP (1 uscita → `PASSACARTE`, 3 uscite → materiale **o** qualità a
  seconda del contesto, 4 uscite → materiale e qualità insieme).
- `PASSACARTE`: nessun instradamento, un'unica uscita (es. ISP1 nel layout 1: calcola
  comunque l'esito qualità/materiale per le statistiche, ma non lo usa per instradare).
- `MATERIALE`: instrada solo in base al materiale riconosciuto (2 uscite).
- `QUALITA`: instrada solo in base a CONFORME/RIVALUTAZIONE/SCARTO (3 uscite).
- `MATERIALE_E_QUALITA` (`ENTRAMBI`): instrada su entrambi i criteri insieme, come
  ISP2 nel layout 1 (4 uscite: conforme materiale A, rivalutazione, scarto, conforme
  materiale B).

Il layout 1 usa `PASSACARTE` su ISP1 ed `ENTRAMBI` su ISP2; il layout 2 (vedi sotto)
usa `MATERIALE` su ISP0 e `QUALITA` su ciascuna delle due ISP di linea.

## Layout 1 (default): smistamento combinato materiale+qualità

Voci del parser presenti in `plant_config_layout1.txt`:

- **1 `INGRESSO`** (`ID=B1`): dichiara `B1` come punto di ingresso della cella, l'unico
  a cui è agganciato il sensore di presenza (`S_Presenza`).
- **1 ISP prima della lavorazione** (`ISP1`, `TEMPO=1`, `DIMX_TARGET=100,
  RAGGIO_TARGET=10` — target grezzo, `TOLLERANZA_CONFORME=5, TOLLERANZA_RIVALUTAZIONE=10`
  espliciti, `SMISTAMENTO=PASSACARTE`): calcola comunque l'esito qualità/materiale per le
  statistiche, ma con un'unica uscita (verso `N1`) non lo usa per instradare.
- **1 `NASTRO`** (`N1`, `CAPACITY=2, VELOCITA=2`) tra ISP1 e M — l'unico nastro
  trasportatore dichiarato nel file (M → B2 è un `CONNECT` diretto, senza nastro
  dedicato).
- **1 `MACCHINA`** (`M`, `TEMPO=3, TOLLERANZA=0.02`): rumore casuale massimo applicato a
  dimensione/raggio al rilascio del pezzo lavorato.
- **1 ISP dopo la lavorazione** (`ISP2`, `TEMPO=2`, `DIMX_TARGET=80, RAGGIO_TARGET=6` —
  target post-lavorazione, ridotto rispetto a ISP1, `TOLLERANZA_CONFORME=5,
  TOLLERANZA_RIVALUTAZIONE=10` espliciti, `SMISTAMENTO=ENTRAMBI`): unica ISP che instrada
  SIA per materiale SIA per qualità insieme, sulle 4 uscite finali.
- **6 `BUFFER`** totali: `B1` (ingresso), `B2` (intermedio dopo M), più le 4 uscite finali
  `B_Alacciaio`, `B_riqualifica`, `B_TRASH`, `B_rame` (tutti `CAPACITY=60`).
- **9 `CONNECT`**: `B1→ISP1→N1→M→B2→ISP2`, poi `ISP2` verso le 4 uscite finali.
- **2 `MOTORE`**: uno su `N1` (nastro) e uno su `M` (macchina), entrambi
  `VELOCITA=5000, ACCEL=2000` — non uno solo.
- **1 solo `DEVIATORE`** (`ISP=ISP2, TEMPO_MIN_COMMUT=3`): ISP1 non ne ha uno proprio,
  essendo `PASSACARTE` a uscita singola.
- Parametri globali: `SIM_STEPS=100`, `SIM_PEZZI=35`, `SIM_PEZZI_B2=0`,
  `SOGLIA_BUFFER=0.8`, `GEN_TARGET_DIMENSIONX=100`, `GEN_TARGET_RAGGIO=10`,
  `GEN_ERRORE_PCT=2`, `SCADENZA_STEP=40`.

## Layout 2: smistamento per materiale prima della lavorazione

A differenza del layout 1 (smistamento combinato materiale+qualità in un'unica ISP a
valle), nel layout 2 (`plant_config_layout2.txt`) lo smistamento per **materiale**
avviene subito dopo l'ingresso, **prima** della lavorazione: da lì in poi ogni
materiale segue una linea dedicata, con una propria macchina e una propria ISP finale
che controlla solo la qualità.

```
B1 -> ISP0 (SMISTAMENTO=MATERIALE) -+-> M1 -> B2 -> ISP_B2 (SMISTAMENTO=QUALITA) -+-> B_Alacciaio (conforme)
                                     |                                             +-> B_riqualifica (condiviso)
                                     |                                             +-> B_TRASH (condiviso)
                                     |
                                     +-> M2 -> B3 -> ISP_B3 (SMISTAMENTO=QUALITA) -+-> B_rame (conforme)
                                                                                    +-> B_riqualifica (condiviso)
                                                                                    +-> B_TRASH (condiviso)
```

Ogni ISP di linea (ISP_B2/ISP_B3) ha quindi **tre** uscite (conforme/rivalutazione/
scarto), non quattro come ISP2 nel layout 1 — da qui la formulazione "smistamento
finale a tre/quattro vie" a inizio README. I buffer `B_riqualifica` e `B_TRASH` sono
condivisi tra le due linee (più `CONNECT` verso lo stesso buffer).

Voci del parser presenti in `plant_config_layout2.txt` (per differenza rispetto al
layout 1):

- **1 `INGRESSO`** (`ID=B1`): stesso ruolo del layout 1 (unico punto agganciato al
  sensore di presenza).
- **1 ISP prima della lavorazione** (`ISP0`, `TEMPO=1`, `SMISTAMENTO=MATERIALE`,
  `DIMX_TARGET=100, RAGGIO_TARGET=10`): giudica il pezzo grezzo così com'è arrivato in
  B1, non dopo un'asportazione di materiale che non è ancora avvenuta.
- **2 MACCHINA** (`M1`, `M2`, entrambe `TEMPO=3,TOLLERANZA=0.02`), una per linea, invece
  dell'unica `M` del layout 1.
- **2 ISP dopo la lavorazione** (`ISP_B2`, `ISP_B3`, entrambe `TEMPO=2,
  SMISTAMENTO=QUALITA`, `DIMX_TARGET=80, RAGGIO_TARGET=6` — target post-lavorazione, non
  quello grezzo di ISP0), una per linea.
- **Nessun `NASTRO`**: a differenza del layout 1 (che ha `N1` tra ISP1 e M), qui ISP0 è
  collegata direttamente a M1/M2 via `CONNECT`, senza un nastro trasportatore dedicato.
  Di conseguenza i `MOTORE` (`MOTORE,NASTRO=M1,...` / `MOTORE,NASTRO=M2,...`) sono
  attuatori collegati direttamente alle macchine, non a un nastro come nel layout 1.
- **3 `DEVIATORE`** (uno per ogni ISP con più uscite: `ISP0`, `ISP_B2`, `ISP_B3`), contro
  l'unico `DEVIATORE,ISP=ISP2` del layout 1.
- **`TOLLERANZA_CONFORME`/`TOLLERANZA_RIVALUTAZIONE` non impostate** su nessuna delle tre
  ISP (a differenza di ISP1/ISP2 nel layout 1, che le impostano esplicitamente a 5/10):
  qui si ricade sempre sul default 5/10 di `S_Qualita.c`.
- **7 `BUFFER`** totali: `B1` (ingresso), `B2`/`B3` (intermedi dopo M1/M2), più le 4
  uscite finali `B_Alacciaio`, `B_riqualifica`, `B_TRASH`, `B_rame` (tutti
  `CAPACITY=60`) — uno in più del layout 1, perché ogni linea ha il proprio buffer
  intermedio (`B2`/`B3`) invece dell'unico `B2` condiviso.
- **13 `CONNECT`**: `B1→ISP0→{M1,M2}`, poi `M1→B2→ISP_B2→` le sue 3 uscite e
  `M2→B3→ISP_B3→` le sue 3 uscite (`B_riqualifica`/`B_TRASH` compaiono due volte, una
  per linea) — contro i 9 del layout 1.
- Parametri globali di simulazione (`SIM_STEPS`, `SIM_PEZZI`, `SIM_PEZZI_B2`,
  `SOGLIA_BUFFER`, `GEN_TARGET_DIMENSIONX`, `GEN_TARGET_RAGGIO`, `GEN_ERRORE_PCT`,
  `SCADENZA_STEP`): stessi nomi e stesso significato del layout 1, non ripetuti qui.

Per lanciare il layout 2: `./programma plant_config_layout2.txt - scenario_nominale_layout2.txt`
(o con un file oggetti esplicito, vedi esempi sopra).

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
