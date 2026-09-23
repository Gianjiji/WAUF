/*
 * WAUF (Where Are You From) - ESP8266 (NodeMCU / Wemos D1 mini) + OLED 128x64 I2C (0.96" o 1.3")
 * -----------------------------------------------------------------------------
 * Rileva gli aerei che sorvolano casa (dati ADS-B via api.adsb.lol),
 * ricava la rotta dal callsign (endpoint routeset) ed evidenzia i voli
 * diretti all'aeroporto scelto (default Torino Caselle, LIMF / TRN).
 *
 * - Web UI stile tabellone aeroportuale su http://<ip>/ o http://wauf.local
 * - Pagina /config: WiFi (scansione reti + password), aeroporto da monitorare,
 *   posizione su mappa OpenStreetMap (senza API key)
 * - Primo avvio (nessun WiFi salvato): access point "WAUF-Setup",
 *   apri http://192.168.4.1 (captive portal) e configura la rete.
 * - OLED con info di collegamento + volo in arrivo piu' vicino
 *
 * Hardware (NodeMCU v2/v3, Wemos D1 mini, qualsiasi dev board ESP8266 con USB):
 *   D2 (GPIO4) = SDA, D1 (GPIO5) = SCL  -> I2C hardware di default, nessun
 *   vincolo di boot. Alimentazione OLED dal pin 3V3 della scheda.
 *   LED di bordo (LED_BUILTIN): lampeggia durante la connessione WiFi,
 *   acceso fisso se c'e' un aereo IN VISTA.
 *
 * Board: "NodeMCU 1.0 (ESP-12E Module)" o "LOLIN(WEMOS) D1 R2 & mini",
 *        Flash 4MB (FS:none o 1MB), CPU 160 MHz consigliata.
 * Librerie: ArduinoJson (v7), Adafruit GFX, e Adafruit SH110X (1.3") oppure
 *           Adafruit SSD1306 (0.96"): vedi #define OLED_SH1106 qui sotto.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>   // OTA via browser: /update (nel core ESP8266)
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

// ---- Scelta display (entrambi 128x64 I2C, stesso layout) ----
//   OLED_SH1106  -> 1.3"  (controller SH1106, libreria "Adafruit SH110X")
//   commentato   -> 0.96" (controller SSD1306, libreria "Adafruit SSD1306")
#define OLED_SH1106

#ifdef OLED_SH1106
  #include <Adafruit_SH110X.h>
  #define OLED_WHITE SH110X_WHITE
#else
  #include <Adafruit_SSD1306.h>
  #define OLED_WHITE SSD1306_WHITE
#endif

// ======================= CONFIGURAZIONE =======================
const char* MDNS_NAME   = "wauf";        // -> http://wauf.local
const char* AP_SSID     = "WAUF-Setup"; // rete aperta di configurazione (primo avvio)
const char* OTA_USER    = "admin";      // login per http://wauf.local/update
const char* OTA_PASS    = "wauf";       // CAMBIALA: chi la conosce puo' riscrivere il firmware
const char* FW_VERSION  = "1.1.0";

// Posizione di default: Torino, centro citta' (Piazza Castello).
// Modificabile dalla pagina /config: viene salvata in EEPROM.
const float DEF_LAT     = 45.0703f;
const float DEF_LON     = 7.6869f;
const int   DEF_RADIUS  = 25;               // miglia nautiche (max 250)

// Soglie "visibile a occhio nudo": diretto a TRN, in discesa e sotto quota
const int   VISIBLE_ALT_FT = 8000;          // ft
const int   VISIBLE_VR     = -300;          // ft/min (discesa)

// Aeroporto di default da monitorare (modificabile da /config, salvato in EEPROM)
const char* DEF_DEST_IATA = "TRN";
const char* DEF_DEST_ICAO = "LIMF";
const char* DEF_DEST_NAME = "TORINO CASELLE";

const uint32_t POLL_MS       = 30000;       // polling ADS-B (30 s: gentile con l'API)
const uint32_t OLED_PAGE_MS  = 5000;        // alternanza pagine OLED
const uint32_t STALE_MS      = 90000;       // scarta velivoli non visti da 90 s
const uint32_t WIFI_TIMEOUT_MS = 20000;     // attesa WiFi prima di aprire l'access point
const uint32_t AP_RETRY_MS   = 180000;      // in AP con credenziali salvate: riprova STA dopo 3 min
// ==============================================================

#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C
#define SDA_PIN 4            // D2 su NodeMCU / D1 mini
#define SCL_PIN 5            // D1 su NodeMCU / D1 mini

#ifdef OLED_SH1106
Adafruit_SH1106G display(SCREEN_W, SCREEN_H, &Wire, -1);
#else
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
#endif
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer otaServer;
DNSServer dnsServer;
bool apMode = false;

// ---------------- Configurazione runtime (persistita in EEPROM) ----------------
float homeLat  = DEF_LAT;
float homeLon  = DEF_LON;
int   radiusNm = DEF_RADIUS;
char  wifiSsid[33] = "";
char  wifiPass[65] = "";
char  destIata[4]  = "TRN";
char  destIcao[5]  = "LIMF";
char  destName[25] = "TORINO CASELLE";

#define CFG_MAGIC 0xC453113CUL   // cambiato rispetto alle versioni precedenti: layout EEPROM diverso
struct PersistCfg {
  uint32_t magic;
  float    lat, lon;
  uint8_t  radius;
  char     ssid[33];
  char     pass[65];
  char     iata[4];
  char     icao[5];
  char     name[25];
};

void loadConfig() {
  strcpy(destIata, DEF_DEST_IATA); strcpy(destIcao, DEF_DEST_ICAO); strcpy(destName, DEF_DEST_NAME);
  PersistCfg c;
  EEPROM.get(0, c);
  if (c.magic == CFG_MAGIC && c.lat >= -90 && c.lat <= 90 &&
      c.lon >= -180 && c.lon <= 180 && c.radius >= 1) {
    homeLat = c.lat; homeLon = c.lon; radiusNm = c.radius;
    c.ssid[32] = 0; c.pass[64] = 0; c.iata[3] = 0; c.icao[4] = 0; c.name[24] = 0;
    strcpy(wifiSsid, c.ssid); strcpy(wifiPass, c.pass);
    if (c.iata[0]) { strcpy(destIata, c.iata); strcpy(destIcao, c.icao); strcpy(destName, c.name); }
    Serial.printf("[i] cfg EEPROM: %.4f %.4f r%d ssid='%s' dest=%s/%s\n",
                  homeLat, homeLon, radiusNm, wifiSsid, destIata, destIcao);
  } else {
    Serial.println(F("[i] cfg: uso i default (nessun WiFi salvato)"));
  }
}

void saveConfig() {
  PersistCfg c = {};
  c.magic = CFG_MAGIC; c.lat = homeLat; c.lon = homeLon; c.radius = (uint8_t)radiusNm;
  strncpy(c.ssid, wifiSsid, 32); strncpy(c.pass, wifiPass, 64);
  strncpy(c.iata, destIata, 3); strncpy(c.icao, destIcao, 4); strncpy(c.name, destName, 24);
  EEPROM.put(0, c);
  EEPROM.commit();
}

// ---------------- Strutture dati ----------------
#define MAX_FLIGHTS 8
struct Flight {
  char  callsign[10];
  char  hex[8];
  char  reg[10];        // matricola (es. EI-DWL)
  char  type[6];        // tipo ICAO (es. B738)
  char  airline[4];     // codice ICAO compagnia (es. RYR) dal routeset
  char  route[20];      // es. "FCO-TRN" (IATA) oppure "?"
  bool  toDest;         // diretto a Caselle
  bool  visible;        // diretto a TRN, in discesa, sotto VISIBLE_ALT_FT
  bool  routeKnown;
  float lat, lon;
  float distKm;
  int   bearing;        // da casa verso l'aereo
  int   altFt;          // -1 = a terra/n.d.
  int   gsKt;
  int   track;
  int   vRate;          // ft/min
  uint32_t seenAt;      // millis() ultimo avvistamento
  bool  active;
};
Flight flights[MAX_FLIGHTS];

#define ROUTE_CACHE 14
struct RouteEntry { char callsign[10]; char route[20]; char airline[4]; uint32_t ts; bool used; };
RouteEntry routeCache[ROUTE_CACHE];

// Prototipi espliciti: l'IDE Arduino genera i suoi prima delle struct e fallisce
bool isVisible(const Flight& f);
RouteEntry* cacheFind(const char* cs);

uint32_t lastPoll = 0, lastPageFlip = 0, bootMillis = 0, lastOkFetch = 0, apStarted = 0;
uint8_t  oledPage = 0;
uint16_t pollCount = 0, errCount = 0;
bool     restartPending = false;
uint32_t restartAt = 0;

// ---------------- Utility ----------------
float deg2rad(float d) { return d * 0.017453292519943f; }

float haversineKm(float lat1, float lon1, float lat2, float lon2) {
  float dLat = deg2rad(lat2 - lat1), dLon = deg2rad(lon2 - lon1);
  float a = sinf(dLat / 2) * sinf(dLat / 2) +
            cosf(deg2rad(lat1)) * cosf(deg2rad(lat2)) * sinf(dLon / 2) * sinf(dLon / 2);
  return 6371.0f * 2 * atan2f(sqrtf(a), sqrtf(1 - a));
}

int bearingDeg(float lat1, float lon1, float lat2, float lon2) {
  float dLon = deg2rad(lon2 - lon1);
  float y = sinf(dLon) * cosf(deg2rad(lat2));
  float x = cosf(deg2rad(lat1)) * sinf(deg2rad(lat2)) -
            sinf(deg2rad(lat1)) * cosf(deg2rad(lat2)) * cosf(dLon);
  int b = (int)(atan2f(y, x) * 57.29578f);
  return (b + 360) % 360;
}

const char* compass8(int deg) {
  static const char* pts[] = {"N","NE","E","SE","S","SO","O","NO"};
  return pts[((deg + 22) % 360) / 45];
}

void trimRight(char* s) {
  int n = strlen(s);
  while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\r' || s[n-1] == '\n')) s[--n] = 0;
}

// Il volo e' diretto all'aeroporto monitorato? (destinazione = un segmento dopo il primo)
bool routeToDest(const char* route) {
  if (!route[0] || route[0] == '?') return false;
  char buf[20]; strncpy(buf, route, 19); buf[19] = 0;
  char* tok = strtok(buf, "-");
  bool first = true;
  while (tok) {
    if (!first && (strcmp(tok, destIata) == 0 || strcmp(tok, destIcao) == 0)) return true;
    first = false;
    tok = strtok(NULL, "-");
  }
  return false;
}

// "Visibile a occhio nudo": inbound Caselle, in discesa, sotto quota soglia
bool isVisible(const Flight& f) {
  return f.toDest && f.altFt > 0 && f.altFt < VISIBLE_ALT_FT && f.vRate <= VISIBLE_VR;
}

RouteEntry* cacheFind(const char* cs) {
  for (int i = 0; i < ROUTE_CACHE; i++)
    if (routeCache[i].used && strcmp(routeCache[i].callsign, cs) == 0) return &routeCache[i];
  return NULL;
}

void cachePut(const char* cs, const char* route, const char* airline) {
  int slot = -1; uint32_t oldest = 0xFFFFFFFF;
  for (int i = 0; i < ROUTE_CACHE; i++) {
    if (!routeCache[i].used) { slot = i; break; }
    if (routeCache[i].ts < oldest) { oldest = routeCache[i].ts; slot = i; }
  }
  routeCache[slot].used = true;
  routeCache[slot].ts = millis();
  strncpy(routeCache[slot].callsign, cs, 9);      routeCache[slot].callsign[9] = 0;
  strncpy(routeCache[slot].route, route, 19);     routeCache[slot].route[19] = 0;
  strncpy(routeCache[slot].airline, airline, 3);  routeCache[slot].airline[3] = 0;
}

// ---------------- Fetch ADS-B ----------------
bool fetchAircraft() {
  if (ESP.getFreeHeap() < 22000) { Serial.println(F("[!] heap basso, salto poll")); return false; }

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();                       // niente verifica cert: ok per dati pubblici

  HTTPClient http;
  http.useHTTP10(true);                        // necessario per il parsing in streaming
  http.setTimeout(9000);

  char url[96];
  snprintf(url, sizeof(url), "https://api.adsb.lol/v2/point/%.4f/%.4f/%d",
           homeLat, homeLon, radiusNm);

  if (!http.begin(*client, url)) return false;
  int code = http.GET();
  if (code != 200) { Serial.printf("[!] point HTTP %d\n", code); http.end(); errCount++; return false; }

  // Filtro: teniamo solo i campi che servono (RAM preziosa!)
  JsonDocument filter;
  filter["ac"][0]["hex"]       = true;
  filter["ac"][0]["flight"]    = true;
  filter["ac"][0]["r"]         = true;   // matricola
  filter["ac"][0]["t"]         = true;   // tipo aereo
  filter["ac"][0]["lat"]       = true;
  filter["ac"][0]["lon"]       = true;
  filter["ac"][0]["alt_baro"]  = true;
  filter["ac"][0]["gs"]        = true;
  filter["ac"][0]["track"]     = true;
  filter["ac"][0]["baro_rate"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { Serial.printf("[!] json: %s\n", err.c_str()); errCount++; return false; }

  for (int i = 0; i < MAX_FLIGHTS; i++) flights[i].active = false;

  int n = 0;
  for (JsonObject ac : doc["ac"].as<JsonArray>()) {
    if (!ac["lat"].is<float>() || !ac["lon"].is<float>()) continue;

    Flight f = {};
    f.active = true;
    f.lat = ac["lat"]; f.lon = ac["lon"];
    f.distKm  = haversineKm(homeLat, homeLon, f.lat, f.lon);
    f.bearing = bearingDeg(homeLat, homeLon, f.lat, f.lon);
    // Nota: gs e track arrivano con i decimali (es. 445.9): con "| 0" ArduinoJson
    // restituirebbe il default. Si legge come float e si arrotonda.
    f.altFt = ac["alt_baro"].is<float>() ? (int)lroundf(ac["alt_baro"].as<float>()) : -1;  // "ground" -> -1
    f.gsKt  = ac["gs"].is<float>()        ? (int)lroundf(ac["gs"].as<float>())        : 0;
    f.track = ac["track"].is<float>()     ? ((int)lroundf(ac["track"].as<float>()) % 360) : -1;
    f.vRate = ac["baro_rate"].is<float>() ? (int)lroundf(ac["baro_rate"].as<float>()) : 0;
    f.seenAt = millis();
    strncpy(f.hex, ac["hex"] | "", 7); f.hex[7] = 0;
    strncpy(f.reg, ac["r"] | "", 9);   f.reg[9] = 0;
    strncpy(f.type, ac["t"] | "", 5);  f.type[5] = 0;
    strncpy(f.callsign, ac["flight"] | "", 9); f.callsign[9] = 0;
    trimRight(f.callsign);
    if (!f.callsign[0]) strncpy(f.callsign, f.hex, 9);

    RouteEntry* rc = cacheFind(f.callsign);
    if (rc) { strncpy(f.route, rc->route, 19); strncpy(f.airline, rc->airline, 3); f.routeKnown = true; }
    else    { strcpy(f.route, "?"); f.routeKnown = false; }
    f.toDest  = routeToDest(f.route);
    f.visible = isVisible(f);

    // Inserimento ordinato per distanza, max MAX_FLIGHTS
    int pos = n < MAX_FLIGHTS ? n : -1;
    for (int j = 0; j < n && j < MAX_FLIGHTS; j++)
      if (f.distKm < flights[j].distKm) { pos = j; break; }
    if (pos >= 0) {
      for (int j = min(n, MAX_FLIGHTS - 1); j > pos; j--) flights[j] = flights[j-1];
      flights[pos] = f;
      if (n < MAX_FLIGHTS) n++;
    }
  }
  pollCount++;
  lastOkFetch = millis();
  Serial.printf("[i] %d aerei nel raggio, heap %u\n", n, ESP.getFreeHeap());
  return true;
}

// Rotte per i callsign non in cache (una POST unica su /api/0/routeset)
void fetchRoutes() {
  String body; body.reserve(300);
  body = F("{\"planes\":[");
  int need = 0;
  for (int i = 0; i < MAX_FLIGHTS && need < 6; i++) {
    if (!flights[i].active || flights[i].routeKnown) continue;
    if (need) body += ',';
    body += F("{\"callsign\":\"");
    body += flights[i].callsign;
    body += F("\",\"lat\":");
    body += String(flights[i].lat, 3);
    body += F(",\"lng\":");
    body += String(flights[i].lon, 3);
    body += '}';
    need++;
  }
  body += F("]}");
  if (!need) return;
  if (ESP.getFreeHeap() < 22000) return;

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(9000);
  if (!http.begin(*client, F("https://api.adsb.lol/api/0/routeset"))) return;
  http.addHeader(F("Content-Type"), F("application/json"));
  // Senza Referer l'API risponde 201 con corpo vuoto (verificato 2026-09)
  http.addHeader(F("Referer"), F("https://adsb.lol/"));
  int code = http.POST(body);
  if (code != 200 && code != 201) { Serial.printf("[!] routeset HTTP %d\n", code); http.end(); return; }

  JsonDocument filter;
  filter[0]["callsign"]            = true;
  filter[0]["_airport_codes_iata"] = true;
  filter[0]["airport_codes"]       = true;
  filter[0]["airline_code"]        = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { Serial.printf("[!] routeset json: %s\n", err.c_str()); return; }

  for (JsonObject r : doc.as<JsonArray>()) {
    const char* cs = r["callsign"] | "";
    const char* rt = r["_airport_codes_iata"] | (const char*)(r["airport_codes"] | "?");
    const char* al = r["airline_code"] | "";
    if (!cs[0]) continue;
    if (strcmp(rt, "unknown") == 0 || !rt[0]) rt = "?";   // callsign non in database
    cachePut(cs, rt, al);
    for (int i = 0; i < MAX_FLIGHTS; i++) {
      if (flights[i].active && strcmp(flights[i].callsign, cs) == 0) {
        strncpy(flights[i].route, rt, 19);
        strncpy(flights[i].airline, al, 3); flights[i].airline[3] = 0;
        flights[i].routeKnown = true;
        flights[i].toDest  = routeToDest(flights[i].route);
        flights[i].visible = isVisible(flights[i]);
      }
    }
  }
}

// ---------------- Web server ----------------
#include "webpage.h"      // HTML/CSS/JS del tabellone (PROGMEM)
#include "configpage.h"   // HTML/CSS/JS della pagina di configurazione (PROGMEM)
#include "updatepage.h"   // pagina di upload OTA (PROGMEM)

void handleRoot() {
  if (apMode) { server.sendHeader("Location", "/config"); server.send(302, "text/plain", ""); return; }
  server.send_P(200, "text/html", WEBPAGE);
}
void handleConfigPage() { server.send_P(200, "text/html", CONFIGPAGE); }

void handleApi() {
  JsonDocument doc;
  doc["dest"]    = destIata;
  doc["destName"]= destName;
  doc["lat"]     = serialized(String(homeLat, 4));
  doc["lon"]     = serialized(String(homeLon, 4));
  doc["radius"]  = radiusNm;
  doc["rssi"]    = WiFi.RSSI();
  doc["heap"]    = ESP.getFreeHeap();
  doc["uptime"]  = (millis() - bootMillis) / 1000;
  doc["polls"]   = pollCount;
  doc["age"]     = lastOkFetch ? (millis() - lastOkFetch) / 1000 : -1;

  JsonArray arr = doc["flights"].to<JsonArray>();
  for (int i = 0; i < MAX_FLIGHTS; i++) {
    if (!flights[i].active) continue;
    if (millis() - flights[i].seenAt > STALE_MS) continue;
    JsonObject o = arr.add<JsonObject>();
    o["cs"]    = flights[i].callsign;
    o["hex"]   = flights[i].hex;
    o["reg"]   = flights[i].reg;
    o["typ"]   = flights[i].type;
    o["al"]    = flights[i].airline;
    o["route"] = flights[i].route;
    o["alt"]   = flights[i].altFt;
    o["gs"]    = flights[i].gsKt;
    o["vr"]    = flights[i].vRate;
    o["trk"]   = flights[i].track;
    o["lat"]   = serialized(String(flights[i].lat, 4));
    o["lon"]   = serialized(String(flights[i].lon, 4));
    o["dist"]  = serialized(String(flights[i].distKm, 1));
    o["dir"]   = compass8(flights[i].bearing);
    o["trn"]   = flights[i].toDest;
    o["vis"]   = flights[i].visible;
  }
  String out; out.reserve(2048);
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// GET /api/status -> stato per la pagina di configurazione
void handleStatus() {
  JsonDocument doc;
  doc["mode"]   = apMode ? "ap" : "sta";
  doc["ssid"]   = wifiSsid;
  doc["ip"]     = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["rssi"]   = apMode ? 0 : WiFi.RSSI();
  doc["lat"]    = serialized(String(homeLat, 5));
  doc["lon"]    = serialized(String(homeLon, 5));
  doc["radius"] = radiusNm;
  doc["mdns"]   = MDNS_NAME;
  doc["iata"]   = destIata;
  doc["icao"]   = destIcao;
  doc["name"]   = destName;
  doc["fw"]     = FW_VERSION;
  doc["sketch"] = ESP.getSketchSize();
  doc["free"]   = ESP.getFreeSketchSpace();
  doc["flash"]  = ESP.getFlashChipRealSize();   // chip fisico
  doc["flashCfg"] = ESP.getFlashChipSize();     // quello dichiarato nell'IDE
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// GET /api/scan -> reti WiFi visibili (bloccante ~2-3 s)
void handleScan() {
  int n = WiFi.scanNetworks(false, false);
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 25; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    bool dup = false;                         // stesso SSID su piu' canali: uno solo
    for (JsonObject o : arr) if (s == (const char*)o["ssid"]) { dup = true; break; }
    if (dup) continue;
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = s;
    o["rssi"] = WiFi.RSSI(i);
    o["enc"]  = WiFi.encryptionType(i) != ENC_TYPE_NONE;
  }
  WiFi.scanDelete();
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// POST /api/wifi (ssid, pass) -> salva e riavvia
void handleWifi() {
  String s = server.arg("ssid"), p = server.arg("pass");
  if (!s.length() || s.length() > 32 || p.length() > 64) {
    server.send(400, "application/json", F("{\"ok\":false,\"err\":\"ssid non valido\"}"));
    return;
  }
  strcpy(wifiSsid, s.c_str()); strcpy(wifiPass, p.c_str());
  saveConfig();
  Serial.printf("[i] WiFi salvato: '%s'. Riavvio.\n", wifiSsid);
  server.send(200, "application/json", F("{\"ok\":true}"));
  restartPending = true; restartAt = millis() + 1500;   // lascia partire la risposta
}

static bool isAlnumUpper(const String& s, unsigned minLen, unsigned maxLen) {
  if (s.length() < minLen || s.length() > maxLen) return false;
  for (unsigned i = 0; i < s.length(); i++)
    if (!isalnum(s[i])) return false;
  return true;
}

// POST /api/config  (lat, lon, radius) -> salva in EEPROM e ri-polla subito
void handleConfig() {
  if (!server.hasArg("lat") || !server.hasArg("lon")) {
    server.send(400, "application/json", F("{\"ok\":false,\"err\":\"lat/lon mancanti\"}"));
    return;
  }
  float la = server.arg("lat").toFloat();
  float lo = server.arg("lon").toFloat();
  int   r  = server.hasArg("radius") ? server.arg("radius").toInt() : radiusNm;
  if (la < -90 || la > 90 || lo < -180 || lo > 180 || r < 1 || r > 250 ||
      (la == 0 && lo == 0)) {
    server.send(400, "application/json", F("{\"ok\":false,\"err\":\"valori non validi\"}"));
    return;
  }
  homeLat = la; homeLon = lo; radiusNm = r;
  saveConfig();
  for (int i = 0; i < MAX_FLIGHTS; i++) flights[i].active = false;
  lastPoll = millis() - POLL_MS;   // forza un poll al prossimo giro di loop
  Serial.printf("[i] nuova posizione: %.4f %.4f r%d\n", homeLat, homeLon, radiusNm);
  server.send(200, "application/json", F("{\"ok\":true}"));
}

// POST /api/dest (iata, icao, name) -> aeroporto da monitorare
void handleDest() {
  String ia = server.arg("iata"), ic = server.arg("icao"), nm = server.arg("name");
  ia.toUpperCase(); ic.toUpperCase(); nm.toUpperCase(); nm.trim();
  if (!isAlnumUpper(ia, 3, 3) || !isAlnumUpper(ic, 4, 4) || nm.length() > 24) {
    server.send(400, "application/json", F("{\"ok\":false,\"err\":\"IATA 3 lettere, ICAO 4 lettere, nome max 24\"}"));
    return;
  }
  if (!nm.length()) nm = ia;
  strcpy(destIata, ia.c_str()); strcpy(destIcao, ic.c_str()); strcpy(destName, nm.c_str());
  saveConfig();
  // Ricalcola lo stato dei voli gia' in memoria con il nuovo aeroporto
  for (int i = 0; i < MAX_FLIGHTS; i++) if (flights[i].active) {
    flights[i].toDest  = routeToDest(flights[i].route);
    flights[i].visible = isVisible(flights[i]);
  }
  Serial.printf("[i] nuovo aeroporto: %s/%s '%s'\n", destIata, destIcao, destName);
  server.send(200, "application/json", F("{\"ok\":true}"));
}

// GET /update: pagina di upload del solo firmware (la POST la gestisce ESP8266HTTPUpdateServer)
void handleUpdatePage() {
  if (!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication();
  server.send_P(200, "text/html", UPDATEPAGE);
}

void handleNotFound() {
  if (apMode) {   // captive portal: qualunque URL -> pagina di configurazione
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/config");
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "404");
}

// ---------------- OLED ----------------
void oledApPage() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(OLED_WHITE);
  display.setCursor(0, 0);  display.println(F("== CONFIGURA WIFI =="));
  display.setCursor(0, 14); display.println(F("Collegati alla rete"));
  display.setCursor(0, 24); display.print(F("  ")); display.println(AP_SSID);
  display.setCursor(0, 38); display.println(F("poi apri nel browser"));
  display.setCursor(0, 48); display.print(F("http://")); display.println(WiFi.softAPIP());
  display.display();
}

void oledInfoPage() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(OLED_WHITE);
  display.setCursor(0, 0);  display.println(F("====== W A U F ======"));
  display.setCursor(0, 12); display.print(F("IP  ")); display.println(WiFi.localIP());
  display.setCursor(0, 22); display.print(F("http://")); display.print(MDNS_NAME); display.println(F(".local"));
  display.setCursor(0, 32); display.printf("Pos %.3f %.3f", homeLat, homeLon);
  display.setCursor(0, 42); display.printf("Raggio %dnm  %ddBm", radiusNm, WiFi.RSSI());
  int tot = 0, trn = 0, vis = 0;
  for (int i = 0; i < MAX_FLIGHTS; i++)
    if (flights[i].active) { tot++; if (flights[i].toDest) trn++; if (flights[i].visible) vis++; }
  display.setCursor(0, 54); display.printf("Aerei:%d %s:%d Vis:%d", tot, destIata, trn, vis);
  display.display();
}

void oledFlightPage() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(OLED_WHITE);

  // Priorita': VISIBILE (guardare fuori!), poi inbound TRN, poi il piu' vicino
  Flight* f = NULL;
  for (int i = 0; i < MAX_FLIGHTS; i++)
    if (flights[i].active && flights[i].visible) { f = &flights[i]; break; }
  bool vis = (f != NULL);
  if (!f) for (int i = 0; i < MAX_FLIGHTS; i++)
    if (flights[i].active && flights[i].toDest) { f = &flights[i]; break; }
  bool inbound = (f != NULL);
  if (!f) for (int i = 0; i < MAX_FLIGHTS; i++)
    if (flights[i].active) { f = &flights[i]; break; }

  if (!f) {
    display.setCursor(0, 0);  display.println(F("====== W A U F ======"));
    display.setCursor(10, 28); display.println(F("Cielo libero..."));
    display.setCursor(10, 40); display.printf("(raggio %d nm)", radiusNm);
    display.display();
    return;
  }

  display.setCursor(0, 0);
  if (vis)          display.printf("!! IN VISTA -> %s !!", destIata);
  else if (inbound) display.printf(">> IN ARRIVO A %s <<", destIata);
  else              display.print(F("--- IN TRANSITO ---"));
  display.println();
  display.setTextSize(2);
  display.setCursor(0, 12); display.println(f->callsign);
  display.setTextSize(1);
  display.setCursor(0, 32); display.print(f->route); display.print(' '); display.println(f->type);
  display.setCursor(0, 42);
  if (f->altFt < 0) display.print(F("A terra"));
  else display.printf("%dft%c %dkt", f->altFt, f->vRate < -100 ? 'v' : (f->vRate > 100 ? '^' : ' '), f->gsKt);
  display.setCursor(0, 54); display.printf("%.1fkm %s di casa", f->distKm, compass8(f->bearing));
  display.display();
}

// ---------------- WiFi ----------------
void startAP() {
  apMode = true;
  apStarted = millis();
  WiFi.mode(WIFI_AP_STA);            // AP_STA: la scansione reti funziona anche in AP
  WiFi.softAP(AP_SSID);
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.printf("[i] Access point '%s' su %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  oledApPage();
}

bool connectSTA() {
  if (!wifiSsid[0]) return false;
  WiFi.mode(WIFI_STA);
  WiFi.hostname(MDNS_NAME);
  WiFi.begin(wifiSsid, wifiPass);
  display.print(F("WiFi: ")); display.println(wifiSsid); display.display();
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300); Serial.print('.'); display.print('.'); display.display();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    if (millis() - t0 > WIFI_TIMEOUT_MS) {
      Serial.println(F("\n[!] WiFi timeout: apro l'access point di configurazione"));
      return false;
    }
  }
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.printf("\n[i] IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n[i] WAUF boot"));
  bootMillis = millis();

  EEPROM.begin(sizeof(PersistCfg) + 8);
  loadConfig();

  Wire.begin(SDA_PIN, SCL_PIN);
#ifdef OLED_SH1106
  if (!display.begin(OLED_ADDR, true)) {
#else
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
#endif
    Serial.println(F("[!] OLED non trovato"));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(OLED_WHITE);
  display.setCursor(0, 0);
  display.println(F("W A U F"));
  display.println();
  display.display();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);   // LED di bordo attivo basso: spento

  if (!connectSTA()) startAP();

  if (!apMode && MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);

  server.on("/", handleRoot);
  server.on("/config", handleConfigPage);
  server.on("/api/flights", handleApi);
  server.on("/api/status", handleStatus);
  server.on("/api/scan", handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/dest", HTTP_POST, handleDest);
  server.onNotFound(handleNotFound);
  // OTA: pagina /update con upload del .bin (Sketch > Export compiled Binary).
  // La GET e' la nostra pagina (solo firmware, niente filesystem), registrata
  // PRIMA di otaServer.setup() cosi' ha la precedenza; la POST resta alla libreria.
  // Non disponibile in modalita' access point.
  if (!apMode) {
    server.on("/update", HTTP_GET, handleUpdatePage);
    otaServer.setup(&server, "/update", OTA_USER, OTA_PASS);
  }
  server.begin();

  if (!apMode) {
    oledInfoPage();
    fetchAircraft();
    fetchRoutes();
  }
  lastPoll = millis();
}

void loop() {
  server.handleClient();
  if (apMode) dnsServer.processNextRequest(); else MDNS.update();

  if (restartPending && (int32_t)(millis() - restartAt) >= 0) ESP.restart();

  // In AP di emergenza con credenziali salvate: riprova la rete di casa dopo un po'
  if (apMode && wifiSsid[0] && millis() - apStarted > AP_RETRY_MS) ESP.restart();

  if (!apMode && millis() - lastPoll >= POLL_MS) {
    lastPoll = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (fetchAircraft()) fetchRoutes();
    }
  }

  if (!apMode && millis() - lastPageFlip >= OLED_PAGE_MS) {
    lastPageFlip = millis();
    oledPage ^= 1;
    if (oledPage) oledFlightPage(); else oledInfoPage();

    // LED di bordo acceso (attivo basso) se c'e' un aereo IN VISTA
    bool anyVis = false;
    for (int i = 0; i < MAX_FLIGHTS; i++)
      if (flights[i].active && flights[i].visible) { anyVis = true; break; }
    digitalWrite(LED_BUILTIN, anyVis ? LOW : HIGH);
  }
}
