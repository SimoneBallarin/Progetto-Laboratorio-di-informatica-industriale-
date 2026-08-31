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
                         .h e .c direttamente nella cartella del modulo (nessuna
                         sottocartella include/src)
lib/parser/              file di configurazione impianto e scenari (.txt)
test/                    test unitari Unity (vedi sotto)
build/                   programmi di prova ad-hoc (demo.c, programma_prova.c) —
                         NON fanno parte della build di app/main.exe
build.sh                 script di build, stessi flag di .vscode/tasks.json
CMakeLists.txt           build alternativa via CMake (vedi sotto)
```

Un modulo = una cartella autonoma sotto `lib/`, indipendente dagli altri (solo `errors.h`
e `object.h` sono condivisi trasversalmente): scelta pensata per un gruppo di più persone
che lavorano su moduli diversi in parallelo senza conflitti di merge sugli stessi file.

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

- `config_path`: default `lib/parser/plant_config_layout1.txt`.
- `oggetti_path`: **nessun default** — se omesso (o passato come `-`), gli arrivi in B1
  sono generati casualmente come sempre (`SIM_PEZZI` pezzi, tutti a step 0). Se
  specificato, i pezzi arrivano da quel file, ciascuno al proprio `ARRIVAL_STEP` (vedi sotto).
- `scenario_path`: default `lib/parser/scenario_nominale_layout1.txt`.
- `oggetti_b2_path`: **nessun default** — stesso meccanismo di `oggetti_path` ma per
  pre-caricare **B2** invece di B1 (vedi sezione dedicata sotto). Se omesso, ricade su
  `SIM_PEZZI_B2` (generatore casuale, default `0` = nessuno).

Esempi:
```bash
./app/main.exe                                                                  # tutto default (comportamento storico, layout 1)
./app/main.exe lib/parser/plant_config_layout1.txt - lib/parser/scenario_difficile_layout1.txt   # solo scenario diverso
./app/main.exe lib/parser/plant_config_layout1.txt lib/parser/oggetti_esempio.txt lib/parser/scenario_nominale_layout1.txt  # da file oggetti (B1)
./app/main.exe lib/parser/plant_config_layout1.txt lib/parser/oggetti_esempio.txt lib/parser/scenario_nominale_layout1.txt lib/parser/oggetti_b2_esempio.txt  # da file oggetti (B1 + B2)
./app/main.exe lib/parser/plant_config_layout2.txt lib/parser/oggetti_esempio.txt lib/parser/scenario_nominale_layout2.txt -  # layout 2 (vedi sez. dedicata sotto)
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

**Attenzione a `SIM_PEZZI`** (in `plant_config_layout1.txt`): è usato **solo** quando NON
passi un file oggetti da riga di comando (ramo del generatore casuale). Con un file
oggetti esplicito, `SIM_PEZZI` viene ignorato — un avviso a console/log lo segnala
esplicitamente quando succede, per non lasciarlo passare inosservato.

## Bug corretto: ordine invertito tra arrivi con lo stesso ARRIVAL_STEP

La coda interna degli arrivi schedulati (`controllore_schedulaArrivo`) inseriva in
**testa** invece che in **coda**: per due o più righe del file oggetti con lo stesso
`ARRIVAL_STEP` (caso comune, es. più pezzi che arrivano tutti a step 0), l'ultimo
schedulato veniva ammesso in B1 **per primo**, invertendo silenziosamente l'ordine
scritto nel file. Irrilevante per la Strategia 1 (la priorità decide comunque chi esce
prima), ma decisivo per la Strategia 2/FCFS, dove l'ordine di ammissione in B1 è l'unica
cosa che conta. Riprodotto con `P001`/`P002` di `oggetti_esempio.txt` (stesso
`ARRIVAL_STEP=0`): con FCFS usciva prima `P002`, nonostante `P001` fosse elencato per
primo nel file. Corretto facendo inserire la coda in ordine FIFO. Test di regressione
dedicato in `test/test_arrivi_schedulati.c`.

