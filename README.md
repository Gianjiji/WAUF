# WAUF (Where Are You From) - ESP8266 dev board (NodeMCU / Wemos D1 mini)

Mostra gli aerei che sorvolano casa e evidenzia quelli diretti all'aeroporto scelto
(default: Torino Caselle, LIMF/TRN). Web UI stile tabellone aeroportuale con loghi
delle compagnie + display OLED. Tutta la configurazione (WiFi, aeroporto, posizione)
si fa dalla pagina web e resta salvata in EEPROM. Aggiornamenti firmware via WiFi (OTA).

File del progetto:

| File            | Contenuto                                            |
|-----------------|------------------------------------------------------|
| `WAUF.ino`      | firmware (WiFi, API adsb.lol, OLED, web server, OTA) |
| `webpage.h`     | tabellone `/` (HTML/CSS/JS in PROGMEM)               |
| `configpage.h`  | pagina di configurazione `/config`                   |
| `updatepage.h`  | pagina di upload OTA `/update`                       |

## Cablaggio

| NodeMCU / D1 mini | OLED 128x64  |
|-------------------|--------------|
| D2 (GPIO4)        | SDA          |
| D1 (GPIO5)        | SCL          |
| 3V3               | VCC          |
| GND               | GND          |

La scheda si alimenta dalla USB (5 V); il regolatore di bordo fornisce i 3,3 V a ESP e OLED.

## Scelta del display

Il firmware supporta due OLED I2C 128x64, stesso cablaggio e stesso layout:

| Display | Controller | Libreria             | Riga nello sketch          |
|---------|------------|----------------------|----------------------------|
| 1.3"    | SH1106     | **Adafruit SH110X**  | `#define OLED_SH1106` (default) |
| 0.96"   | SSD1306    | **Adafruit SSD1306** | commentare `#define OLED_SH1106` |

Se con il 1.3" l'immagine appare spostata di qualche pixel con una colonna di
"spazzatura" a destra, stai usando il driver SSD1306 su un SH1106: attiva la define.
Indirizzo I2C 0x3C per entrambi (raro: 0x3D, cambia `OLED_ADDR`).

## Arduino IDE

1. Board manager: **esp8266 by ESP8266 Community**
2. Scheda: `NodeMCU 1.0 (ESP-12E Module)` oppure `LOLIN(WEMOS) D1 R2 & mini`
   - Flash Size: **4MB (FS:none OTA:~1019KB)** (o FS:1MB; NON FS:3MB, vedi sezione OTA)
   - CPU Frequency: **160 MHz** (aiuta il TLS)
   - lwIP: v2 Lower Memory
3. Librerie (Library Manager): **ArduinoJson** (v7), **Adafruit GFX**, e **Adafruit SH110X**
   per il display 1.3" (oppure **Adafruit SSD1306** per lo 0.96", vedi sezione display).
   DNSServer, EEPROM, mDNS e WebServer fanno parte del core ESP8266.
4. Collega la USB, scegli la porta COM, Upload. Monitor seriale a 115200 baud.
5. Prima del caricamento cambia `OTA_PASS` nello sketch (password della pagina di
   aggiornamento). Posizione di default: Torino centro (`DEF_LAT`/`DEF_LON`), usata
   solo finche' non ne salvi una dalla pagina di configurazione.

## Primo avvio: configurazione WiFi

Nel codice non ci sono piu' SSID e password.

1. Al primo avvio (o se la rete salvata non risponde entro 20 s) l'ESP crea la rete
   WiFi aperta **WAUF-Setup**. L'OLED lo mostra.
2. Collegati a quella rete con il telefono o il PC. Di solito si apre da solo il
   portale di configurazione; altrimenti apri `http://192.168.4.1`.
3. Premi **CERCA RETI**, scegli la tua rete, inserisci la password, **SALVA E RIAVVIA**.
4. L'ESP si riavvia e si collega alla tua rete. Torna sulla tua rete WiFi e apri
   `http://wauf.local` (o l'IP mostrato sull'OLED).

Se in modalita' access point ci sono credenziali salvate (es. router spento), dopo
3 minuti l'ESP si riavvia da solo e riprova la rete di casa.

## Pagina di configurazione (`/config`)

Raggiungibile dal pulsante CONFIGURAZIONE del tabellone.

- **Stato**: modo (connesso / access point), rete, IP, segnale, indirizzo mDNS.
- **Rete WiFi**: scansione reti, password, salva e riavvia.
- **Aggiornamento firmware (OTA)**: versione firmware, dimensione sketch, spazio
  libero e dimensione flash; avvisa se lo spazio non basta. Link alla pagina `/update`.
