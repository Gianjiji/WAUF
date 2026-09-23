#pragma once
#include <pgmspace.h>

// Pagina /config: WiFi (scansione + password), aeroporto da monitorare,
// posizione su mappa OpenStreetMap via Leaflet (nessuna API key).
// Leaflet e le tile OSM sono caricati dal browser: servono solo se il
// telefono/PC ha internet (in modalita' access point la mappa non compare,
// restano i campi manuali).
static const char CONFIGPAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="it"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WAUF - CONFIGURAZIONE</title>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">
<style>
:root{--amb:#f5b301;--dim:#7a5c07;--bg:#0b0c0e;--cell:#17181c;--grn:#3ddc84;--red:#e05252}
*{box-sizing:border-box;margin:0}
body{background:var(--bg);color:var(--amb);font-family:"Courier New",ui-monospace,monospace;
 min-height:100vh;padding:18px 10px}
header{max-width:900px;margin:0 auto 14px;display:flex;justify-content:space-between;
 align-items:baseline;flex-wrap:wrap;gap:8px;border-bottom:2px solid var(--dim);padding-bottom:10px}
h1{font-size:clamp(17px,4vw,26px);letter-spacing:.18em;font-weight:700}
a{color:var(--dim);text-decoration:none;letter-spacing:.15em;font-size:13px;border:1px solid var(--dim);padding:6px 14px}
section{max-width:900px;margin:0 auto 16px;border:1px dashed var(--dim);padding:14px}
h2{font-size:13px;letter-spacing:.25em;color:var(--dim);margin-bottom:12px;font-weight:400}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-bottom:10px}
label{color:var(--dim);letter-spacing:.1em;font-size:13px}
input,select{background:var(--cell);border:1px solid var(--dim);color:var(--amb);font:inherit;padding:6px 8px}
input.s{width:110px}input.xs{width:70px}input.w{width:220px}select{min-width:220px}
button{background:none;border:1px solid var(--dim);color:var(--amb);font:inherit;
 padding:6px 14px;letter-spacing:.15em;cursor:pointer}