## Bug corretto: riepilogo statistiche/log stampato due volte

`esegui_simulazione` (`app/main.c`) viene chiamata due volte, una per strategia (sez.
"Come eseguire" sopra): la seconda run (Strategia 2/FCFS) è pensata per girare "in
silenzio", con `stampa_stato_console=false`, lasciando i dettagli solo nei rispettivi file
(`statistiche_strategia2.txt`, `simulazione_strategia2.log`) — così la console mostra
solo il resoconto della prima run seguito dalla tabella di confronto finale, senza due
resoconti completi affollati uno sopra l'altro.

**Bug trovato e corretto**: `statistiche_stampa` e `log_stampaRiepilogo` stampavano
**sempre** il proprio riepilogo su stdout, ignorando `stampa_stato_console` — il flag
veniva rispettato solo per lo stato iniziale/finale della cella (`controllore_print`), non
per queste due chiamate. Risultato: `=== STATISTICHE ===` e `=== LOG EVENTI (riepilogo)
===` comparivano due volte in sequenza sulla stessa console, una per ciascuna strategia,
anche quando la seconda run avrebbe dovuto restare silenziosa.

**Corretto** aggiungendo un parametro `anche_su_stdout` a entrambe le funzioni (stesso
pattern già usato da `log_create`, vedi `log.h`): `esegui_simulazione` ora passa
`stampa_stato_console` a entrambe, così solo la prima run stampa a schermo. La scrittura
su file resta **sempre** attiva indipendentemente da `anche_su_stdout` — anzi, come
effetto collaterale positivo, `log_stampaRiepilogo` ora scrive il riepilogo anche nel file
di log (prima andava SOLO su stdout, quindi per la seconda run non era salvato da nessuna
parte, nemmeno nel file: un dato silenziosamente perso, non solo duplicato). Test di
regressione dedicati in `test/test_statistiche.c`
(`test_statistiche_stampa_scrive_sempre_su_file`,
`test_statistiche_stampa_con_anche_su_stdout_scrive_anche_su_stdout`).

## Pre-caricare pezzi anche in B2

Oltre al backlog di B1 (generatore casuale o file oggetti), è possibile far partire la
simulazione con un numero scelto di pezzi già presenti in **B2**, indipendentemente da
come è stato popolato B1. Due modi, mutuamente esclusivi come per B1:

**1. Generatore casuale** (`SIM_PEZZI_B2` nel file di configurazione impianto, default `0`):

```
SIM_PEZZI_B2=10
```

Le dimensioni vengono generate intorno al target **POST-lavorazione** (target grezzo di
ingresso meno la riduzione fissa di M, vedi `MACHINE_DLAVORATO`/`MACHINE_RLAVORATO` in
`machine.h`), non intorno al target grezzo usato per B1: un pezzo pre-caricato in B2
rappresenta un pezzo che ha già virtualmente attraversato ISP1/N1/M prima dell'inizio
della simulazione, quindi deve avere le dimensioni che avrebbe DOPO la lavorazione di M,
non prima.

> **Bug corretto**: la versione precedente centrava la generazione sul target grezzo
> (`GEN_TARGET_DIMENSIONX`/`GEN_TARGET_RAGGIO`, es. 100/10), identico a quello usato per
> B1 — ma ISP2 (a valle di B2) si aspetta dimensioni già ridotte da M (`DIMX_TARGET=80`,
> `RAGGIO_TARGET=6` nel `plant_config_layout1.txt` di riferimento, esattamente
> target grezzo meno la riduzione di M). Con un target sbagliato di +20/+4 unità, ogni
> pezzo pre-caricato risultava fuori tolleranza per costruzione e ISP2 lo classificava
> **sempre** come SCARTO, indipendentemente dal rumore casuale — vanificando lo scopo
> della funzionalità (osservare il comportamento normale della Strategia 1 buffer-aware a
> valle di M con B2 già parzialmente pieno).

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

