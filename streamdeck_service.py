#!/usr/bin/env python3
"""Stream Deck service: serial listener + config web server on localhost:1313"""
import glob
import http.server
import json
import os
import platform
import serial
import serial.tools.list_ports
import subprocess
import sys
import threading
import time
import webbrowser

IS_MAC = platform.system() == "Darwin"
IS_WIN = platform.system() == "Windows"
PORT_WEB = 1313

sys.stdout.reconfigure(line_buffering=True)
sys.stderr.reconfigure(line_buffering=True)

ser = None
ser_lock = threading.Lock()
config_cache = []

# ─── Serial ───
def find_esp32():
    for p in serial.tools.list_ports.comports():
        d = (p.description or "").lower()
        if "ch340" in d or "ch341" in d or "usbserial" in p.device.lower():
            return p.device
    if not IS_WIN:
        for g in glob.glob("/dev/cu.usbserial-*"):
            return g
    return None

def serial_send(cmd):
    with ser_lock:
        if ser and ser.is_open:
            ser.write((cmd + "\n").encode())
            ser.flush()

def serial_send_wait(cmd, marker="OK", timeout=3):
    with ser_lock:
        if not ser or not ser.is_open:
            return ""
        ser.reset_input_buffer()
        ser.write((cmd + "\n").encode())
        ser.flush()
        buf = ""
        start = time.time()
        while time.time() - start < timeout:
            if ser.in_waiting:
                buf += ser.read(ser.in_waiting).decode("utf-8", errors="ignore")
                if marker in buf or "END" in buf or "ERR" in buf:
                    time.sleep(0.05)
                    buf += ser.read(ser.in_waiting).decode("utf-8", errors="ignore")
                    break
            time.sleep(0.02)
        return buf.strip()

def load_config_from_esp32():
    global config_cache
    resp = serial_send_wait("GETALL", "END", 4)
    buttons = []
    for line in resp.split("\n"):
        if not line.startswith("CFG:"):
            continue
        p = []
        start = 0
        for c in range(7):
            idx = line.index(":", start) if ":" in line[start:] else -1
            if idx < 0:
                break
            p.append(line[start:idx])
            start = idx + 1
        p.append(line[start:])
        if len(p) < 8:
            continue
        colors = p[3].split(",")
        style = p[5].split(",")
        buttons.append({
            "label": p[2], "r": int(colors[0]), "g": int(colors[1]), "b": int(colors[2]),
            "action": p[7], "actionType": int(p[6] or 0),
            "iconSizeIdx": int(style[0]) if len(style) > 0 else 1,
            "borderStyle": int(style[1]) if len(style) > 1 else 0,
            "showLabel": int(style[2]) != 0 if len(style) > 2 else True,
        })
    if buttons:
        config_cache = buttons
    return buttons

def open_url(url):
    webbrowser.open(url)

def open_app(app):
    if IS_MAC:
        subprocess.Popen(["open", "-a", app])
    elif IS_WIN:
        subprocess.Popen(f'start "" "{app}"', shell=True)

def serial_listener():
    global ser
    while True:
        port = find_esp32()
        if not port:
            time.sleep(3)
            continue
        try:
            with ser_lock:
                ser = serial.Serial(port, 115200, timeout=1)
            print(f"Conectado a {port}")
            time.sleep(1)
            load_config_from_esp32()
            while True:
                with ser_lock:
                    if not ser or not ser.is_open:
                        break
                    raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="ignore").strip()
                if not line.startswith("BTN:"):
                    continue
                parts = line.split(":", 3)
                if len(parts) < 4:
                    continue
                idx, atype, action = int(parts[1]), int(parts[2]), parts[3]
                print(f"  Btn {idx+1}: type={atype} -> {action}")
                if atype == 1:
                    open_url(action)
                elif atype == 3:
                    open_app(action)
        except serial.SerialException:
            print("Desconectado. Reintentando...")
            with ser_lock:
                ser = None
            time.sleep(2)
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(2)

