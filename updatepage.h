#pragma once
#include <pgmspace.h>

// Pagina /update: upload del solo firmware (.bin). La POST viene gestita da
// ESP8266HTTPUpdateServer, che si aspetta il campo file chiamato "firmware".
static const char UPDATEPAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="it"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WAUF - AGGIORNAMENTO</title>
<style>
:root{--amb:#f5b301;--dim:#7a5c07;--bg:#0b0c0e;--cell:#17181c;--grn:#3ddc84;--red:#e05252}
*{box-sizing:border-box;margin:0}
body{background:var(--bg);color:var(--amb);font-family:"Courier New",ui-monospace,monospace;min-height:100vh;padding:18px 10px}
header{max-width:700px;margin:0 auto 14px;display:flex;justify-content:space-between;align-items:baseline;flex-wrap:wrap;gap:8px;border-bottom:2px solid var(--dim);padding-bottom:10px}
h1{font-size:clamp(17px,4vw,26px);letter-spacing:.18em;font-weight:700}
a{color:var(--dim);text-decoration:none;letter-spacing:.15em;font-size:13px;border:1px solid var(--dim);padding:6px 14px}
section{max-width:700px;margin:0 auto 16px;border:1px dashed var(--dim);padding:14px}
h2{font-size:13px;letter-spacing:.25em;color:var(--dim);margin-bottom:12px;font-weight:400}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-bottom:10px}
input[type=file]{color:var(--amb);font:inherit}
button{background:none;border:1px solid var(--dim);color:var(--amb);font:inherit;padding:6px 14px;letter-spacing:.15em;cursor:pointer}
button:disabled{opacity:.4;cursor:default}
.msg{letter-spacing:.15em;font-size:13px}.ok{color:var(--grn)}.err{color:var(--red)}
.hint{color:var(--dim);font-size:11px;letter-spacing:.05em;line-height:1.5}
#bar{height:8px;background:var(--cell);border:1px solid var(--dim);margin:10px 0;display:none}
#bar i{display:block;height:100%;width:0;background:var(--amb)}
</style></head><body>
<header><h1>WAUF &#8593; AGGIORNAMENTO</h1><a href="/config">&larr; CONFIGURAZIONE</a></header>
<section>
<h2>FIRMWARE (.BIN)</h2>
<form id="f" method="POST" action="/update" enctype="multipart/form-data">
<div class="row"><input type="file" name="firmware" id="file" accept=".bin" required></div>
<div class="row"><button id="b" type="submit">CARICA E RIAVVIA</button><span class="msg" id="m"></span></div>
</form>
<div id="bar"><i id="fill"></i></div>
<p class="hint">Nell'IDE Arduino: Sketch &rarr; Esporta binario compilato, poi scegli il file <b>WAUF.ino.*.bin</b>
dalla cartella dello sketch. L'upload dura 10-30 s; al termine il dispositivo si riavvia da solo.
Non togliere alimentazione durante l'aggiornamento.</p>
</section>
<script>
const $=id=>document.getElementById(id);
$('f').onsubmit=e=>{
 e.preventDefault();const file=$('file').files[0];if(!file)return;
 if(!/\.bin$/i.test(file.name)){$('m').textContent='SERVE UN FILE .BIN';$('m').className='msg err';return}
 const fd=new FormData();fd.append('firmware',file);
 const x=new XMLHttpRequest();x.open('POST','/update');
 $('b').disabled=true;$('bar').style.display='block';$('m').textContent='UPLOAD...';$('m').className='msg';
 x.upload.onprogress=ev=>{if(ev.lengthComputable)$('fill').style.width=Math.round(ev.loaded/ev.total*100)+'%'};
 x.onload=()=>{const ok=x.status==200&&!/FAIL/i.test(x.responseText);
  $('m').textContent=ok?'OK, RIAVVIO IN CORSO. ATTENDI 20 S E RICARICA IL TABELLONE':'ERRORE: '+x.responseText;
  $('m').className='msg '+(ok?'ok':'err');$('b').disabled=false;
  if(ok)setTimeout(()=>location.href='/',20000)};
 x.onerror=()=>{$('m').textContent='ERRORE DI RETE';$('m').className='msg err';$('b').disabled=false};
 x.send(fd);
};
</script></body></html>)rawliteral";