button:disabled{opacity:.4;cursor:default}
.msg{letter-spacing:.15em;font-size:13px}.ok{color:var(--grn)}.err{color:var(--red)}
.hint{color:var(--dim);font-size:11px;letter-spacing:.05em;line-height:1.5}
#map{height:340px;border:1px solid var(--dim);margin:10px 0;background:#111}
#map.off{display:flex;align-items:center;justify-content:center;color:var(--dim);font-size:12px;letter-spacing:.1em;text-align:center;padding:20px}
.st{color:var(--dim);font-size:12px;letter-spacing:.08em}.st b{color:var(--amb);font-weight:400}
.leaflet-container{background:#0b0c0e}
.osm-dark{filter:invert(1) hue-rotate(180deg) brightness(.8) contrast(.95) saturate(.5)}
.leaflet-control-attribution{background:rgba(0,0,0,.6)!important;color:#888!important}
.leaflet-control-attribution a{color:#aaa!important}
.leaflet-bar a{background:#17181c!important;color:#f5b301!important;border-color:#333!important}
</style></head><body>
<header><h1>WAUF &#9881; CONFIGURAZIONE</h1><a href="/">&larr; TABELLONE</a></header>

<section>
<h2>STATO</h2>
<div class="st" id="st">...</div>
</section>

<section>
<h2>RETE WIFI</h2>
<div class="row"><button id="bScan">CERCA RETI</button><span class="msg" id="scanMsg"></span></div>
<div class="row"><label>RETE</label><select id="selSsid"><option value="">-- cerca le reti --</option></select>
 <label>O SCRIVI</label><input class="w" id="iSsid" placeholder="SSID" maxlength="32"></div>
<div class="row"><label>PASSWORD</label><input class="w" id="iPass" type="password" maxlength="64" autocomplete="off">
 <button id="bEye" type="button">&#128065;</button></div>
<div class="row"><button id="bWifi">SALVA E RIAVVIA</button><span class="msg" id="wifiMsg"></span></div>
<p class="hint">Dopo il salvataggio il dispositivo si riavvia e si collega alla rete scelta.
Torna sulla tua rete WiFi e apri <b>http://wauf.local</b> (oppure l'IP mostrato sull'OLED).
Se la connessione fallisce, dopo 20 s riappare la rete <b>WAUF-Setup</b> per riprovare.</p>
</section>

<section>
<h2>AGGIORNAMENTO FIRMWARE (OTA)</h2>
<div class="st" id="ota">...</div>
<div class="row" style="margin-top:10px"><a href="/update" target="_blank">APRI PAGINA DI UPLOAD</a></div>
<p class="hint">Nell'IDE Arduino: Sketch &rarr; Esporta binario compilato, poi carica il file <b>.bin</b>
dalla pagina di upload (utente <b>admin</b>, password impostata nello sketch). Il dispositivo si riavvia da solo.
Non spegnere durante l'aggiornamento. Non disponibile in modalita' access point.</p>
</section>

<section>
<h2>AEROPORTO DA MONITORARE</h2>
<div class="row"><label>IATA</label><input class="xs" id="iIata" maxlength="3" placeholder="TRN">
 <label>ICAO</label><input class="xs" id="iIcao" maxlength="4" placeholder="LIMF">
 <label>NOME</label><input class="w" id="iName" maxlength="24" placeholder="TORINO CASELLE"></div>
<div class="row"><button id="bDest">SALVA AEROPORTO</button><span class="msg" id="destMsg"></span></div>
<p class="hint">Vengono evidenziati i voli la cui rotta termina in questo aeroporto (codice IATA o ICAO).
Esempi: MXP/LIMC Milano Malpensa, LIN/LIML Milano Linate, FCO/LIRF Roma Fiumicino, GOA/LIMJ Genova.</p>
</section>

<section>
<h2>POSIZIONE E RAGGIO</h2>
<div id="map" class="off">Mappa non disponibile (serve internet sul telefono/PC). Inserisci le coordinate a mano.</div>
<div class="row"><label>LAT</label><input class="s" id="iLat" inputmode="decimal">
 <label>LON</label><input class="s" id="iLon" inputmode="decimal">
 <label>RAGGIO NM</label><input class="xs" id="iRad" inputmode="numeric">
 <button id="bGeo" type="button">USA GPS</button></div>
<div class="row"><button id="bPos">SALVA POSIZIONE</button><span class="msg" id="posMsg"></span></div>
<p class="hint">Clicca sulla mappa o trascina il marcatore per impostare casa; il cerchio mostra il raggio.
USA GPS funziona solo su https o su localhost per limiti del browser: su http://ip locale usa la mappa.</p>
</section>

<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<script>
const $=id=>document.getElementById(id);
let map=null,marker=null,circle=null,st=null;
function msg(id,t,ok){const e=$(id);e.textContent=t;e.className='msg '+(ok?'ok':'err')}
function setPos(la,lo){$('iLat').value=(+la).toFixed(5);$('iLon').value=(+lo).toFixed(5);
 if(marker){marker.setLatLng([la,lo]);circle.setLatLng([la,lo])}}
function rad(){return Math.max(1,Math.min(250,parseInt($('iRad').value)||25))*1852}
function initMap(la,lo){
 if(typeof L==='undefined')return;
 const m=$('map');m.className='';m.textContent='';
 map=L.map('map').setView([la,lo],10);
 // Tile OSM standard rese scure via filtro CSS (.osm-dark): nessuna chiave
 L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,className:'osm-dark',attribution:'&copy; OpenStreetMap'}).addTo(map);
 marker=L.marker([la,lo],{draggable:true}).addTo(map);
 circle=L.circle([la,lo],{radius:rad(),color:'#f5b301',weight:1,fillOpacity:.05}).addTo(map);
 marker.on('drag',e=>{const p=e.target.getLatLng();setPos(p.lat,p.lng)});
 map.on('click',e=>setPos(e.latlng.lat,e.latlng.lng));
 $('iRad').oninput=()=>circle.setRadius(rad());
 $('iLat').onchange=$('iLon').onchange=()=>{const la=+$('iLat').value,lo=+$('iLon').value;
  if(isFinite(la)&&isFinite(lo)){setPos(la,lo);map.panTo([la,lo])}};
}
async function load(){
 try{st=await(await fetch('/api/status')).json()}catch(e){$('st').textContent='ERRORE';return}
 $('st').innerHTML='MODO <b>'+(st.mode=='ap'?'ACCESS POINT (prima configurazione)':'CONNESSO')+'</b> &middot; RETE <b>'+
  (st.ssid||'-')+'</b> &middot; IP <b>'+st.ip+'</b>'+(st.mode=='sta'?' &middot; WIFI <b>'+st.rssi+' dBm</b>':'')+
  ' &middot; MDNS <b>http://'+st.mdns+'.local</b>';
 $('iSsid').value=st.ssid||'';
 const kb=n=>Math.round(n/1024)+' KB';
 const mb=n=>(n/1048576).toFixed(n%1048576?1:0)+' MB';
 $('ota').innerHTML='FIRMWARE <b>v'+(st.fw||'?')+'</b> &middot; SKETCH <b>'+kb(st.sketch||0)+'</b> &middot; SPAZIO LIBERO PER OTA <b>'+kb(st.free||0)+'</b>'+
  ' &middot; FLASH <b>'+mb(st.flash||0)+'</b>'+(st.flashCfg&&st.flash&&st.flashCfg!=st.flash?' (IDE: '+mb(st.flashCfg)+')':'')+
  (st.free&&st.sketch&&st.free<st.sketch?'<br><span class="err">SPAZIO INSUFFICIENTE: nell\'IDE scegli Flash Size "4MB (FS:none OTA:~1019KB)" o "FS:1MB" e ricarica via USB</span>':'');
 $('iIata').value=st.iata;$('iIcao').value=st.icao;$('iName').value=st.name;
 $('iLat').value=st.lat;$('iLon').value=st.lon;$('iRad').value=st.radius;
 initMap(+st.lat,+st.lon);
}
$('bScan').onclick=async()=>{
 $('bScan').disabled=true;msg('scanMsg','SCANSIONE IN CORSO...',true);
 try{const nets=await(await fetch('/api/scan')).json();
  nets.sort((a,b)=>b.rssi-a.rssi);
  const sel=$('selSsid');sel.innerHTML='<option value="">-- scegli --</option>';
  for(const n of nets){const o=document.createElement('option');o.value=n.ssid;
   o.textContent=n.ssid+'  ('+n.rssi+' dBm'+(n.enc?', protetta':', aperta')+')';sel.appendChild(o)}
  msg('scanMsg',nets.length+' RETI TROVATE',true);
 }catch(e){msg('scanMsg','ERRORE SCANSIONE',false)}
 $('bScan').disabled=false;
};
$('selSsid').onchange=()=>{if($('selSsid').value)$('iSsid').value=$('selSsid').value};
$('bEye').onclick=()=>{const p=$('iPass');p.type=p.type=='password'?'text':'password'};
$('bWifi').onclick=async()=>{
 const ssid=$('iSsid').value.trim();if(!ssid){msg('wifiMsg','INSERISCI LA RETE',false);return}
 $('bWifi').disabled=true;msg('wifiMsg','SALVATAGGIO...',true);
 try{const r=await(await fetch('/api/wifi',{method:'POST',body:new URLSearchParams({ssid,pass:$('iPass').value})})).json();
  msg('wifiMsg',r.ok?'SALVATO. RIAVVIO IN CORSO: RICOLLEGATI ALLA TUA RETE':'ERRORE: '+r.err,r.ok);
 }catch(e){msg('wifiMsg','ERRORE DI RETE',false)}
 $('bWifi').disabled=false;
};
$('bDest').onclick=async()=>{
 const p=new URLSearchParams({iata:$('iIata').value.trim(),icao:$('iIcao').value.trim(),name:$('iName').value.trim()});
 try{const r=await(await fetch('/api/dest',{method:'POST',body:p})).json();
  msg('destMsg',r.ok?'SALVATO':'ERRORE: '+r.err,r.ok);
 }catch(e){msg('destMsg','ERRORE DI RETE',false)}
};
$('bPos').onclick=async()=>{
 const p=new URLSearchParams({lat:$('iLat').value.replace(',','.'),lon:$('iLon').value.replace(',','.'),radius:$('iRad').value});
 try{const r=await(await fetch('/api/config',{method:'POST',body:p})).json();
  msg('posMsg',r.ok?'SALVATO':'VALORI NON VALIDI',r.ok);
 }catch(e){msg('posMsg','ERRORE DI RETE',false)}
};
$('bGeo').onclick=()=>{
 if(!navigator.geolocation){msg('posMsg','GPS NON DISPONIBILE',false);return}
 navigator.geolocation.getCurrentPosition(p=>{setPos(p.coords.latitude,p.coords.longitude);
  if(map)map.panTo([p.coords.latitude,p.coords.longitude]);msg('posMsg','POSIZIONE GPS LETTA',true)},
  ()=>msg('posMsg','GPS NEGATO (serve https): usa la mappa',false));
};
load();
</script></body></html>)rawliteral";