121 test su 12 file (`test_object.c`, `test_buffer.c`, `test_statistiche.c`,
`test_controllore_strategia.c`, `test_isp_guasto.c`, `test_sensore_qualita.c`,
`test_isp_routing_materiale.c`, `test_arrivi_schedulati.c`, `test_parser.c`,
`test_machine.c`, `test_tolleranza_qualita.c`, `test_smistamento_generalizzato.c`): validazione
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
- `machine_tryRelease` (lib/Macchina/machine.c) applicava un rumore casuale di
  lavorazione che, nel 20% dei casi, usciva di **5 volte** dalla tolleranza dichiarata
  invece di restarci dentro — bug rimasto invisibile perché il modulo non aveva nessun
  test (vedi `test_machine.c`, aggiunto insieme alla correzione:
  `test_tryRelease_rumore_resta_entro_la_tolleranza` esercita il rilascio 500 volte e
  verifica che ogni risultato resti entro la tolleranza dichiarata).
- Un memory leak nel test stesso (non nel codice di produzione), causato da una
  caratteristica del progetto da tenere a mente: `cell_getBuffer` risolve sempre passando
  dal registro **globale** (`registry_getBuffer`, vedi `registry.h`: "una sola cella per
  esecuzione"). Dopo una `registry_clear()`, anche una cella ancora viva diventa
  irraggiungibile tramite `cell_getBuffer` — bisogna liberare gli oggetti PRIMA di
  svuotare il registro, non dopo. Lo stesso accorgimento è già applicato in
  `esegui_simulazione` (`app/main.c`) per poter eseguire le due strategie in sequenza
  nello stesso processo, e serve svuotarlo anche tra un test e l'altro nello stesso
  eseguibile Unity (vedi `tearDown` in `test/test_parser.c`).
- `statistiche_stampa`/`log_stampaRiepilogo` stampavano il riepilogo finale **due volte**
  su console (vedi sezione dedicata più sotto).

## Valgrind

Verifica di memory leak/errori sul binario reale (non solo sui test):

```bash
# Scenario nominale layout 1 (esplicito, per restare confrontabile col comando
# sotto — la sola "./app/main.exe" dipende dai default correnti in main.c,
# vedi CONFIG_PATH_DEFAULT/SCENARIO_PATH_DEFAULT):
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    ./app/main.exe lib/parser/plant_config_layout1.txt lib/parser/oggetti_esempio.txt lib/parser/scenario_nominale_layout1.txt

# Scenario difficile layout 1 (terzo argomento = scenario_path, non il primo):
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    ./app/main.exe lib/parser/plant_config_layout1.txt lib/parser/oggetti_esempio.txt lib/parser/scenario_difficile_layout1.txt
```

**Risultato attuale**: `app/main.exe` risulta pulito su entrambi gli scenari — `0 errors
from 0 contexts`, tutti gli allocchi bilanciati da altrettante free (584 allocs / 584
frees su nominale, 590/590 su difficile). Verificato anche con
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

La scadenza configurata (`SCADENZA_STEP` nel plant_config, default 40 passi se assente —
vedi sezione "Tolleranze configurabili" sotto per il dettaglio) viene verificata contro
entrambe, separatamente: un oggetto può risultare "entro scadenza" secondo una
definizione e non secondo l'altra (tipicamente: quasi sempre entro scadenza sul PROCESSO,
spesso fuori scadenza sul SISTEMA — la coda è il vero collo di bottiglia, non la
pipeline).

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
`controllore_getStepBloccoGuasto`), aggiornato passo per passo e riportato in
`statistiche.txt`/`statistiche_strategia2.txt`. Il contatore storico `anomalie_rilevate`
(letture sbagliate prodotte durante il guasto) resta a 0 con questa logica: durante il
guasto la ISP non produce più letture affatto, quindi non ne produce di sbagliate — è
`step_bloccata_per_guasto` la metrica da guardare per misurare l'impatto del guasto.

