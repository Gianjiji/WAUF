// Test su PC della logica di WAUF (wauf_logic.h): nessuna scheda, nessuna libreria.
//
//   g++ -std=c++11 -Wall -Wextra -o test_logic test_logic.cpp && ./test_logic
//   (oppure: make -C tests)
//
// Esce con codice 0 se tutto passa, 1 al primo gruppo di errori.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../wauf_logic.h"

static int failures = 0, checks = 0;

#define CHECK(cond)                                                              \
  do {                                                                           \
    ++checks;                                                                    \
    if (!(cond)) { ++failures; std::printf("FALLITO %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

#define CHECK_NEAR(value, expected, tolerance)                                   \
  do {                                                                           \
    ++checks;                                                                    \
    double v_ = (value), e_ = (expected);                                        \
    if (std::fabs(v_ - e_) > (tolerance)) {                                      \
      ++failures;                                                                \
      std::printf("FALLITO %s:%d  %s = %.4f, atteso %.4f (+/- %g)\n", __FILE__, __LINE__, #value, v_, e_, (double)(tolerance)); \
    }                                                                            \
  } while (0)

#define CHECK_STR(value, expected)                                               \
  do {                                                                           \
    ++checks;                                                                    \
    if (std::strcmp((value), (expected)) != 0) {                                 \
      ++failures;                                                                \
      std::printf("FALLITO %s:%d  %s = \"%s\", atteso \"%s\"\n", __FILE__, __LINE__, #value, (value), (expected)); \
    }                                                                            \
  } while (0)

using namespace wauf;

// Torino (Piazza Castello), Milano (Duomo), Roma (Colosseo), aeroporto di Caselle.
static const float TO_LAT = 45.0703f, TO_LON = 7.6869f;
static const float MI_LAT = 45.4642f, MI_LON = 9.1900f;
static const float RM_LAT = 41.8902f, RM_LON = 12.4922f;
static const float TRN_LAT = 45.2008f, TRN_LON = 7.6497f;

static void test_distanze() {
  CHECK_NEAR(haversineKm(TO_LAT, TO_LON, TO_LAT, TO_LON), 0.0, 0.01);
  CHECK_NEAR(haversineKm(TO_LAT, TO_LON, MI_LAT, MI_LON), 126.0, 2.0);     // Torino-Milano ~126 km
  CHECK_NEAR(haversineKm(TO_LAT, TO_LON, RM_LAT, RM_LON), 524.0, 5.0);     // Torino-Roma ~524 km
  CHECK_NEAR(haversineKm(TO_LAT, TO_LON, TRN_LAT, TRN_LON), 14.8, 0.5);    // centro-Caselle ~15 km
  // simmetrica
  CHECK_NEAR(haversineKm(MI_LAT, MI_LON, TO_LAT, TO_LON), haversineKm(TO_LAT, TO_LON, MI_LAT, MI_LON), 0.01);
  // un grado di latitudine sono circa 111 km, ovunque
  CHECK_NEAR(haversineKm(0, 0, 1, 0), 111.2, 0.3);
  CHECK_NEAR(haversineKm(60, 10, 61, 10), 111.2, 0.3);
  // un grado di longitudine a 60 gradi nord e' la meta'
  CHECK_NEAR(haversineKm(60, 10, 60, 11), 55.6, 0.3);
  // attraverso l'antimeridiano: 179 E -> 179 O sono 2 gradi, non 358
  CHECK_NEAR(haversineKm(0, 179, 0, -179), 222.4, 0.6);
  // antipodi: mezza circonferenza
  CHECK_NEAR(haversineKm(0, 0, 0, 180), 20015.0, 10.0);
}

static void test_direzioni() {
  CHECK(bearingDeg(TO_LAT, TO_LON, TO_LAT + 1, TO_LON) == 0);              // nord
  CHECK_NEAR(bearingDeg(TO_LAT, TO_LON, TO_LAT, TO_LON + 1), 90, 1);       // est
  CHECK(bearingDeg(TO_LAT, TO_LON, TO_LAT - 1, TO_LON) == 180);            // sud
  CHECK_NEAR(bearingDeg(TO_LAT, TO_LON, TO_LAT, TO_LON - 1), 270, 1);      // ovest
  CHECK_NEAR(bearingDeg(TO_LAT, TO_LON, MI_LAT, MI_LON), 69, 2);           // Milano e' a est-nord-est
  CHECK_NEAR(bearingDeg(TO_LAT, TO_LON, RM_LAT, RM_LON), 130, 3);          // Roma a sud-est
  for (int lat = -80; lat <= 80; lat += 20)
    for (int lon = -170; lon <= 170; lon += 35) {
      int b = bearingDeg(TO_LAT, TO_LON, (float)lat, (float)lon);
      CHECK(b >= 0 && b < 360);
    }
}

static void test_punti_cardinali() {
  CHECK_STR(compass8(0), "N");    CHECK_STR(compass8(45), "NE");  CHECK_STR(compass8(90), "E");
  CHECK_STR(compass8(135), "SE"); CHECK_STR(compass8(180), "S");  CHECK_STR(compass8(225), "SO");
  CHECK_STR(compass8(270), "O");  CHECK_STR(compass8(315), "NO"); CHECK_STR(compass8(359), "N");
  // confini dei settori (45 gradi l'uno, centrati sul punto cardinale)
  CHECK_STR(compass8(22), "N");   CHECK_STR(compass8(23), "NE");
  CHECK_STR(compass8(67), "NE");  CHECK_STR(compass8(68), "E");
  CHECK_STR(compass8(337), "NO"); CHECK_STR(compass8(338), "N");
  // fuori intervallo: non deve mai leggere oltre la tabella
  CHECK_STR(compass8(360), "N");  CHECK_STR(compass8(450), "E");  CHECK_STR(compass8(720), "N");
  CHECK_STR(compass8(-1), "N");   CHECK_STR(compass8(-90), "O");  CHECK_STR(compass8(-360), "N");
  for (int d = -1000; d <= 1000; d++) CHECK(compass8(d) != NULL && std::strlen(compass8(d)) <= 2);
}

static void test_trim() {
  char a[] = "RYR123  ";  trimRight(a);  CHECK_STR(a, "RYR123");
  char b[] = "AZA1\r\n";  trimRight(b);  CHECK_STR(b, "AZA1");
  char c[] = "   ";       trimRight(c);  CHECK_STR(c, "");
  char d[] = "";          trimRight(d);  CHECK_STR(d, "");
  char e[] = " X Y ";     trimRight(e);  CHECK_STR(e, " X Y");            // solo a destra
}

static void test_rotte() {
  CHECK(routeReaches("FCO-TRN", "TRN", "LIMF"));
  CHECK(routeReaches("LIRF-LIMF", "TRN", "LIMF"));                        // codici ICAO
  CHECK(routeReaches("LHR-MXP-TRN", "TRN", "LIMF"));                      // con scalo
  CHECK(routeReaches("LHR-TRN-FCO", "TRN", "LIMF"));                      // scalo intermedio
  CHECK(!routeReaches("TRN-FCO", "TRN", "LIMF"));                         // parte da qui: non arriva
  CHECK(!routeReaches("FCO-MXP", "TRN", "LIMF"));
  CHECK(!routeReaches("?", "TRN", "LIMF"));
  CHECK(!routeReaches("", "TRN", "LIMF"));
  CHECK(!routeReaches(NULL, "TRN", "LIMF"));
  CHECK(!routeReaches("TRN", "TRN", "LIMF"));                             // un solo scalo
  CHECK(!routeReaches("FCO-TRNX", "TRN", "LIMF"));                        // niente corrispondenze parziali
  CHECK(!routeReaches("FCO-XTRN", "TRN", "LIMF"));
  CHECK(!routeReaches("FCO-trn", "TRN", "LIMF"));                         // maiuscole/minuscole contano
  CHECK(!routeReaches("FCO-MXP", "", ""));                                // aeroporto non configurato
  CHECK(!routeReaches("FCO-", "", ""));
  CHECK(routeReaches("FCO--TRN", "TRN", "LIMF"));                         // trattini doppi tollerati
  // rotta piu' lunga del buffer: non deve scrivere fuori, e non inventa arrivi
  CHECK(!routeReaches("AAA-BBB-CCC-DDD-EEE-FFF-GGG-TRN", "TRN", "LIMF"));
  // la stringa passata non viene modificata
  const char* literal = "FCO-TRN";
  CHECK(routeReaches(literal, "TRN", "LIMF"));
  CHECK_STR(literal, "FCO-TRN");
}

static void test_visibilita() {
  const int ALT = 8000, VR = -300;
  CHECK(visibleToEye(true, 5000, -800, ALT, VR));
  CHECK(visibleToEye(true, 7999, -300, ALT, VR));                         // ai limiti, dentro
  CHECK(!visibleToEye(true, 8000, -800, ALT, VR));                        // quota: soglia esclusa
  CHECK(!visibleToEye(true, 5000, -299, ALT, VR));                        // scende troppo piano
  CHECK(!visibleToEye(true, 5000, 0, ALT, VR));                           // in volo livellato
  CHECK(!visibleToEye(true, 5000, 900, ALT, VR));                         // in salita
  CHECK(!visibleToEye(false, 5000, -800, ALT, VR));                       // non viene qui
  CHECK(!visibleToEye(true, -1, -800, ALT, VR));                          // a terra / quota ignota
  CHECK(!visibleToEye(true, 0, -800, ALT, VR));
}

static void test_tempi() {
  CHECK(elapsedMs(1000, 400) == 600);
  CHECK(elapsedMs(5, 0xFFFFFFF0u) == 21);                                 // millis() ripartito da zero
  CHECK(!isStale(100000, 20000, 90000));                                  // 80 s
  CHECK(!isStale(110000, 20000, 90000));                                  // 90 s esatti: ancora valido
  CHECK(isStale(110001, 20000, 90000));
  CHECK(!isStale(10, 0xFFFFFFF0u, 90000));                                // a cavallo del riavvolgimento
  CHECK(isStale(90100, 0xFFFFFFF0u, 90000));

  // il difetto originale: "mai aggiornato" deve essere -1, non 4294967295
  CHECK(ageSeconds(123456, 0) == -1);
  CHECK(ageSeconds(123456, 0) < 0);
  CHECK(ageSeconds(61000, 1000) == 60);
  CHECK(ageSeconds(1999, 1000) == 0);                                     // secondi interi, per difetto
  CHECK(ageSeconds(1000, 1000) == 0);
  CHECK(ageSeconds(2000, 0xFFFFFFFFu - 2999) == 5);                       // a cavallo del riavvolgimento
  CHECK(ageSeconds(0xFFFFFFFFu, 1) >= 0);                                 // 49 giorni: resta positivo
}

static void test_validazione() {
  CHECK(isAlnumCode("TRN", 3, 3));   CHECK(isAlnumCode("LIMF", 4, 4));  CHECK(isAlnumCode("A1B", 3, 3));
  CHECK(!isAlnumCode("TR", 3, 3));   CHECK(!isAlnumCode("TRNX", 3, 3)); CHECK(!isAlnumCode("", 3, 3));
  CHECK(!isAlnumCode("T N", 3, 3));  CHECK(!isAlnumCode("T-N", 3, 3));  CHECK(!isAlnumCode("T<N", 3, 3));
  CHECK(!isAlnumCode("T\"N", 3, 3)); CHECK(!isAlnumCode(NULL, 3, 3));
  CHECK(!isAlnumCode("T\xC3\xA8", 3, 3));                                 // lettere accentate (byte > 127)

  CHECK(validHome(45.07f, 7.69f, 25));
  CHECK(validHome(-90, -180, 1));     CHECK(validHome(90, 180, 250));     // estremi inclusi
  CHECK(validHome(0, 7.69f, 25));     CHECK(validHome(45.07f, 0, 25));    // un solo zero va bene
  CHECK(!validHome(0, 0, 25));                                            // campo vuoto
  CHECK(!validHome(90.01f, 7, 25));   CHECK(!validHome(-90.01f, 7, 25));
  CHECK(!validHome(45, 180.01f, 25)); CHECK(!validHome(45, -180.01f, 25));
  CHECK(!validHome(45, 7, 0));        CHECK(!validHome(45, 7, 251));      CHECK(!validHome(45, 7, -5));
  CHECK(!validHome(NAN, 7, 25));      CHECK(!validHome(45, NAN, 25));     // "nan" passava i confronti diretti
  CHECK(!validHome(INFINITY, 7, 25)); CHECK(!validHome(45, -INFINITY, 25));
}

int main() {
  test_distanze();
  test_direzioni();
  test_punti_cardinali();
  test_trim();
  test_rotte();
  test_visibilita();
  test_tempi();
  test_validazione();
  std::printf("%d controlli, %d falliti\n", checks, failures);
  return failures ? 1 : 0;
}