# ─── Config HTML (embedded) ───
CONFIG_HTML = """<!DOCTYPE html>
<html lang="es"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stream Deck Config</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:20px 12px}
h1{font-size:24px;margin-bottom:6px;background:linear-gradient(135deg,#667eea,#764ba2);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.sub{color:#888;margin-bottom:16px;font-size:13px}
.status{padding:4px 12px;border-radius:12px;font-size:12px;margin-bottom:16px}
.on{background:#0d3320;color:#4ade80}.off{background:#3b1111;color:#f87171}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;max-width:640px;width:100%;margin-bottom:20px}
.btn-card{aspect-ratio:1.3;border-radius:12px;display:flex;flex-direction:column;align-items:center;justify-content:center;cursor:pointer;transition:all .15s;box-shadow:0 3px 10px rgba(0,0,0,.4);border:2px solid transparent;gap:4px;position:relative}
.btn-card:hover{transform:translateY(-2px);border-color:rgba(255,255,255,.3)}
.btn-card .label{font-size:14px;font-weight:700;text-shadow:0 1px 2px rgba(0,0,0,.5)}
.btn-card .hint{font-size:10px;opacity:.6;max-width:90%;text-align:center;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.7);z-index:100;align-items:center;justify-content:center}
.overlay.active{display:flex}
.modal{background:#16213e;border-radius:16px;padding:24px;width:420px;max-width:95vw;box-shadow:0 15px 50px rgba(0,0,0,.5);max-height:90vh;overflow-y:auto}
.modal h2{margin-bottom:16px;font-size:18px}
.field{margin-bottom:14px}
.field label{display:block;font-size:12px;color:#aaa;margin-bottom:4px}
.field input{width:100%;padding:8px 12px;border-radius:8px;border:1px solid #333;background:#0f1a30;color:#eee;font-size:14px;outline:none}
.field input:focus{border-color:#667eea}
.field input[type=color]{height:40px;padding:3px;cursor:pointer}
.pills{display:flex;gap:4px}
.pills label{flex:1;text-align:center;padding:7px;border-radius:6px;background:#0f1a30;border:1px solid #333;cursor:pointer;font-size:12px;transition:all .2s}
.pills input{display:none}
.pills label:has(input:checked){border-color:#667eea;background:#1a2540;color:#667eea}
.presets{display:grid;grid-template-columns:repeat(3,1fr);gap:4px;margin-top:8px}
.preset{padding:6px;border-radius:6px;background:#0f1a30;border:1px solid #333;color:#ccc;font-size:11px;cursor:pointer;text-align:center}
.preset:hover{border-color:#667eea;color:#fff}
.preset .pi{font-size:16px;display:block}
.sep{border-top:1px solid #2a2a3e;margin:16px 0}
.toggle-row{display:flex;align-items:center;justify-content:space-between;cursor:pointer}
.toggle-row input{display:none}
.toggle-sw{width:40px;height:22px;background:#333;border-radius:11px;position:relative;transition:background .2s}
.toggle-sw::after{content:"";position:absolute;top:2px;left:2px;width:18px;height:18px;background:#888;border-radius:50%;transition:all .2s}
.toggle-row input:checked+.toggle-sw{background:#667eea}
.toggle-row input:checked+.toggle-sw::after{left:20px;background:#fff}
.actions{display:flex;gap:8px;margin-top:18px}
.actions button{flex:1;padding:10px;border-radius:8px;border:none;font-size:14px;font-weight:600;cursor:pointer}
.btn-save{background:#667eea;color:#fff}.btn-cancel{background:#2a2a3e;color:#aaa}.btn-clear{background:#dc2626;color:#fff}
.help{font-size:10px;color:#555;margin-top:4px}
</style></head><body>
<h1>Stream Deck Config</h1>
<p class="sub">by Bits y Tornillos</p>
<span class="status" id="st">Cargando...</span>
<div class="grid" id="grid"></div>
<div class="overlay" id="overlay">
<div class="modal">
<h2>Boton <span id="eIdx"></span></h2>
<div class="field"><label>Nombre</label><input type="text" id="eLabel" maxlength="19"></div>
<div class="field"><label>Color</label><input type="color" id="eColor"></div>
<div class="field"><label>Accion</label>
<div class="pills">
<label><input type="radio" name="aType" value="0"><span>Ninguna</span></label>
<label><input type="radio" name="aType" value="1"><span>URL</span></label>
<label><input type="radio" name="aType" value="3"><span>App</span></label>
<label><input type="radio" name="aType" value="2"><span>Teclado</span></label>
</div></div>
<div id="presetBox" style="display:none"><div class="presets" id="presets"></div></div>
<div class="field"><label>Valor</label><input type="text" id="eAction"><div class="help" id="eHelp"></div></div>
<div class="sep"></div>
<div class="field"><label>Tamano icono</label><div class="pills">
<label><input type="radio" name="iSize" value="0"><span>S</span></label>
<label><input type="radio" name="iSize" value="1"><span>M</span></label>
<label><input type="radio" name="iSize" value="2"><span>L</span></label>
<label><input type="radio" name="iSize" value="3"><span>XL</span></label>
</div></div>
<div class="field"><label>Marco</label><div class="pills">
<label><input type="radio" name="bStyle" value="0"><span>No</span></label>
<label><input type="radio" name="bStyle" value="1"><span>Fino</span></label>
<label><input type="radio" name="bStyle" value="2"><span>Grueso</span></label>
<label><input type="radio" name="bStyle" value="3"><span>Glow</span></label>
</div></div>
<div class="field"><label class="toggle-row"><span>Mostrar texto</span><input type="checkbox" id="eShowLabel" checked><span class="toggle-sw"></span></label></div>
<div class="actions">
<button class="btn-cancel" onclick="closeModal()">Cancelar</button>
<button class="btn-clear" onclick="clearBtn()">Limpiar</button>
<button class="btn-save" onclick="saveBtn()">Guardar</button>
</div></div></div>
<script>
let B=[],eI=-1;
const presets=[
{i:"\\u{1F50A}",l:"Vol +",v:"vol_up"},{i:"\\u{1F509}",l:"Vol -",v:"vol_down"},{i:"\\u{1F507}",l:"Mute",v:"vol_mute"},
{i:"\\u23EF",l:"Play",v:"play_pause"},{i:"\\u23ED",l:"Next",v:"next_track"},{i:"\\u23EE",l:"Prev",v:"prev_track"},
{i:"\\u{1F4CB}",l:"Copy",v:"ctrl+c"},{i:"\\u{1F4CC}",l:"Paste",v:"ctrl+v"},{i:"\\u21A9",l:"Undo",v:"ctrl+z"},
{i:"\\u{1F4BE}",l:"Save",v:"ctrl+s"},{i:"\\u{1F50D}",l:"Find",v:"ctrl+f"},{i:"\\u{1F4F8}",l:"Screenshot",v:"gui+shift+s"}];
const helps={0:"",1:"https://youtube.com",3:"spotify: / discord:",2:"ctrl+c, vol_up, play_pause..."};
const phs={0:"",1:"https://www.amazon.es",3:"spotify:",2:"ctrl+shift+a"};

async function load(){
  const r=await fetch("/api/buttons");B=await r.json();render();
  const s=await fetch("/api/status");const d=await s.json();
  const el=document.getElementById("st");
  el.textContent=d.connected?"Conectado":"Desconectado";
  el.className="status "+(d.connected?"on":"off");
}
function render(){
  const g=document.getElementById("grid");g.innerHTML="";
  B.forEach((b,i)=>{const d=document.createElement("div");d.className="btn-card";
    d.style.background="rgb("+b.r+","+b.g+","+b.b+")";
    let h=b.action?(b.action.replace("https://","").replace("http://","").substring(0,25)):"";
    d.innerHTML='<span class="label">'+b.label+'</span><span class="hint">'+h+'</span>';
    d.onclick=()=>openModal(i);g.appendChild(d);});}
function openModal(i){eI=i;const b=B[i];
  document.getElementById("eIdx").textContent=i+1;
  document.getElementById("eLabel").value=b.label;
  document.getElementById("eColor").value=rgbHex(b.r,b.g,b.b);
  document.getElementById("eAction").value=b.action||"";
  document.querySelector('input[name=aType][value="'+(b.actionType||0)+'"]').checked=true;
  document.querySelector('input[name=iSize][value="'+(b.iconSizeIdx||1)+'"]').checked=true;
  document.querySelector('input[name=bStyle][value="'+(b.borderStyle||0)+'"]').checked=true;
  document.getElementById("eShowLabel").checked=b.showLabel!==false;
  updHelp();document.getElementById("overlay").classList.add("active");}
function closeModal(){document.getElementById("overlay").classList.remove("active");eI=-1;}
function rgbHex(r,g,b){return"#"+[r,g,b].map(x=>x.toString(16).padStart(2,"0")).join("");}
function hexRgb(h){const m=h.match(/^#?(\\w{2})(\\w{2})(\\w{2})$/);return m?[parseInt(m[1],16),parseInt(m[2],16),parseInt(m[3],16)]:[100,100,100];}
function updHelp(){
  const t=+document.querySelector("input[name=aType]:checked").value;
  document.getElementById("eHelp").textContent=helps[t]||"";
  document.getElementById("eAction").placeholder=phs[t]||"";
  const pb=document.getElementById("presetBox");
  if(t===2){pb.style.display="";const pg=document.getElementById("presets");pg.innerHTML="";
    presets.forEach(p=>{const d=document.createElement("div");d.className="preset";
      d.innerHTML='<span class="pi">'+p.i+'</span>'+p.l;
      d.onclick=()=>{document.getElementById("eAction").value=p.v;document.getElementById("eLabel").value=p.l;};
      pg.appendChild(d);});}else pb.style.display="none";}
document.querySelectorAll("input[name=aType]").forEach(r=>r.addEventListener("change",updHelp));
document.getElementById("overlay").addEventListener("click",e=>{if(e.target.id==="overlay")closeModal();});
async function saveBtn(){
  const i=eI,c=hexRgb(document.getElementById("eColor").value);
  const data={label:document.getElementById("eLabel").value||String(i+1),
    r:c[0],g:c[1],b:c[2],action:document.getElementById("eAction").value,
    actionType:+document.querySelector("input[name=aType]:checked").value,
    iconSizeIdx:+document.querySelector("input[name=iSize]:checked").value,
    borderStyle:+document.querySelector("input[name=bStyle]:checked").value,
    showLabel:document.getElementById("eShowLabel").checked};
  await fetch("/api/btn/"+i,{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(data)});
  B[i]=data;render();closeModal();}
async function clearBtn(){
  await fetch("/api/btn/"+eI,{method:"POST",headers:{"Content-Type":"application/json"},
    body:JSON.stringify({label:String(eI+1),r:100,g:100,b:100,action:"",actionType:0,iconSizeIdx:1,borderStyle:0,showLabel:true})});
  load();closeModal();}
load();setInterval(load,10000);
</script></body></html>"""