Oltre alle statistiche, ogni transizione di stato del sensore produce anche un evento
discreto in `simulazione.log` (`aggiornaSensoriQualita` in `Controllore.c`, confrontando
`is_malfunzionante` prima/dopo `update_status`): un `WARNING` nell'istante esatto in cui
il sensore entra in guasto, un `INFO` quando rientra — non una riga ripetuta per ogni
passo in cui resta guasto, solo sul fronte. Verificato su entrambi i layout
(`scenario_difficile_layout1.txt`/`_layout2.txt`): 7 eventi ciascuno su 100 passi (4
ingressi in guasto + 3 rientri, coerente col ciclo `FAULT_TIME_ERROR=20`/`FAULT_TIME_OK=5`).
Test dedicati in `test/test_isp_guasto.c`
(`test_guasto_produce_un_evento_warning_nel_log`,
`test_guasto_rientrato_produce_un_evento_info_nel_log`,
`test_senza_guasto_nessun_evento_guasto_nel_log`).

`guasto_isp_id` nello scenario è generico (funziona su qualunque ISP con un sensore di
qualità agganciato) e supporta **più ISP contemporaneamente**: `FAULT_ISP=ISP1,ISP2`
(elenco separato da virgole, senza spazi) applica lo stesso guasto a entrambe nello
stesso scenario — vedi `lib/parser/scenario_doppio_guasto_layout1.txt`. Per provarlo su una sola
ISP diversa da quella di default basta cambiare `FAULT_ISP=ISP1` nel file di scenario,
senza ricompilare.

## Tolleranze configurabili: macchina, controllo qualità e scadenza

Tre parametri indipendenti, tutti impostabili dal file di configurazione impianto
(qualunque layout, es. `plant_config_layout1.txt` o `plant_config_layout2.txt`), senza
dover ricompilare:

**1. Tolleranza di lavorazione della macchina M** (rumore casuale applicato a
dimensionX/raggio al rilascio, vedi `machine_tryRelease`): campo facoltativo `TOLLERANZA`
sulla riga `MACCHINA`.

```
MACCHINA,ID=M,TEMPO=3,TOLLERANZA=0.02
```

Frazione (es. `0.02` = 2%), deve essere `>= 0`. Se assente, resta il default storico
(`0.02`, vedi `TOLLERANZA_LAVORAZIONE_DEFAULT` in `machine.c`). Un valore non valido
(negativo) viene segnalato su stderr e la macchina resta sul default, la riga non viene
comunque scartata.

**2. Soglie di classificazione qualità di un'ISP** (CONFORME/RIVALUTAZIONE/SCARTO, vedi
`get_qualita` in `S_Qualita.c`): campi facoltativi `TOLLERANZA_CONFORME` e
`TOLLERANZA_RIVALUTAZIONE` sulla riga `ISP`, in percentuale di scostamento da
`DIMX_TARGET`/`RAGGIO_TARGET`.

```
ISP,ID=ISP1,TEMPO=1,DIMX_TARGET=100,RAGGIO_TARGET=10,TOLLERANZA_CONFORME=5,TOLLERANZA_RIVALUTAZIONE=10
```

Sotto `TOLLERANZA_CONFORME` il pezzo è CONFORME, sotto `TOLLERANZA_RIVALUTAZIONE` è
RIVALUTAZIONE, oltre è SCARTO. Se assenti, restano i default storici (5%/10%). Vanno
indicati **entrambi o nessuno dei due** (un solo valore lascerebbe l'altra soglia al
default, con un rischio concreto di violare il vincolo sotto); se solo uno è presente, la
riga viene comunque collegata ma con un avviso su stderr e le soglie restano al default.
`TOLLERANZA_RIVALUTAZIONE` deve restare **maggiore o uguale** a `TOLLERANZA_CONFORME`
(altrimenti nessun pezzo potrebbe mai risultare RIVALUTAZIONE, dato che `get_qualita`
controlla prima CONFORME e poi RIVALUTAZIONE) — un valore che violi questo vincolo viene
rifiutato con un avviso su stderr, mantenendo le soglie precedenti.

