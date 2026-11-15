"""Le pagine servite dal dispositivo, provate in un browser vero.

Le pagine sono stringhe dentro gli header C (webpage.h, configpage.h): qui
vengono estratte e aperte in Chromium, con le API del dispositivo simulate.
Si verifica cio' che su una scheda non si riesce a controllare con comodo:
che un nome di rete o un callsign ostile resti testo, che "mai aggiornato" si
legga MAI, che un 401 venga spiegato.

    pip install pytest playwright && playwright install chromium
    pytest tests/test_pages.py

Senza Playwright (o senza browser) i test vengono saltati.
"""
import json
import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
BASE = "http://wauf.test"
PWN = "window.__pwned=(window.__pwned||[]).concat('%s')"


def page_source(header: str) -> str:
    """L'HTML contenuto in un header: quello fra R"rawliteral( e )rawliteral"."""
    text = (ROOT / header).read_text(encoding="utf-8")
    match = re.search(r'R"rawliteral\((.*)\)rawliteral"', text, re.S)
    assert match, f"{header}: letterale non trovato"
    return match.group(1)


def hostile(tag: str) -> str:
    return '<img src=x onerror="%s">' % (PWN % tag)


FLIGHTS = {
    "dest": "TRN", "destName": "TORINO CASELLE", "lat": "45.0703", "lon": "7.6869",
    "radius": 25, "rssi": -61, "heap": 30000, "uptime": 120, "polls": 4, "age": 12,
    "flights": [
        {"cs": "RYR123", "hex": "4ca123", "reg": "EI-DWL", "typ": "B738", "al": "RYR", "route": "FCO-TRN",
         "alt": 5200, "gs": 210, "vr": -900, "trk": 320, "lat": "45.1500", "lon": "7.7000",
         "dist": "9.4", "dir": "NE", "trn": True, "vis": True},
        {"cs": hostile("cs"), "hex": hostile("hex"), "reg": hostile("reg"), "typ": hostile("typ"),
         "al": '"><', "route": hostile("route"), "alt": 31000, "gs": 440, "vr": 0, "trk": 90,
         "lat": "45.3000", "lon": "7.9000", "dist": "30.1", "dir": hostile("dir"), "trn": False, "vis": False},
        {"cs": "X'\"><b>Y", "hex": "", "reg": "", "typ": "", "al": "javascript:alert()", "route": "?",
         "alt": -1, "gs": 0, "vr": 0, "trk": -1, "lat": "45.1000", "lon": "7.6000",
         "dist": "4.0", "dir": "S", "trn": True, "vis": False},
    ],
}

STATUS = {
    "mode": "sta", "ssid": hostile("ssid"), "ip": "192.168.1.77", "rssi": -58,
    "lat": "45.07030", "lon": "7.68690", "radius": 25, "mdns": "wauf",
    "iata": "TRN", "icao": "LIMF", "name": "TORINO CASELLE", "fw": "1.2.0",
    "sketch": 482416, "free": 1500000, "flash": 4194304, "flashCfg": 4194304,
}


@pytest.fixture(scope="module")
def browser():
    sync_api = pytest.importorskip("playwright.sync_api")
    with sync_api.sync_playwright() as playwright:
        try:
            instance = playwright.chromium.launch()
        except Exception as error:
            pytest.skip("Chromium non avviabile: %s" % str(error).splitlines()[0])
        yield instance
        instance.close()


