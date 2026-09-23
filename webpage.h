#pragma once
#include <pgmspace.h>

// Tabellone stile aeroporto: ambra su nero, celle a "palette" split-flap.
// Logo compagnia: immagine caricata dal browser (FlightAware, per codice ICAO),
// il device invia solo il codice. Configurazione in /config.
static const char WEBPAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="it"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WAUF</title>
<style>
:root{--amb:#f5b301;--dim:#7a5c07;--bg:#0b0c0e;--cell:#17181c;--grn:#3ddc84}
*{box-sizing:border-box;margin:0}
body{background:var(--bg);color:var(--amb);font-family:"Courier New",ui-monospace,monospace;
 min-height:100vh;padding:18px 10px}
header{max-width:1040px;margin:0 auto 14px;display:flex;justify-content:space-between;
 align-items:baseline;flex-wrap:wrap;gap:8px;border-bottom:2px solid var(--dim);padding-bottom:10px}
h1{font-size:clamp(17px,4vw,26px);letter-spacing:.18em;font-weight:700}
#clock{font-size:clamp(17px,4vw,26px);letter-spacing:.1em}
#meta{max-width:1040px;margin:0 auto 16px;color:var(--dim);font-size:12px;letter-spacing:.08em;
 display:flex;gap:16px;flex-wrap:wrap}
#meta b{color:var(--amb);font-weight:400}
/* Il tabellone deve stare nella larghezza disponibile senza scorrimento orizzontale:
   le tessere scalano con la larghezza della finestra e le colonne meno importanti
   spariscono progressivamente (vedi media query in fondo). */