# ─── Web Server ───
class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _json(self, data, code=200):
        b = json.dumps(data).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)
    def do_GET(self):
        if self.path == "/":
            b = CONFIG_HTML.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(b)))
            self.end_headers()
            self.wfile.write(b)
        elif self.path == "/api/buttons":
            if not config_cache:
                load_config_from_esp32()
            self._json(config_cache)
        elif self.path == "/api/status":
            with ser_lock:
                connected = ser is not None and ser.is_open
            self._json({"connected": connected})
        else:
            self.send_error(404)
    def do_POST(self):
        if self.path.startswith("/api/btn/"):
            idx = int(self.path.split("/")[-1])
            length = int(self.headers.get("Content-Length", 0))
            body = json.loads(self.rfile.read(length))
            l = body.get("label", str(idx+1))[:19]
            r, g, b = body.get("r", 100), body.get("g", 100), body.get("b", 100)
            isz = body.get("iconSizeIdx", 1)
            bst = body.get("borderStyle", 0)
            shl = 1 if body.get("showLabel", True) else 0
            act = body.get("action", "")
            aty = body.get("actionType", 0)
            # Send to ESP32
            serial_send_wait(f"SET:{idx}:{l}:{r},{g},{b}:{isz},{bst},{shl}", "OK", 2)
            serial_send_wait(f"ACT:{idx}:{aty}:{act}", "OK", 2)
            load_config_from_esp32()
            self._json({"ok": True})
        else:
            self.send_error(404)
    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