class Device:
    """Il dispositivo finto: risponde alle stesse URL della scheda."""

    def __init__(self, browser, flights=None, status=None, post_status=200, scan=None):
        self.flights = FLIGHTS if flights is None else flights
        self.status = STATUS if status is None else status
        self.post_status = post_status
        self.scan = scan if scan is not None else [
            {"ssid": "Casa", "rssi": -40, "enc": True},
            {"ssid": hostile("scan"), "rssi": -70, "enc": False}]
        self.posts, self.external, self.dialogs = [], [], []
        self.page = browser.new_page()
        self.page.on("dialog", lambda d: (self.dialogs.append(d.message), d.dismiss()))
        self.page.route("**/*", self._answer)

    def _answer(self, route):
        request = route.request
        url = request.url
        if not url.startswith(BASE):
            self.external.append(url)
            return route.abort()                      # niente rete: le pagine devono reggere
        path = url[len(BASE):].split("?")[0]
        if request.method == "POST":
            self.posts.append((path, request.post_data))
            if self.post_status == 401:
                return route.fulfill(status=401, headers={"WWW-Authenticate": 'Basic realm="x"'},
                                     body="401 Unauthorized")
            return route.fulfill(status=200, content_type="application/json", body='{"ok":true}')
        pages = {"/": "webpage.h", "/config": "configpage.h"}
        if path in pages:
            return route.fulfill(status=200, content_type="text/html; charset=utf-8",
                                 body=page_source(pages[path]))
        api = {"/api/flights": self.flights, "/api/status": self.status, "/api/scan": self.scan}
        if path in api:
            return route.fulfill(status=200, content_type="application/json", body=json.dumps(api[path]))
        return route.fulfill(status=404, body="404")

    def open(self, path):
        self.page.goto(BASE + path, wait_until="load")
        self.page.wait_for_timeout(400)
        return self

    def pwned(self):
        return self.page.evaluate("() => window.__pwned || null")

    def text(self, selector="body"):
        """Il testo visibile, con gli spazi normalizzati."""
        return " ".join(self.page.inner_text(selector).split())

    def close(self):
        self.page.close()


@pytest.fixture
def device(browser):
    made = []

    def make(**kwargs):
        made.append(Device(browser, **kwargs))
        return made[-1]

    yield make
    for d in made:
        d.close()


# --- tabellone --------------------------------------------------------------
def test_board_shows_flights(device):
    d = device().open("/")
    board = d.text("#tb")
    assert "RYR123" in board.replace(" ", "")
    assert "IN VISTA" in board
    assert "TORINO CASELLE" in d.text("#ttl")
    assert d.page.title() == "WAUF - TORINO CASELLE"


def test_hostile_flight_data_stays_text(device):
    d = device().open("/")
    d.page.click("#bAll")                              # mostra anche i voli non diretti qui
    d.page.wait_for_timeout(200)
    assert d.pwned() is None
    assert d.dialogs == []
    injected = d.page.evaluate("""() => [...document.querySelectorAll('#tb *, #meta *')]
        .filter(e => [...e.attributes].some(a => a.name.startsWith('on') && a.value.includes('__pwned'))).length""")
    assert injected == 0
    assert d.page.locator("#tb img[src='x']").count() == 0
    assert d.page.locator("#tb tr").count() == 3


def test_airline_logo_only_for_a_real_code(device):
    d = device().open("/")
    d.page.click("#bAll")
    d.page.wait_for_timeout(200)
    # Qui la rete e' bloccata, quindi l'immagine fallisce e la pagina la sostituisce con la
    # sigla: conta quali loghi sono stati *richiesti*. Solo quello della compagnia vera.
    logos = sorted({url for url in d.external if "airline_logos" in url})
    assert logos == ["https://www.flightaware.com/images/airline_logos/90p/RYR.png"]
    assert d.page.locator("#tb td.lg span", has_text="RYR").count() == 1


def test_tooltips_are_escaped(device):
    d = device().open("/")
    d.page.click("#bAll")
    d.page.wait_for_timeout(200)
    titles = d.page.evaluate("() => [...document.querySelectorAll('#tb tr')].map(r => r.getAttribute('title'))")
    assert any("<img" in t for t in titles), "il testo ostile deve restare nell'attributo, intero"
    assert d.pwned() is None


@pytest.mark.parametrize("age, expected", [(-1, "MAI"), (0, "0 S FA"), (12, "12 S FA")])
def test_age_label(device, age, expected):
    d = device(flights={**FLIGHTS, "age": age}).open("/")
    assert "AGG. " + expected in d.text("#meta")


