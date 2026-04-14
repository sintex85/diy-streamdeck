#pragma once

static const char WIFI_SETUP_HTML[] PROGMEM = R"==(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stream Deck - WiFi Setup</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:30px 16px}
h1{font-size:22px;margin-bottom:20px;color:#667eea}
.card{background:#16213e;border-radius:16px;padding:24px;max-width:400px;width:100%}
.net{padding:12px;border-radius:8px;background:#0f1a30;margin-bottom:8px;cursor:pointer;display:flex;justify-content:space-between;align-items:center;border:2px solid transparent}
.net:hover,.net.sel{border-color:#667eea}
.net .name{font-weight:600}.net .sig{color:#888;font-size:12px}
.field{margin-top:16px}
.field label{display:block;font-size:13px;color:#aaa;margin-bottom:4px}
.field input{width:100%;padding:10px;border-radius:8px;border:1px solid #333;background:#0f1a30;color:#eee;font-size:15px}
button{width:100%;padding:12px;border-radius:10px;border:none;background:#667eea;color:#fff;font-size:16px;font-weight:600;cursor:pointer;margin-top:16px}
button:hover{background:#5a6fd6}
.msg{text-align:center;margin-top:16px;font-size:14px}
.scanning{color:#888;text-align:center;padding:20px}
</style></head><body>
<h1>Stream Deck - WiFi</h1>
<div class="card">
<div id="nets"><div class="scanning">Buscando redes...</div></div>
<div class="field"><label>Contrasena WiFi</label><input type="password" id="pass" placeholder="Tu contrasena WiFi"></div>
<button onclick="connect()">Conectar</button>
<div class="msg" id="msg"></div>
</div>
<script>
let sel="";
async function scan(){
const r=await fetch("/api/wifi/scan");const nets=await r.json();
const el=document.getElementById("nets");el.innerHTML="";
nets.forEach(n=>{const d=document.createElement("div");d.className="net"+(sel===n.ssid?" sel":"");
d.innerHTML=`<span class="name">${n.ssid}</span><span class="sig">${n.rssi}dB ${n.open?"🔓":"🔒"}</span>`;
d.onclick=()=>{sel=n.ssid;scan();};el.appendChild(d);});
if(!nets.length)el.innerHTML='<div class="scanning">No se encontraron redes</div>';}
async function connect(){
if(!sel){document.getElementById("msg").textContent="Selecciona una red";return;}
document.getElementById("msg").textContent="Conectando...";
const r=await fetch("/api/wifi/connect",{method:"POST",headers:{"Content-Type":"application/json"},
body:JSON.stringify({ssid:sel,pass:document.getElementById("pass").value})});
const d=await r.json();
if(d.ok)document.getElementById("msg").innerHTML="<b>Conectado!</b><br>Abre <b>http://streamdeck.local</b><br>o <b>http://"+d.ip+"</b>";
else document.getElementById("msg").textContent="Error: "+d.error;}
scan();
</script></body></html>)==";
