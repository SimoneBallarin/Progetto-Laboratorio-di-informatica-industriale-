# Simulazione e controllo di una cella meccatronica flessibile

Progetto Finale — Gruppo I MEJO

Simulazione a passi discreti di una cella composta da nastri trasportatori, buffer a
capacità limitata, una stazione di lavorazione (M), due stazioni di controllo qualità
(ISP1/ISP2) e uno smistamento finale a tre/quattro vie. Il layout, gli obiettivi e le
scelte progettuali sono descritti in dettaglio in `progetto preliminare gruppo I MEJO.pdf`.

## Struttura del progetto

```
app/main.c              punto d'ingresso della simulazione
lib/                     un modulo per cartella (Buffer, Cella, Controllore, ISP, ecc.),
                         ciascuno con la coppia include/.h + src/.c (o scr/, per refusi storici)
lib/parser/              file di configurazione impianto e scenari (.txt)
test/                    test unitari Unity (vedi sotto)
build/                   programmi di prova ad-hoc (demo.c, programma_prova.c) —
                         NON fanno parte della build di app/main.exe
build.sh                 script di build, stessi flag di .vscode/tasks.json
CMakeLists.txt           build alternativa via CMake (vedi sotto)
```

## Come compilare

Due modi equivalenti, a scelta:

```bash
./build.sh                 # script diretto (gcc), produce app/main.exe
```

oppure con CMake:

```bash
cmake -S . -B build_cmake
cmake --build build_cmake
# eseguibile in build_cmake/main
```

Da VSCode: `Ctrl+Shift+B` lancia il task di default ("build main", equivalente a `build.sh`).

## Come eseguire

```bash
./app/main.exe [config_path] [oggetti_path] [scenario_path] [oggetti_b2_path]
```