def test_never_updated_is_not_a_huge_number(device):
    """Il firmware mandava 4294967295 al posto di -1 e la pagina non lo riconosceva."""
    d = device(flights={**FLIGHTS, "age": -1}).open("/")
    assert "4294967295" not in d.text("#meta")


def test_empty_sky(device):
    d = device(flights={**FLIGHTS, "flights": []}).open("/")
    assert d.page.is_visible("#empty")


def test_board_survives_without_the_map_library(device):
    """Leaflet arriva da una CDN: senza rete il tabellone deve funzionare lo stesso."""
    d = device().open("/")
    assert any("unpkg.com/leaflet" in url for url in d.external)
    d.page.click("#bMap")
    d.page.wait_for_timeout(200)
    assert d.page.locator("#tb tr").count() >= 1
    assert d.pwned() is None


# --- configurazione ---------------------------------------------------------
def test_hostile_ssid_stays_text(device):
    """Il nome della rete finiva nella pagina come HTML: bastava salvarne uno ostile."""
    d = device().open("/config")
    assert d.pwned() is None
    assert d.dialogs == []
    assert d.page.locator("#st img").count() == 0
    assert '<img src=x onerror=' in d.text("#st")       # visibile, come testo
    assert d.page.input_value("#iSsid") == STATUS["ssid"]


def test_status_fields_are_escaped(device):
    status = {**STATUS, "ssid": "Casa", "ip": hostile("ip"), "mdns": hostile("mdns"),
              "fw": hostile("fw"), "rssi": hostile("rssi")}
    d = device(status=status).open("/config")
    assert d.pwned() is None
    assert d.page.locator("#st img, #ota img").count() == 0


def test_scan_results_are_text(device):
    d = device().open("/config")
    d.page.click("#bScan")
    d.page.wait_for_timeout(300)
    assert d.pwned() is None
    assert d.page.locator("#selSsid option").count() == 3
    assert d.page.locator("#selSsid img").count() == 0


def test_saving_sends_the_form(device):
    d = device().open("/config")
    d.page.fill("#iIata", "MXP")
    d.page.fill("#iIcao", "LIMC")
    d.page.fill("#iName", "MALPENSA")
    d.page.click("#bDest")
    d.page.wait_for_timeout(300)
    assert ("/api/dest", "iata=MXP&icao=LIMC&name=MALPENSA") in d.posts
    assert "SALVATO" in d.text("#destMsg")


@pytest.mark.parametrize("button, message", [("#bDest", "#destMsg"), ("#bPos", "#posMsg"), ("#bWifi", "#wifiMsg")])
def test_unauthorised_change_is_explained(device, button, message):
    """Senza credenziali il dispositivo risponde 401: la pagina deve dirlo, non tacere."""
    d = device(post_status=401, status={**STATUS, "ssid": "Casa"}).open("/config")
    d.page.click(button)
    d.page.wait_for_timeout(300)
    assert "ACCESSO NEGATO" in d.text(message)
    assert d.page.locator(message + ".err").count() == 1


# --- sorgente ---------------------------------------------------------------
@pytest.mark.parametrize("header", ["webpage.h", "configpage.h"])
def test_cdn_resources_are_pinned(header):
    html = page_source(header)
    tags = re.findall(r'<(?:script|link)\b[^>]*https://[^>]*>', html)
    assert len(tags) == 2
    for tag in tags:
        assert "leaflet@1.9.4" in tag, tag
        assert re.search(r'integrity="sha(256|384|512)-[A-Za-z0-9+/=]+"', tag), tag
        assert "crossorigin" in tag, tag


@pytest.mark.parametrize("header", ["webpage.h", "configpage.h"])
def test_pages_fit_in_a_raw_literal(header):
    """Una sequenza )rawliteral" dentro la pagina chiuderebbe la stringa C in anticipo."""
    text = (ROOT / header).read_text(encoding="utf-8")
    assert text.count('R"rawliteral(') == 1 and text.count(')rawliteral"') == 1