Implementate rispettivamente da `machine_setTolleranzaLavorazione` (`machine.h`, già
esistente ma prima mai raggiungibile da configurazione) e dalla nuova
`sensore_qualita_imposta_tolleranze` (`S_Qualita.h`) esposta a livello di controllore da
`controllore_impostaToleranzaQualita` (`Controllore.h`) — stesso schema già usato da
`controllore_impostaGuastoQualita` per il guasto simulato. Test dedicati in
`test/test_machine.c` (setter/getter di libreria), `test/test_sensore_qualita.c` (effetto
sulla classificazione) e `test/test_tolleranza_qualita.c` (livello controllore, end-to-end).

**3. Scadenza per il calcolo "entro scadenza" nelle statistiche** (vedi sezione "Statistiche:
tempo SISTEMA vs tempo PROCESSO" sopra): campo facoltativo `SCADENZA_STEP` tra i
parametri globali del plant_config (stessa sezione di `SIM_STEPS`/`SOGLIA_BUFFER`/ecc.,
non sulla riga di un'entità specifica).

```
SCADENZA_STEP=40
```

Passi di simulazione entro cui un pezzo è considerato "in tempo". Se assente, resta il
default storico (`40`, prima hardcoded in `app/main.c`). Un valore `<= 0` disattiva del
tutto il controllo (ogni pezzo risulta "in tempo" per definizione, vedi `statistiche.c`).
A differenza delle prime due, questa NON è per-entità (macchina/ISP specifica) ma un
singolo valore globale per l'intera run — resta quindi la stessa per tutte le priorità
(vedi "Limitazioni note" più sotto per questo e altri limiti simili).

Letta da `parser_caricaSimulazione` in `SimulationConfig.scadenza_step`, usata in
`app/main.c` al posto della costante fissa. Test dedicati in `test/test_parser.c`
(`test_caricaSimulazione_SCADENZA_STEP_default_40`,
`test_caricaSimulazione_SCADENZA_STEP_letta_correttamente`).

## Smistamento generalizzato delle ISP: layout 1 e layout 2 dallo stesso codice

Il documento preliminare (sez. 1.1) propone due layout alternativi per la cella:

- **Layout 1** (`plant_config_layout1.txt`, quello di default): B1 → ISP1 (passacarte) → M →
  B2 → ISP2, dove un'unica ISP finale smista **sia** per materiale **sia** per qualità
  insieme, su 4 uscite (`A-conforme`/`rivalutazione`/`scarto`/`B-conforme`).
- **Layout 2** (`plant_config_layout2.txt`, nuovo): lo smistamento per **materiale**
  avviene subito dopo l'ingresso, **prima** della lavorazione — da lì in poi ogni
  materiale segue una linea dedicata (M1+B2+ISP_B2 per il materiale A, M2+B3+ISP_B3 per il
  B), e ciascuna ISP finale di linea smista **solo** per qualità (il materiale è già
  implicito nella linea che l'ha portato lì).

```
Layout 1:  B1 -> ISP1 -> M -> B2 -> ISP2 -+-> B_Alacciaio (A, conforme)
          (passacarte)   (ENTRAMBI)        +-> B_riqualifica (rivalutazione)
                                            +-> B_TRASH (scarto)
                                            +-> B_rame (B, conforme)

Layout 2:  B1 -> ISP0 -+-> M1 -> B2 -> ISP_B2 -+-> B_Alacciaio (conforme)
          (MATERIALE)   |        (QUALITA)      +-> B_riqualifica (rivalutazione, condiviso)
                        |                        +-> B_TRASH (scarto, condiviso)
                        +-> M2 -> B3 -> ISP_B3 -+-> B_rame (conforme)
                                     (QUALITA)   +-> B_riqualifica (rivalutazione, condiviso)
                                                  +-> B_TRASH (scarto, condiviso)
```

**Nota di design**: il documento preliminare disegnava, per il layout 2, una **quarta ISP
condivisa** tra le due linee per decidere rivalutazione/scarto. Qui non è stata
riprodotta: ogni ISP di linea (ISP_B2/ISP_B3) instrada direttamente su tutte e 3 le uscite,
senza passare da una seconda ISP — i **buffer** di destinazione (`B_riqualifica`,
`B_TRASH`) restano invece condivisi tra le due linee (più `CONNECT` verso lo stesso
buffer). Stessa semplificazione già fatta, di fatto, per il layout 1 rispetto al proprio
disegno preliminare (che a sua volta prevedeva due ISP in cascata dopo B2, ridotte a una
sola ISP2 a 4 uscite nell'implementazione finale) — vedi anche i diagrammi nel PDF
`progetto preliminare gruppo I MEJO.pdf`.

### Come funziona: `SMISTAMENTO` sulla riga ISP

Prima di questa funzionalità, la decisione su quale uscita usare (`processISP` in
`Controllore.c`) era **hardcoded** per il solo caso del layout 1 (1 uscita → passacarte;
4 uscite → combinato materiale+qualità): il layout 2 avrebbe richiesto un secondo binario
di codice, o peggio, modifiche dirette a `processISP` ogni volta che serviva un layout
diverso. Ora la decisione passa da un'unica funzione di dispatch
(`determinaIndiceUscitaQualita`, statica in `Controllore.c`), che sceglie il criterio in
base al campo facoltativo `SMISTAMENTO` sulla riga `ISP` del plant_config:

```
ISP,ID=ISP0,TEMPO=1,DIMX_TARGET=100,RAGGIO_TARGET=10,SMISTAMENTO=MATERIALE
ISP,ID=ISP_B2,TEMPO=2,DIMX_TARGET=80,RAGGIO_TARGET=6,SMISTAMENTO=QUALITA
```

Valori accettati (vedi `tipo_smistamento_t` in `Controllore.h`):

| Valore     | Criterio                                            | Uscite tipiche | Esempio d'uso                  |
|------------|------------------------------------------------------|:--------------:|---------------------------------|
| `AUTO`     | Dedotto dal numero di uscite collegate (default)      | qualunque       | non specificato = comportamento storico |
| `PASSACARTE` | Nessuna decisione, sempre indice 0                  | 1                | ISP1 nel layout 1               |
| `MATERIALE`  | Solo materiale (`get_Material`): 0='A', 1='B'       | 2 (+1 opzionale per i non classificati) | ISP0 nel layout 2 |
| `QUALITA`    | Solo esito qualità: 0=CONFORME, 1=RIVALUTAZIONE, 2=SCARTO | 3           | ISP_B2/ISP_B3 nel layout 2       |
| `ENTRAMBI`   | Combinato materiale+qualità (storico)               | 4                | ISP2 nel layout 1                |

`AUTO` (il default se il campo è assente) riproduce **esattamente** il comportamento
storico per 1 o 4+ uscite; con un numero di uscite diverso (2 o 3) ricade su `QUALITA`
(il caso più comune), ma è consigliato dichiarare `SMISTAMENTO` esplicitamente in quei
casi. Un valore non riconosciuto viene segnalato su stderr e trattato come `AUTO`, senza
scartare la riga.

Nota: entrambi i plant_config di esempio (`plant_config_layout1.txt` e
`plant_config_layout2.txt`) dichiarano `SMISTAMENTO` esplicitamente su ogni riga `ISP`,
anche dove coincide con quello che `AUTO` dedurrebbe comunque da sola — per chiarezza e
per rendere i due file direttamente confrontabili a colpo d'occhio, non perché sia
obbligatorio farlo.

**Passare da un layout all'altro richiede quindi di editare solo il plant_config**
(nuove righe `ISP`/`MACCHINA`/`BUFFER`, nuovi `CONNECT`, ed eventualmente `SMISTAMENTO`)
— non serve toccare `Controllore.c`. Anche la scoperta delle entità da monitorare per le
statistiche (`app/main.c`) è dinamica: prima elencava nomi fissi (`"M"`, `"N1"`, `"ISP1"`,
`"ISP2"`, validi solo per il layout 1), ora scopre buffer/ISP/motori direttamente dalla
cella così com'è stata costruita dal parser (vedi `controllore_haMotoreCollegato`, nuova,
usata per capire quali nastri/macchine hanno davvero un motore agganciato prima di
monitorarli).

Provare il layout 2 (con lo scenario nominale dedicato, vedi sotto — niente più l'avviso
su un'ISP inesistente):
```bash
./app/main.exe lib/parser/plant_config_layout2.txt lib/parser/oggetti_esempio.txt lib/parser/scenario_nominale_layout2.txt -
```
(il quarto argomento `-` disabilita il pre-caricamento di B2 da file, non usato in questo
layout).

Test dedicati in `test/test_smistamento_generalizzato.c` (i 4 criteri di smistamento,
validazione, `controllore_haMotoreCollegato`).

### Aggiungere un terzo layout (o un layout completamente diverso)

**Sì**, il codice ora è predisposto: un layout diverso richiede in generale solo un nuovo
plant_config (nuove righe `BUFFER`/`NASTRO`/`MACCHINA`/`ISP`, nuovi `CONNECT`,
`SMISTAMENTO` dove serve) e, se si vuole simulare un guasto, un nuovo file di scenario con
`FAULT_ISP` che punti a un'ISP che esiste davvero in quel layout — senza toccare
`Controllore.c` né `app/main.c`. Verificato concretamente con un layout "giocattolo" a
nomi completamente inventati (`ISP_UNICA`, `USCITA_OK`/`USCITA_DUBBIO`/`USCITA_KO`,
nessuno di questi mai comparso prima nel codice): funziona, e le statistiche di
completamento tornano corrette.

Restano SOLO due convenzioni implicite in `app/main.c`, non ancora parametrizzate:

- **Il buffer di ingresso è sempre `"B1"`** — hardcoded in tre punti (il nome passato a
  `parser_caricaOggetti`/`genera_arrivi_esempio` per il flusso principale, e
  `statistiche_monitoraSensorePresenza`). Un layout con un ingresso chiamato
  diversamente (o con più punti di ingresso) richiederebbe di editare quei punti — non è
  ancora letto dinamicamente dalla riga `INGRESSO` del plant_config, che invece il parser
  usa già per altro (`parser_collegaSensoriPresenza`).
- **Il pre-caricamento opzionale è sempre verso `"B2"`** (`SIM_PEZZI_B2`/`oggetti_b2_path`,
  vedi sezione dedicata sopra) — stesso discorso, nome fisso.

Il rilevamento dei buffer **terminali** (destinazione finale di un pezzo completato,
usato per le statistiche di completamento) invece **non** ha più questo problema: prima
elencava 4 nomi fissi (`"B_Alacciaio"`/`"B_riqualifica"`/`"B_TRASH"`/`"B_rame"`, gli unici
usati sia dal layout 1 sia dal layout 2 — motivo per cui il problema non era emerso prima),
ora un buffer è riconosciuto come terminale semplicemente se non ha nessuna uscita
collegata (`buffer_getOutputCount(...) == 0`), lo stesso identico criterio già usato
internamente da `Controllore.c` per decidere quando incrementare
`controllore_getCompletati()` — quindi qualunque nome si scelga per i buffer di
destinazione finale, funziona senza modifiche a `main.c`.

## File di scenario

Quattro file, uno per combinazione layout × scenario (stesso `SCENARIO_NAME`/
`LOAD_MULTIPLIER`/`FAULT_*`, solo `FAULT_ISP` cambia per puntare a un'ISP che esiste
davvero in quel layout — vedi sez. "Smistamento generalizzato delle ISP" sopra per la
topologia di ciascun layout):

- `lib/parser/scenario_nominale_layout1.txt` / `scenario_difficile_layout1.txt`
  (carico maggiore + guasto del sensore di qualità su `ISP2`, l'unica ISP finale del
  layout 1).
- `lib/parser/scenario_nominale_layout2.txt` / `scenario_difficile_layout2.txt`
  (stessa idea, guasto su `ISP_B2`, una delle due ISP finali di linea del layout 2 — la
  linea materiale 'B', `ISP_B3`, resta sempre sana, utile per confrontare le due linee
  nella stessa run).

Usare lo scenario giusto per il plant_config in uso (un'ISP inesistente nello scenario
viene comunque segnalata su stderr e ignorata, senza interrompere la simulazione — vedi
`parser_applicaScenario`). Il percorso si passa da riga di comando come terzo argomento,
senza ricompilare:
```bash
./app/main.exe lib/parser/plant_config_layout1.txt - lib/parser/scenario_difficile_layout1.txt
./app/main.exe lib/parser/plant_config_layout2.txt lib/parser/oggetti_esempio.txt lib/parser/scenario_difficile_layout2.txt -
```

C'è anche `lib/parser/scenario_doppio_guasto_layout1.txt` (guasto simultaneo su `ISP1` e
`ISP2`, solo layout 1 — non ha un equivalente per il layout 2, essendo pensato per
verificare il caso limite di due stazioni in avaria contemporaneamente sull'unica linea
del layout 1).

## Limitazioni note

Vincoli tecnici/numerici del programma (non scelte di design da discutere, solo cosa il
codice accetta o rifiuta concretamente):

- **Lunghezza di un identificativo (ID)**: 19 caratteri utili + terminatore
  (`IDLENGTH` in `object.h`, usata per ogni entità — buffer, ISP, macchina, nastro,
  motore, deviatore, sensore, oggetto). Un ID più lungo viene troncato in fase di lettura.
- **Priorità di un oggetto**: intera, da `PRIORITY_MIN=0` a `PRIORITY_MAX=10` (`object.h`).
  Un valore fuori range viene rifiutato da `object_create`.
- **ISP guaste contemporaneamente in un singolo scenario**: al massimo 4
  (`MAX_GUASTO_ISP` in `parser.h`). `FAULT_ISP=ISP1,ISP2,ISP3,ISP4` è il massimo elencabile
  in una riga; oltre la quarta, le entrate in più vengono scartate con un avviso su
  stderr, senza bloccare le altre. Tutte le ISP elencate condividono lo stesso profilo di
  guasto (`FAULT_TIME_ERROR`/`FAULT_TIME_OK` unici per l'intera riga).
- **Lunghezza di una riga nei file di configurazione/scenario/oggetti**: 255 caratteri
  (`MAX_LINE_LEN` in `parser.c`). Una riga più lunga viene troncata da `fgets`.
- **Campi per riga in un file di configurazione**: al massimo 16 token separati da virgola
  (`tokens[16]` in `parser.c`), abbondante per il formato chiave=valore in uso (nessuna
  riga del progetto ne usa più di 6-7).
- **Lunghezza di una singola chiave/valore in un campo `CHIAVE=valore`**: 31 caratteri
  (63 per i valori `SMISTAMENTO`/nomi scenario, vedi i buffer `key[32]`/`value[32]`/
  `value[64]` in `parser.c`). Un valore più lungo viene troncato.
- **Numero di buffer, ISP, macchine, nastri, oggetti**: **nessun limite fisso** — sono
  tutte liste concatenate allocate dinamicamente (`cell.c`, `registry.c`), limitate solo
  dalla memoria disponibile. Lo stesso vale per il numero di pezzi in ingresso (letti riga
  per riga dal file oggetti, nessun array a dimensione fissa) e per il numero di uscite
  collegate a un'ISP/buffer/macchina/nastro tramite `CONNECT`.
- **Capacità di un buffer/nastro**: intera (`int`), quindi fino a circa 2 miliardi
  (`INT_MAX`) — nessun limite pratico rilevante per questo progetto.
- **Tolleranza di lavorazione macchina, soglie qualità, scadenza**: valori `double`/`int`
  senza limite superiore imposto dal codice (solo vincoli di coerenza tra loro, es.
  `TOLLERANZA_RIVALUTAZIONE >= TOLLERANZA_CONFORME` — vedi sezione "Tolleranze
  configurabili" sopra).