Tutti e quattro gli argomenti sono opzionali (sez. 8/10 della traccia: "poter essere
eseguito da linea di comando indicando almeno i file di configurazione, oggetti e
scenario"):

- `config_path`: default `lib/parser/plant_config_valid.txt`.
- `oggetti_path`: **nessun default** — se omesso (o passato come `-`), gli arrivi in B1
  sono generati casualmente come sempre (`SIM_PEZZI` pezzi, tutti a step 0). Se
  specificato, i pezzi arrivano da quel file, ciascuno al proprio `ARRIVAL_STEP` (vedi sotto).
- `scenario_path`: default `lib/parser/scenario_nominale.txt`.
- `oggetti_b2_path`: **nessun default** — stesso meccanismo di `oggetti_path` ma per
  pre-caricare **B2** invece di B1 (vedi sezione dedicata sotto). Se omesso, ricade su
  `SIM_PEZZI_B2` (generatore casuale, default `0` = nessuno).

Esempi:
```bash
./app/main.exe                                                                  # tutto default (comportamento storico)
./app/main.exe lib/parser/plant_config_valid.txt - lib/parser/scenario_difficile.txt   # solo scenario diverso
./app/main.exe lib/parser/plant_config_valid.txt lib/parser/oggetti_esempio.txt lib/parser/scenario_nominale.txt  # da file oggetti (B1)
./app/main.exe lib/parser/plant_config_valid.txt lib/parser/oggetti_esempio.txt lib/parser/scenario_nominale.txt lib/parser/oggetti_b2_esempio.txt  # da file oggetti (B1 + B2)
```

Ogni esecuzione fa **sempre**, in un solo comando:

1. Simula con la **Strategia 1** (priorità + ammissione buffer-aware) e ne stampa il
   resoconto completo (`statistiche.txt`, `simulazione.log`).
2. Rilancia automaticamente la stessa identica configurazione (stessi file, stesso seed)
   con la **Strategia 2** (FCFS), e stampa un confronto affiancato tra le due
   (`confronto_strategie.txt`, dettaglio completo in `statistiche_strategia2.txt`).

Non serve nessun flag aggiuntivo: si scelgono i file, si lancia, ed escono sia il
resoconto della propria simulazione sia il confronto con l'altra strategia.

## File oggetti in ingresso

`lib/parser/oggetti_esempio.txt` (formato CSV, header opzionale):

```
ID,PRIORITY,TYPE,ARRIVAL_STEP,DIMENSIONX,RAGGIO
P001,8,A,0,100.0,10.0
P003,5,A,3,98.5,9.8
```

A differenza del generatore casuale storico (tutti i pezzi a step 0), ogni pezzo letto da
file entra nella cella esattamente al proprio `ARRIVAL_STEP` — arrivi realmente
distribuiti nel tempo, non un unico burst iniziale. Implementato con un nuovo meccanismo
generico nel Controllore (`controllore_schedulaArrivo`/`controllore_getArriviSchedulatiCount`,
vedi `Controllore.h`): l'oggetto resta in una coda interna finché la simulazione non
raggiunge il suo step, poi entra automaticamente (ritentando ai passi successivi se il
buffer è pieno esattamente in quel momento — non viene mai perso).

`parser_caricaOggetti` esisteva già in `parser.c` ma non era mai stata collegata al main,
e conteneva due bug corretti in questo stesso intervento: inseriva gli oggetti con
`buffer_insertObject` diretto invece che tramite il controllore (bypassando il
`SensoreBuffer`, letture di occupazione permanentemente disallineate), e ignorava
completamente il campo `ARRIVAL_STEP` (li inseriva tutti subito, indipendentemente da
quanto scritto nel file).

**Attenzione a `SIM_PEZZI`** (in `plant_config_valid.txt`): è usato **solo** quando NON
passi un file oggetti da riga di comando (ramo del generatore casuale). Con un file
oggetti esplicito, `SIM_PEZZI` viene ignorato — un avviso a console/log lo segnala
esplicitamente quando succede, per non lasciarlo passare inosservato.

## Pre-caricare pezzi anche in B2

Oltre al backlog di B1 (generatore casuale o file oggetti), è possibile far partire la
simulazione con un numero scelto di pezzi già presenti in **B2**, indipendentemente da
come è stato popolato B1. Due modi, mutuamente esclusivi come per B1:

**1. Generatore casuale** (`SIM_PEZZI_B2` nel file di configurazione impianto, default `0`):

```
SIM_PEZZI_B2=10
```

Stessa logica di generazione già usata per B1 (stesso target/errore percentuale).

**2. File oggetti dedicato** (quarto argomento da riga di comando):

```bash
./app/main.exe [config_path] [oggetti_path] [scenario_path] [oggetti_b2_path]
```

Stesso formato CSV del file oggetti per B1 (`ID,PRIORITY,TYPE,ARRIVAL_STEP,DIMENSIONX,RAGGIO`),
vedi `lib/parser/oggetti_b2_esempio.txt`. Se fornito, ha priorità su `SIM_PEZZI_B2` (che
viene ignorato, con avviso esplicito a console/log). Il quarto argomento è stato aggiunto
in coda, non subito dopo il secondo, per non spostare la posizione degli argomenti già
esistenti — chi lanciava già `./main.exe config oggetti scenario` continua a funzionare
identico.

`parser_caricaOggetti` era già generica (accetta il buffer di destinazione come
parametro): il file per B2 riusa la stessa funzione già scritta per B1, senza duplicare
logica di parsing.

## Commenti nei file di configurazione

Tutti i file letti dal parser (impianto, scenario, oggetti) supportano righe di commento:
qualunque riga il cui primo carattere non-spazio è `#` viene ignorata, esattamente come una
riga vuota (nessun avviso "record sconosciuto"). Implementato con un'unica modifica in
`trim_line` (funzione condivisa da tutti i cicli di parsing in `parser.c`), invece di
duplicare il controllo in ogni singolo ciclo.

## Come lanciare i test unitari (Unity)

```bash
./test/run_tests.sh
```

Compila e lancia in sequenza tutti i file `test/test_*.c` (framework
[Unity](https://github.com/ThrowTheSwitch/Unity), vendorizzato in `test/unity/`),
riportando un riepilogo finale. Da VSCode: `Ctrl+Shift+P` → *Tasks: Run Test Task*.

71 test su 9 file (`test_object.c`, `test_buffer.c`, `test_statistiche.c`,
`test_controllore_strategia.c`, `test_isp_guasto.c`, `test_sensore_qualita.c`,
`test_isp_routing_materiale.c`, `test_arrivi_schedulati.c`, `test_parser.c`): validazione
parametri, criteri di inserimento/prelievo per priorità vs FIFO, la scomposizione tempo
SISTEMA/PROCESSO, un test end-to-end che verifica che le due strategie di controllo
producano davvero ordini di smaltimento diversi sugli stessi dati in ingresso, la logica
"guasto = stazione indisponibile", la classificazione materiale A/B/non_classificato, gli
arrivi schedulati da file, e il parsing di configurazione/scenario/oggetti (validi,
malformati, ID duplicati, file inesistenti).

**Diversi bug reali sono stati trovati e corretti scrivendo questi test** (vedi anche le
sezioni dedicate più sotto per i dettagli):

- `object_create` (lib/Oggetto/src/object.c) non rifiutava un ID vuoto (`""`), a
  differenza di `buffer_create` che lo fa esplicitamente — corretto per coerenza con lo
  stesso pattern già usato nel resto del progetto (`strlen(ID) == 0 || strlen(ID) >= IDLENGTH`).
- `get_Material` (S_Qualita.c) poteva instradare un pezzo nel buffer del materiale
  **opposto** a quello dichiarato, per un bug nel confronto incrociato tra densità.
- `parser_caricaOggetti` bypassava il sensore di buffer e ignorava `ARRIVAL_STEP`.
- Un memory leak nel test stesso (non nel codice di produzione), causato da una
  caratteristica del progetto da tenere a mente: `cell_getBuffer` risolve sempre passando
  dal registro **globale** (`registry_getBuffer`, vedi `registry.h`: "una sola cella per
  esecuzione"). Dopo una `registry_clear()`, anche una cella ancora viva diventa
  irraggiungibile tramite `cell_getBuffer` — bisogna liberare gli oggetti PRIMA di
  svuotare il registro, non dopo. Lo stesso accorgimento è già applicato in
  `esegui_simulazione` (`app/main.c`) per poter eseguire le due strategie in sequenza
  nello stesso processo, e serve svuotarlo anche tra un test e l'altro nello stesso
  eseguibile Unity (vedi `tearDown` in `test/test_parser.c`).

## Valgrind

Verifica di memory leak/errori sul binario reale (non solo sui test):

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./app/main.exe lib/parser/scenario_nominale.txt
```

**Risultato attuale**: `app/main.exe` risulta pulito su entrambi gli scenari — `0 errors
from 0 contexts`, tutti gli allocchi bilanciati da altrettante free (588 allocs / 588
frees su nominale, 586/586 su difficile). Verificato anche con
AddressSanitizer+UndefinedBehaviorSanitizer (`gcc -fsanitize=address,undefined`) durante
lo sviluppo delle Strategie 1/2 e del confronto automatico: nessun leak, nessun comportamento
indefinito rilevato nemmeno eseguendo due simulazioni complete nello stesso processo.

**Un leak reale è stato trovato** facendo girare Valgrind su `build/programma_prova.c`
(uno dei programmi di prova ad-hoc, non collegato alla build principale):

```
==1725== HEAP SUMMARY:
==1725==     in use at exit: 968 bytes in 24 blocks
==1725== 112 (32 direct, 80 indirect) bytes in 1 blocks are definitely lost in loss record 24 of 24
==1725==    at 0x4846828: malloc (...)
==1725==    by 0x10B58B: cell_create (cell.c:74)
==1725==    by 0x1092D7: main (programma_prova.c:27)
==1725== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
```

**Causa**: il programma usa una macro `CHECK_OK` che, al primo controllo fallito, stampa
un messaggio e fa `return 1` immediatamente — saltando la sezione di pulizia finale
(`cell_destroy(cell)`) che si trova più avanti nel file. Il controllo che falliva era
`CHECK_OK(deviatore_init(&dv, "DEV_QC") == 0, "deviatore_init")`: `deviatore_init`
restituisce `OP_SUCCESS` in caso di successo, che nel progetto vale `1` (vedi
`errors.h`), non `0` — il programma di prova confrontava il valore di ritorno con la
costante sbagliata. Investigato a fondo con gdb (vedi sezione sotto) per confermare la
causa esatta prima di correggere. **Corretto** in `build/programma_prova.c` (confronto
ora contro `OP_SUCCESS`); rilanciando Valgrind dopo la correzione: `0 errors`, tutti i
31 allocchi liberati.

## gdb

Sessione usata per investigare a fondo il leak sopra, prima di correggerlo (così da
confermare la causa esatta — un valore di ritorno sbagliato — invece di ipotizzarla
dalla sola lettura del codice):

```bash
gdb ./programma_prova   # build con il bug non ancora corretto
(gdb) break deviatore_init
(gdb) run
(gdb) finish             # esegue fino al ritorno dalla funzione, mostra il valore restituito
(gdb) print $rax         # registro di ritorno su x86-64: conferma il valore intero restituito
(gdb) continue
```

Output della sessione:

```
Breakpoint 1, deviatore_init (d=0x7fffffffe960, ID=0x55555555d2a4 "DEV_QC") at lib/Attuatori/Servo/scr/Deviatore.c:7
7           if (d == NULL || ID == NULL) return ERR_NULL_PTR;
Run till exit from #0  deviatore_init (...)
0x00005555555556d0 in main () at programma_prova.c:58
58          CHECK_OK(deviatore_init(&dv, "DEV_QC") == 0, "deviatore_init");
Value returned is $1 = 1
$2 = 1
ERRORE in "deviatore_init" (linea 58)
```

`Value returned is $1 = 1` conferma che `deviatore_init` aveva effettivamente
**successo** (`OP_SUCCESS == 1`): il bug non era nella funzione ma nel confronto
`== 0` fatto dal chiamante. Questo ha permesso di correggere il punto esatto (una
costante sbagliata) invece di modificare `deviatore_init` stesso, che era corretto.

## Strategie di controllo

- **Strategia 1** (default, `STRATEGIA_PRIORITA_BUFFER_AWARE`): ammissione ordinata per
  priorità, con ritardo se il buffer a valle di una macchina supera la soglia
  configurata (`SOGLIA_BUFFER` nel file di configurazione impianto).
- **Strategia 2** (`STRATEGIA_FCFS`): First-Come-First-Served puro, nessun controllo su
  priorità o occupazione dei buffer.

Impostabile a runtime con `controllore_impostaStrategia` (vedi `Controllore.h`). Ogni
esecuzione di `app/main.exe` le confronta entrambe automaticamente sullo stesso scenario
e con lo stesso seed (`rand()`, vedi `SEED_ESECUZIONE` in `main.c`), così le differenze
nei risultati riflettono solo la strategia, non un caso diverso di dati in ingresso.

## Statistiche: tempo SISTEMA vs tempo PROCESSO

Per evitare ambiguità, ogni metrica di tempo/scadenza è sempre riportata in due varianti
distinte (vedi `statistiche.h`):

- **SISTEMA** = tempo totale, dalla coda di ingresso all'uscita (coda + pipeline).
- **PROCESSO** = solo la pipeline vera e propria (ISP1→N1→M→B2→ISP2), **esclusa**
  l'attesa in coda su B1.

La scadenza configurata (40 passi, in `app/main.c`) viene verificata contro entrambe,
separatamente: un oggetto può risultare "entro scadenza" secondo una definizione e non
secondo l'altra (tipicamente: quasi sempre entro scadenza sul PROCESSO, spesso fuori
scadenza sul SISTEMA — la coda è il vero collo di bottiglia, non la pipeline).

## Bug corretto: instradamento nel materiale opposto (get_Material)

`get_Material` (S_Qualita.c) decide, per i pezzi CONFORMI su un'ISP con 4 uscite (es.
ISP2: `B_Alacciaio`/`B_riqualifica`/`B_TRASH`/`B_rame`), in quale delle due uscite
"conforme" instradare il pezzo, in base al materiale (densità 8.96 = uno, 7.85 = l'altro).

**Bug trovato e corretto**: la versione precedente calcolava la massa di riferimento
(`materiale`/`e`) **una sola volta**, in base al tipo dichiarato dell'oggetto (`'A'` o
`'B'`), poi la riusava per verificare **entrambi** i confronti (prima quello coerente col
tipo, poi — in caso di fallimento — quello con la densità dell'*altro* materiale, ma sulla
stessa massa di riferimento sbagliata). Risultato: un pezzo dichiarato `'A'` con dimensioni
appena fuori dalla tolleranza di `'A'` poteva risultare classificato `'B'` per puro caso
numerico, e finire instradato **nel buffer del materiale opposto** a quello dichiarato.
Riprodotto e confermato con un oggetto costruito ad hoc (`type='A'`, dimensioni scelte
apposta) prima della correzione.

Approfondendo, il problema è più radicale di un semplice "densità sbagliata": la
tolleranza è calcolata come **percentuale** della massa di riferimento, quindi la densità
si **semplifica matematicamente** in ogni confronto (il rapporto si riduce sempre a
`volume_oggetto` entro ±12% di `volume_target`, indipendentemente da quale densità si usi)
— un vero controllo incrociato, anche ricalcolato correttamente, avrebbe dato **sempre lo
stesso esito** del controllo primario, quindi non avrebbe comunque avuto alcun valore
discriminante reale. La correzione elimina perciò il confronto incrociato: un solo
controllo, coerente con il tipo dichiarato; se le dimensioni non rientrano nella tolleranza
per quel tipo, il pezzo è `non_classificato` (mai un tentativo con l'altro materiale).

Effetto misurabile su una run reale (scenario nominale): `non_classificato` passa da 4 a
12 su ISP2 — gli 8 pezzi in più erano quelli prima misclassificati nel materiale sbagliato,
ora correttamente segnalati come non classificabili invece che instradati a caso. Test di
regressione dedicato in `test/test_sensore_qualita.c`
(`test_get_Material_non_instrada_piu_nel_materiale_opposto`).

**Decisione presa col gruppo su dove va un pezzo "non classificato"**: non più instradato
in base al tipo dichiarato (ripiego rimosso), ma verso `RIVALUTAZIONE`/`B_riqualifica`
(indice 1, come un esito di qualità incerto) — un pezzo le cui dimensioni reali non
corrispondono al target per il proprio materiale merita una verifica ulteriore, non deve
proseguire come se fosse regolare. Vedi `processISP` in `Controllore.c`. Test dedicati in
`test/test_isp_routing_materiale.c`.

Da segnalare: questa classificazione a 4 vie (materiale A/B) **non è descritta da nessuna
parte nel documento preliminare** (che parla solo di conforme/rivalutazione/scarto a 3
vie) — è una funzionalità aggiunta durante lo sviluppo, andrebbe documentata anche lì.

## Classificazione materiale: contatore "non classificato"

`get_Material` (S_Qualita.c) confronta le dimensioni dell'oggetto con la tolleranza
attesa per i due materiali noti (densità A/B); se un pezzo non rientra in nessuna delle
due, prima restava semplicemente invisibile (nessun contatore lo registrava). Aggiunto
`non_classificato` (getter `controllore_getMaterialeNonClassificato`), riportato in
`statistiche.txt` accanto a materiale A/B — su una run reale (scenario nominale) emergono
subito 4 pezzi su 31 non classificati su ISP2, prima persi silenziosamente.

Nello stesso intervento, corretto un uso di variabili non inizializzate in `get_Material`:
se `object->type` non è né `'A'` né `'B'`, le variabili di confronto non venivano mai
assegnate ma venivano comunque lette nel confronto successivo (comportamento indefinito).
Oggi non è raggiungibile (il generatore in `main.c` produce solo 'A'/'B'), ma lo sarebbe
diventato appena collegato un file oggetti in ingresso con tipi arbitrari. Corretto
trattando esplicitamente un tipo sconosciuto come "non classificato". Test dedicati in
`test/test_sensore_qualita.c`.

## Guasto sensore qualità = stazione indisponibile

Quando il sensore di qualità di un'ISP è in guasto (`FAULT_ENABLED=1` nello scenario),
la stazione si comporta come **fisicamente ferma**, non solo come "sensore che legge
male": non accetta nuovi pezzi in ingresso (`genericIsAvailable`, `ENTITY_ISP`) **e**
trattiene il pezzo che sta già controllando anche a tempo di controllo scaduto
(`processISP`), finché il guasto non rientra. Scelta esplicita del gruppo, motivata da:

- la traccia elenca esplicitamente "*indisponibilità temporanea*" tra i comportamenti
  non ideali accettati per un sensore (§5), e "*una stazione*" tra le entità su cui può
  ricadere un guasto o degrado (stesso paragrafo);
- rende il guasto **osservabile indipendentemente dal numero di uscite dell'ISP**: prima,
  un guasto su un'ISP a singola uscita (es. ISP1, che instrada sempre là comunque) non
  aveva alcun effetto visibile sul flusso, solo sui contatori. Ora blocca la stazione a
  prescindere da quante uscite ha.

Contatore dedicato `step_bloccata_per_guasto` (per ISP, via
`controllore_getStepBloccoGuasto`), loggato passo per passo (`WARNING`) e riportato in
`statistiche.txt`/`statistiche_strategia2.txt`. Il contatore storico `anomalie_rilevate`
(letture sbagliate prodotte durante il guasto) resta a 0 con questa logica: durante il
guasto la ISP non produce più letture affatto, quindi non ne produce di sbagliate — è
`step_bloccata_per_guasto` la metrica da guardare per misurare l'impatto del guasto.

`guasto_isp_id` nello scenario è generico (funziona su qualunque ISP con un sensore di
qualità agganciato) e supporta **più ISP contemporaneamente**: `FAULT_ISP=ISP1,ISP2`
(elenco separato da virgole, senza spazi) applica lo stesso guasto a entrambe nello
stesso scenario — vedi `lib/parser/scenario_doppio_guasto.txt`. Per provarlo su una sola
ISP diversa da quella di default basta cambiare `FAULT_ISP=ISP1` nel file di scenario,
senza ricompilare.

## File di scenario

`lib/parser/scenario_nominale.txt` e `scenario_difficile.txt` (carico maggiore + guasto
del sensore di qualità su ISP2). Il percorso si passa da riga di comando, senza
ricompilare (`./app/main.exe lib/parser/scenario_difficile.txt`).