- **Aeroporto da monitorare**: codice IATA (3 lettere), ICAO (4 lettere) e nome
  mostrato nel titolo del tabellone. Esempi: MXP/LIMC, LIN/LIML, FCO/LIRF, GOA/LIMJ.
- **Posizione e raggio**: mappa **OpenStreetMap** in tema scuro (Leaflet, nessuna
  API key): clicca o trascina il marcatore, il cerchio mostra il raggio. Campi manuali
  per chi e' in modalita' access point (senza internet la mappa non si carica).
  Il pulsante USA GPS funziona solo su https per limiti del browser.

Tutto viene salvato in EEPROM e sopravvive a riavvii e riflash.

## Tabellone (`/`)

- Colonne: logo compagnia, volo, rotta, tipo aereo, quota (con freccia salita/discesa),
  velocita', distanza e direzione da casa, stato.
- Passando il mouse su una riga: matricola, tipo e codice ICAO24 del velivolo.
- Filtro **SOLO <IATA>** / **TUTTO IL TRAFFICO**, refresh automatico ogni 10 s.
- Pulsante **MAPPA**: mappa OpenStreetMap in tema scuro (tile standard, colori
  invertiti via CSS: nessuna chiave ne' servizi aggiuntivi) con casa, cerchio del raggio e un'icona
  aereo per ogni volo, orientata secondo la rotta (verde se diretto all'aeroporto,
  lampeggiante se IN VISTA). Clic sull'aereo per i dettagli. Segue il filtro attivo.
  La scelta mappa aperta/chiusa viene ricordata dal browser.
- Stati: **IN VISTA** (verde lampeggiante: diretto all'aeroporto, in discesa sotto
  8000 ft, guarda fuori), **IN ARRIVO**, TRANSITO, ROTTA N.D.
- Layout adattivo senza barre di scorrimento: le tessere scalano con la finestra;
  sotto 980 px spariscono AEREO e VEL, sotto 640 px il logo; sotto 480 px scorre
  solo il riquadro del tabellone (telefoni in verticale).
- LED di bordo acceso quando c'e' un aereo IN VISTA.

## Display OLED

- Avvio: nome, poi rete WiFi in connessione (puntini). In access point: istruzioni
  per la prima configurazione (nome rete e indirizzo).
- Ogni 5 s alterna la pagina INFO (IP, mDNS, posizione, raggio, segnale, conteggi
  aerei / diretti all'aeroporto / in vista) e la pagina VOLO (il piu' rilevante:
  IN VISTA, altrimenti in arrivo, altrimenti il piu' vicino) con callsign, rotta,
  tipo, quota con freccia, velocita', distanza e direzione da casa.

## Aggiornamento firmware via WiFi (OTA)

1. Nell'IDE Arduino: **Sketch → Esporta binario compilato**. Il file `.bin` finisce
   nella cartella dello sketch.
2. Apri `http://wauf.local/update` (link anche nella pagina di configurazione),
   utente `admin`, password `OTA_PASS` definita nello sketch (**cambiala**).
3. Scegli il `.bin` (solo firmware: il progetto non usa un filesystem), premi
   CARICA E RIAVVIA, segui la barra di avanzamento. Al termine il dispositivo si
   riavvia e la pagina torna da sola al tabellone dopo 20 s. Non togliere alimentazione.

Spazio: l'OTA richiede spazio libero nell'area sketch almeno pari al nuovo binario.
Lo spazio dipende dalla voce **Flash Size** dell'IDE (il valore "OTA:~xxxKB" nel nome
e' proprio quello):

| Flash Size (IDE)              | Spazio OTA | OTA con firmware ~500 KB |
|-------------------------------|------------|--------------------------|
| 4MB (FS:none OTA:~1019KB)     | ~1019 KB   | si'                      |
| 4MB (FS:1MB OTA:~1019KB)      | ~1019 KB   | si'                      |
| 4MB (FS:2MB OTA:~1019KB)      | ~1019 KB   | si'                      |
| 4MB (FS:3MB OTA:~512KB)       | ~512 KB    | **no**, troppo stretto   |

Se la pagina di configurazione segnala "SPAZIO INSUFFICIENTE", ricarica via USB
con Flash Size **4MB (FS:none)**: da quel momento gli aggiornamenti via OTA funzionano.
La dimensione dell'applicativo non c'entra: e' il layout della flash.
L'OTA non e' attivo in modalita' access point.

## Loghi delle compagnie

Il device invia solo il codice ICAO della compagnia (es. RYR). Il browser scarica il
logo da FlightAware (`flightaware.com/images/airline_logos/90p/<ICAO>.png`), senza
chiave. Se il routeset non fornisce il codice, si usano le prime tre lettere del
callsign. Se il logo non esiste appare il codice testuale. E' un hotlink a un servizio
di terzi: va bene per uso personale, potrebbe smettere di funzionare senza preavviso.

## Dati disponibili per ogni volo

Dall'endpoint `point` di adsb.lol (usati: callsign, hex, matricola, tipo, lat/lon,
quota, velocita', rotta, rateo di salita). Disponibili ma non usati: quota GPS,
squawk, emergenza, categoria, operatore (`ownOp`), descrizione aereo (`desc`),
eta' dell'ultimo messaggio, quota selezionata sull'autopilota.
Dal `routeset`: compagnia, numero volo, aeroporti di partenza/arrivo con nome e
coordinate.

## Endpoint HTTP

| Metodo | Percorso        | Funzione                                   |
|--------|-----------------|--------------------------------------------|
| GET    | `/`             | tabellone                                  |
| GET    | `/config`       | pagina di configurazione                   |
| GET    | `/api/flights`  | JSON voli + stato                          |
| GET    | `/api/status`   | JSON modo WiFi, rete, IP, aeroporto, pos.  |
| GET    | `/api/scan`     | JSON reti WiFi visibili (bloccante ~3 s)   |
| POST   | `/api/wifi`     | `ssid`, `pass` -> salva e riavvia          |
| POST   | `/api/dest`     | `iata`, `icao`, `name`                     |
| POST   | `/api/config`   | `lat`, `lon`, `radius`                     |
| GET    | `/update`       | pagina di upload OTA (login admin/OTA_PASS)|
| POST   | `/update`       | upload del `.bin` (campo `firmware`)       |

In modalita' access point ogni URL sconosciuto reindirizza a `/config` (captive
portal) e `/update` non e' registrato.

## Dati

- Posizioni: `api.adsb.lol/v2/point/<lat>/<lon>/<raggio>` (gratuita, senza chiave)
- Rotte: `api.adsb.lol/api/0/routeset` (POST). L'endpoint risponde con dati solo
  se la richiesta contiene l'header `Referer: https://adsb.lol/`; il firmware lo invia.
- Polling ogni 30 s per non gravare su un servizio gratuito e comunitario.

## Note

- Aggiornando da una versione precedente la EEPROM viene reinizializzata (layout
  cambiato): posizione e raggio tornano ai default e va rifatta la configurazione WiFi.
- Il sistema di build dell'IDE Arduino richiede i prototipi espliciti di `isVisible` e
  `cacheFind` dopo le struct: non toglierli.
- Velocita', rotta, quota e rateo arrivano dall'API con i decimali: vanno letti come
  float e arrotondati (con `| 0` ArduinoJson restituirebbe il default, cioe' 0 KT).
- Versione firmware in `FW_VERSION`: aggiornala a ogni release, e' mostrata in `/config`.

## Costanti di configurazione nello sketch

| Costante          | Default          | Significato                                  |
|-------------------|------------------|----------------------------------------------|
| `MDNS_NAME`       | `wauf`           | nome mDNS (`http://wauf.local`)              |
| `AP_SSID`         | `WAUF-Setup`     | rete aperta di prima configurazione          |
| `OTA_USER/PASS`   | `admin` / `wauf` | login della pagina `/update` (cambia la pass)|
| `DEF_LAT/LON`     | Torino centro    | posizione di default                         |
| `DEF_RADIUS`      | 25 nm            | raggio di default (max 250)                  |
| `DEF_DEST_*`      | TRN / LIMF       | aeroporto di default                         |
| `VISIBLE_ALT_FT`  | 8000 ft          | sotto questa quota, in discesa -> IN VISTA   |
| `VISIBLE_VR`      | -300 ft/min      | rateo minimo di discesa per IN VISTA         |
| `POLL_MS`         | 30 s             | intervallo interrogazione adsb.lol           |
| `WIFI_TIMEOUT_MS` | 20 s             | attesa WiFi prima di aprire l'access point   |
| `AP_RETRY_MS`     | 3 min            | in AP con credenziali salvate: riprova STA   |
| `OLED_SH1106`     | definita         | display 1.3" (commenta per lo 0.96")         |