.board{max-width:1040px;margin:0 auto;border:2px solid #26272b;background:#101114;padding:10px}
table{width:100%;border-collapse:separate;border-spacing:0 6px}
th{color:var(--dim);font-size:11px;letter-spacing:.2em;text-align:left;padding:2px 6px;font-weight:400}
td{padding:2px 6px;white-space:nowrap;vertical-align:middle}
td.lg,th.lg{width:56px;padding:0 4px}
td.lg img{height:22px;max-width:52px;object-fit:contain;background:#fff;border-radius:3px;padding:2px;display:block}
td.lg span{display:inline-block;font-size:11px;color:var(--dim);letter-spacing:.1em}
.t{display:inline-flex;gap:2px}
.t span{display:inline-block;width:1.05em;text-align:center;background:var(--cell);
 border-radius:2px;padding:3px 0;font-size:clamp(11px,1.45vw,15px);
 background-image:linear-gradient(#1d1e23 49%,#101114 51%);box-shadow:0 1px 0 #000}
tr.flip .t span{animation:fl .45s ease}
@keyframes fl{0%{transform:rotateX(90deg)}100%{transform:rotateX(0)}}
tr.trn .t span,tr.vis .t span{color:var(--grn)}
.st{font-size:clamp(10px,1.2vw,12px);letter-spacing:.12em}
tr.trn .st{color:var(--grn)}
tr.vis .st{color:var(--grn);animation:bl .7s step-end infinite}
@keyframes bl{50%{opacity:.15}}
#empty{padding:30px 8px;color:var(--dim);letter-spacing:.15em;text-align:center}
#ctl{max-width:1040px;margin:14px auto 0;display:flex;gap:10px;flex-wrap:wrap}
button,a.btn{background:none;border:1px solid var(--dim);color:var(--dim);font:inherit;
 padding:6px 14px;letter-spacing:.15em;cursor:pointer;text-decoration:none;font-size:13px}
button.on{border-color:var(--amb);color:var(--amb)}
a.btn{margin-left:auto}
#map{max-width:1040px;margin:14px auto 0;height:420px;border:2px solid #26272b;background:#101114}
#map[hidden]{display:none}
.pl{width:28px;height:28px;display:flex;align-items:center;justify-content:center}
.pl svg{width:22px;height:22px;fill:var(--amb);filter:drop-shadow(0 0 2px #000)}
.pl.trn svg{fill:var(--grn)}
.pl.vis svg{animation:bl .7s step-end infinite}
.pl b{position:absolute;top:26px;left:50%;transform:translateX(-50%);font:11px "Courier New",monospace;
 color:#fff;background:rgba(0,0,0,.65);padding:1px 4px;border-radius:2px;white-space:nowrap}
.leaflet-popup-content{font:13px "Courier New",monospace}
.leaflet-container{background:#0b0c0e}
.osm-dark{filter:invert(1) hue-rotate(180deg) brightness(.8) contrast(.95) saturate(.5)}
.leaflet-control-attribution{background:rgba(0,0,0,.6)!important;color:#888!important}
.leaflet-control-attribution a{color:#aaa!important}
.leaflet-bar a{background:#17181c!important;color:#f5b301!important;border-color:#333!important}
.leaflet-popup-content-wrapper,.leaflet-popup-tip{background:#17181c;color:#f5b301}
@media(max-width:980px){.hide{display:none}}                 /* via AEREO e VEL */
@media(max-width:640px){.lg{display:none}body{padding:12px 6px}.board{padding:6px}th,td{padding:2px 4px}}
@media(max-width:480px){.board{overflow-x:auto}}             /* telefoni stretti: scorre solo il tabellone */
</style>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">
</head><body>
<header><h1 id="ttl">WAUF</h1><div id="clock">--:--:--</div></header>
<div id="meta"></div>
<div class="board"><table>
<thead><tr><th class="lg"></th><th>VOLO</th><th>ROTTA</th><th class="hide">AEREO</th><th>QUOTA</th><th class="hide">VEL</th><th>DIST</th><th>NOTE</th></tr></thead>
<tbody id="tb"></tbody></table><div id="empty" hidden>NESSUN TRAFFICO NEL RAGGIO</div></div>
<div id="ctl">
<button id="bTrn" class="on">SOLO ARRIVI</button>
<button id="bAll">TUTTO IL TRAFFICO</button>
<button id="bMap">MAPPA</button>
<a class="btn" href="/config">&#9881; CONFIGURAZIONE</a>
</div>
<div id="map" hidden></div>
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<script>
let onlyTrn=true,last={},data=null,showMap=false,map=null,home=null,ring=null,markers={};
const $=id=>document.getElementById(id);
const PLANE='<svg viewBox="0 0 24 24"><path d="M21 16v-2l-8-5V3.5a1.5 1.5 0 0 0-3 0V9l-8 5v2l8-2.5V19l-2 1.5V22l3.5-1 3.5 1v-1.5L13 19v-5.5z"/></svg>';
try{showMap=localStorage.getItem('wauf_map')=='1'}catch(e){}
$('bMap').onclick=()=>{showMap=!showMap;try{localStorage.setItem('wauf_map',showMap?'1':'0')}catch(e){}
 $('bMap').classList.toggle('on',showMap);$('map').hidden=!showMap;if(showMap)renderMap()};
// Mappa scura: tile OpenStreetMap standard (nessuna chiave, nessun servizio in piu')
// rese scure con un filtro CSS (classe .osm-dark).
function darkTiles(m){
 L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,className:'osm-dark',attribution:'&copy; OpenStreetMap'}).addTo(m);
}
function initMap(){
 if(map||typeof L==='undefined'||!data)return;
 map=L.map('map').setView([+data.lat,+data.lon],9);
 darkTiles(map);
 home=L.circleMarker([+data.lat,+data.lon],{radius:5,color:'#f5b301',fillColor:'#f5b301',fillOpacity:1}).addTo(map).bindTooltip('CASA');
 ring=L.circle([+data.lat,+data.lon],{radius:data.radius*1852,color:'#f5b301',weight:1,fillOpacity:.03}).addTo(map);
 map.fitBounds(ring.getBounds());
}
function renderMap(){
 if(!showMap||!data)return;
 initMap();if(!map)return;
 home.setLatLng([+data.lat,+data.lon]);ring.setLatLng([+data.lat,+data.lon]);ring.setRadius(data.radius*1852);
 const alive={};
 for(const f of data.flights){
  if(!(f.lat&&f.lon))continue;
  if(onlyTrn&&!f.trn)continue;
  alive[f.cs]=1;
  const rot=f.trk>=0?f.trk:0;
  const icon=L.divIcon({className:'',iconSize:[28,28],iconAnchor:[14,14],
   html:'<div class="pl'+(f.vis?' vis trn':f.trn?' trn':'')+'" style="transform:rotate('+rot+'deg)">'+PLANE+'</div><b>'+esc(f.cs)+'</b>'});
  const pop='<b>'+esc(f.cs)+'</b> '+esc(f.route)+'<br>'+(f.typ?esc(f.typ)+' ':'')+(f.reg?esc(f.reg):'')+
   '<br>'+(f.alt<0?'a terra':f.alt+' ft')+' &middot; '+f.gs+' kt &middot; '+f.dist+' km '+f.dir;
  if(markers[f.cs]){markers[f.cs].setLatLng([+f.lat,+f.lon]).setIcon(icon).setPopupContent(pop)}
  else markers[f.cs]=L.marker([+f.lat,+f.lon],{icon}).addTo(map).bindPopup(pop);
 }
 for(const k in markers)if(!alive[k]){map.removeLayer(markers[k]);delete markers[k]}
}
const LOGO='https://www.flightaware.com/images/airline_logos/90p/';
$('bTrn').onclick=()=>{onlyTrn=true;sw();render();renderMap()};
$('bAll').onclick=()=>{onlyTrn=false;sw();render();renderMap()};
function sw(){$('bTrn').classList.toggle('on',onlyTrn);$('bAll').classList.toggle('on',!onlyTrn)}
function tiles(s,w){s=(s+'').toUpperCase().padEnd(w).slice(0,w);
 return '<span class="t">'+[...s].map(c=>'<span>'+(c==' '?'&nbsp;':c)+'</span>').join('')+'</span>'}
function esc(s){return (s+'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
// Codice compagnia: dal routeset, altrimenti le 3 lettere iniziali del callsign (prefisso ICAO)
function airline(f){if(f.al)return f.al;const m=/^([A-Z]{3})[0-9]/.exec(f.cs||'');return m?m[1]:''}
function logo(f){const a=airline(f);if(!a)return '';
 return '<img src="'+LOGO+a+'.png" alt="'+a+'" title="'+a+'" onerror="this.replaceWith(Object.assign(document.createElement(\'span\'),{textContent:\''+a+'\'}))">'}
function render(){
 if(!data)return;
 const tb=$('tb');let rows='';const seen={};
 let fl=data.flights.filter(f=>!onlyTrn||f.trn);
 $('empty').hidden=fl.length>0;
 for(const f of fl){
  const alt=f.alt<0?'TERRA':f.alt+(f.vr<-100?'↓':f.vr>100?'↑':'');
  const note=f.vis?'IN VISTA':f.trn?'IN ARRIVO':(f.route=='?'?'ROTTA N.D.':'TRANSITO');
  const key=f.cs,str=f.cs+f.route+f.alt+f.dist;
  const flip=last[key]&&last[key]!=str?' flip':'';seen[key]=str;
  const tip=esc((f.reg?'Matricola '+f.reg+' ':'')+(f.typ?'Tipo '+f.typ+' ':'')+(f.hex?'ICAO24 '+f.hex:''));
  rows+='<tr class="'+(f.vis?'vis':f.trn?'trn':'')+flip+'" title="'+tip+'"><td class="lg">'+logo(f)+'</td><td>'+tiles(f.cs,8)+
   '</td><td>'+tiles(f.route,9)+'</td><td class="hide">'+tiles(f.typ||'----',4)+
   '</td><td>'+tiles(alt,6)+'</td><td class="hide">'+tiles(f.gs+'KT',5)+'</td><td>'+
   tiles(f.dist+'KM '+f.dir,9)+'</td><td class="st">'+note+'</td></tr>';
 }
 last=seen;tb.innerHTML=rows;
 $('ttl').textContent='WAUF ✈ ARRIVI '+(data.destName||data.dest);
 document.title='WAUF - '+(data.destName||data.dest);
 $('bTrn').textContent='SOLO '+data.dest;
 $('meta').innerHTML='POS <b>'+data.lat+' '+data.lon+'</b> &middot; RAGGIO <b>'+data.radius+
  ' NM</b> &middot; WIFI <b>'+data.rssi+' dBm</b> &middot; AGG. <b>'+
  (data.age<0?'MAI':data.age+' S FA')+'</b> &middot; DEST <b>'+data.dest+'</b>';
}
async function doFetch(){
 try{data=await(await fetch('/api/flights')).json();render();renderMap()}catch(e){}
}
async function poll(){await doFetch();setTimeout(poll,10000)}
setInterval(()=>{$('clock').textContent=new Date().toLocaleTimeString('it-IT')},1000);
$('bMap').classList.toggle('on',showMap);$('map').hidden=!showMap;
poll();
</script></body></html>)rawliteral";