# ─── Main ───
def install():
    exe = sys.executable
    script = os.path.abspath(__file__)
    if IS_MAC:
        plist_path = os.path.expanduser("~/Library/LaunchAgents/com.diy.streamdeck.plist")
        plist = f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>Label</key><string>com.diy.streamdeck</string>
<key>ProgramArguments</key><array><string>{exe}</string><string>{script}</string></array>
<key>RunAtLoad</key><true/>
<key>KeepAlive</key><true/>
<key>StandardOutPath</key><string>/tmp/streamdeck.log</string>
<key>StandardErrorPath</key><string>/tmp/streamdeck.log</string>
</dict></plist>"""
        os.makedirs(os.path.dirname(plist_path), exist_ok=True)
        with open(plist_path, "w") as f:
            f.write(plist)
        subprocess.run(["launchctl", "load", plist_path])
        print("Auto-arranque instalado.")
    elif IS_WIN:
        startup = os.path.join(os.environ.get("APPDATA", ""), "Microsoft", "Windows", "Start Menu", "Programs", "Startup")
        with open(os.path.join(startup, "StreamDeck.vbs"), "w") as f:
            f.write(f'Set ws=CreateObject("Wscript.Shell")\nws.Run """{exe}"" ""{script}""", 0, False\n')
        print("Auto-arranque instalado.")

def uninstall():
    if IS_MAC:
        p = os.path.expanduser("~/Library/LaunchAgents/com.diy.streamdeck.plist")
        subprocess.run(["launchctl", "unload", p], capture_output=True)
        if os.path.exists(p): os.remove(p)
    elif IS_WIN:
        p = os.path.join(os.environ.get("APPDATA", ""), "Microsoft", "Windows", "Start Menu", "Programs", "Startup", "StreamDeck.vbs")
        if os.path.exists(p): os.remove(p)
    print("Auto-arranque desinstalado.")

def main():
    print("Stream Deck Service")
    # Start serial listener thread
    t = threading.Thread(target=serial_listener, daemon=True)
    t.start()
    # Start web server
    class R(http.server.HTTPServer):
        allow_reuse_address = True
    server = R(("0.0.0.0", PORT_WEB), Handler)
    print(f"Config: http://localhost:{PORT_WEB}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        server.shutdown()

if __name__ == "__main__":
    if "--install" in sys.argv: install()
    elif "--uninstall" in sys.argv: uninstall()
    else: main()
