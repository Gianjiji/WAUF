/*
 * WAUF - logica pura, senza dipendenze da Arduino.
 *
 * Qui sta tutto cio' che e' calcolo e decisione: distanze, rotte, soglie,
 * tempi. Non tocca WiFi, display ne' rete, quindi si compila e si prova anche
 * su un PC (vedi tests/). Lo sketch include questo file e ne usa le funzioni:
 * una regola corretta qui e' corretta anche sul dispositivo.
 */
#ifndef WAUF_LOGIC_H
#define WAUF_LOGIC_H

#include <math.h>
#include <stdint.h>
#include <string.h>

namespace wauf {

inline float deg2rad(float d) { return d * 0.017453292519943f; }

// Distanza in km fra due punti sulla sfera (formula dell'emisenoverso).
inline float haversineKm(float lat1, float lon1, float lat2, float lon2) {
  float dLat = deg2rad(lat2 - lat1), dLon = deg2rad(lon2 - lon1);
  float a = sinf(dLat / 2) * sinf(dLat / 2) +
            cosf(deg2rad(lat1)) * cosf(deg2rad(lat2)) * sinf(dLon / 2) * sinf(dLon / 2);
  return 6371.0f * 2 * atan2f(sqrtf(a), sqrtf(1 - a));
}

// Direzione iniziale da (lat1,lon1) verso (lat2,lon2), in gradi 0..359.
inline int bearingDeg(float lat1, float lon1, float lat2, float lon2) {
  float dLon = deg2rad(lon2 - lon1);
  float y = sinf(dLon) * cosf(deg2rad(lat2));
  float x = cosf(deg2rad(lat1)) * sinf(deg2rad(lat2)) -
            sinf(deg2rad(lat1)) * cosf(deg2rad(lat2)) * cosf(dLon);
  int b = (int)(atan2f(y, x) * 57.29578f);
  return (b + 360) % 360;
}

// Punto cardinale (otto settori, sigle italiane) per una direzione in gradi.
inline const char* compass8(int deg) {
  static const char* pts[] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
  deg %= 360;
  if (deg < 0) deg += 360;            // una direzione negativa non deve uscire dalla tabella
  return pts[((deg + 22) % 360) / 45];
}

inline void trimRight(char* s) {
  int n = (int)strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
}

// La rotta ("FCO-TRN", "LHR-MXP-TRN") arriva all'aeroporto indicato?
// Conta ogni scalo dopo il primo: il primo e' la partenza, non una destinazione.
inline bool routeReaches(const char* route, const char* iata, const char* icao) {
  if (!route || !route[0] || route[0] == '?') return false;
  char buf[20];
  strncpy(buf, route, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  bool first = true;
  for (char* tok = strtok(buf, "-"); tok; tok = strtok(NULL, "-")) {
    if (!first && ((iata && iata[0] && strcmp(tok, iata) == 0) ||
                   (icao && icao[0] && strcmp(tok, icao) == 0)))
      return true;
    first = false;
  }
  return false;
}

// "Visibile a occhio nudo": diretto all'aeroporto, in volo, in discesa e
// sotto la quota soglia.
inline bool visibleToEye(bool toDest, int altFt, int vRate, int maxAltFt, int maxVRate) {
  return toDest && altFt > 0 && altFt < maxAltFt && vRate <= maxVRate;
}

// Millisecondi trascorsi fra due letture di millis(), corretti anche quando
// il contatore a 32 bit riparte da zero (ogni 49,7 giorni).
inline uint32_t elapsedMs(uint32_t now, uint32_t since) { return now - since; }

// Un avvistamento e' scaduto se piu' vecchio di staleMs.
inline bool isStale(uint32_t now, uint32_t seenAt, uint32_t staleMs) {
  return elapsedMs(now, seenAt) > staleMs;
}

// Secondi dall'ultimo aggiornamento riuscito, oppure -1 se non ce n'e' mai
// stato uno. Il risultato ha il segno: scritto come
//   lastOk ? (now - lastOk) / 1000 : -1
// in aritmetica senza segno il -1 diventava 4294967295 e la pagina, che
// mostra "MAI" quando il valore e' negativo, non lo riconosceva.
inline int32_t ageSeconds(uint32_t now, uint32_t lastOk) {
  if (!lastOk) return -1;
  return (int32_t)(elapsedMs(now, lastOk) / 1000UL);
}

// Tre o quattro caratteri alfanumerici: la forma di un codice IATA o ICAO.
inline bool isAlnumCode(const char* s, unsigned minLen, unsigned maxLen) {
  if (!s) return false;
  size_t n = strlen(s);
  if (n < minLen || n > maxLen) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) return false;
  }
  return true;
}

// Posizione e raggio accettabili per la configurazione. (0,0) e' rifiutato:
// e' cio' che si ottiene da un campo vuoto, non un posto dove abita qualcuno.
inline bool validHome(float lat, float lon, int radiusNm) {
  if (!(lat >= -90.0f && lat <= 90.0f) || !(lon >= -180.0f && lon <= 180.0f)) return false;  // anche NaN
  if (radiusNm < 1 || radiusNm > 250) return false;
  return !(lat == 0.0f && lon == 0.0f);
}

}  // namespace wauf

#endif  // WAUF_LOGIC_H
