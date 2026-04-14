#!/usr/bin/env python3
"""
DIY Stream Deck - App de configuración y listener.
Ejecuta: python3 streamdeck_app.py
Abre: http://localhost:8888
"""

import base64
import glob as globmod
import http.server
import io
import json
import os
import platform
import serial
import serial.tools.list_ports
import subprocess
import sys
import threading
import time
import urllib.parse
import urllib.request
import webbrowser

IS_WINDOWS = platform.system() == "Windows"
IS_MAC = platform.system() == "Darwin"

PORT_WEB = 1313
BAUD = 115200

# When running as PyInstaller exe, use the exe's directory for config/icons
if getattr(sys, 'frozen', False):
    APP_BASE = os.path.dirname(sys.executable)
else:
    APP_BASE = os.path.dirname(os.path.abspath(__file__))

CONFIG_FILE = os.path.join(APP_BASE, "streamdeck_config.json")
ICON_DIR = os.path.join(APP_BASE, "icons")
ICON_SIZE = 32

NUM_BUTTONS = 12

# Try to import PIL for icon processing
try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False
    print("[Aviso] Pillow no instalado - sin iconos. Instala con: pip3 install Pillow")

# ─── Config ───
DEFAULT_COLORS = [
    [231,76,60],[46,204,113],[52,152,219],[241,196,15],
    [155,89,182],[230,126,34],[26,188,156],[236,64,122],
    [52,73,94],[127,140,141],[39,174,96],[41,128,185],
]

config = []
ser = None
ser_lock = threading.Lock()

ICON_SIZES = [24, 32, 48, 64]

def default_button(i):
    return {"label": str(i+1), "color": DEFAULT_COLORS[i % len(DEFAULT_COLORS)],
            "action": "", "action_type": "url", "has_icon": False,
            "icon_size": 1, "border_style": 0, "show_label": True}

def load_config():
    global config
    if os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE, "r") as f:
            config = json.load(f)
    else:
        config = [default_button(i) for i in range(NUM_BUTTONS)]
    while len(config) < NUM_BUTTONS:
        config.append(default_button(len(config)))
    for btn in config:
        btn.setdefault("has_icon", False)

def save_config():
    with open(CONFIG_FILE, "w") as f:
        json.dump(config, f, indent=2)

# ─── Icon handling ───
def rgb_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565 (big-endian for ESP32 pushImage)."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def fetch_favicon(url):
    """Download favicon for a URL and return as PIL Image."""
    if not HAS_PIL:
        print("[Icon] Pillow no instalado, no se pueden procesar iconos")
        return None
    try:
        domain = urllib.parse.urlparse(url).netloc
        if not domain:
            domain = url.replace("https://","").replace("http://","").split("/")[0]
        if not domain:
            print(f"[Icon] No se pudo extraer dominio de: {url}")
            return None

        # Try multiple favicon sources
        sources = [
            f"https://www.google.com/s2/favicons?domain={domain}&sz=64",
            f"https://favicone.com/{domain}?s=64",
            f"https://{domain}/favicon.ico",
        ]

        for favicon_url in sources:
            try:
                print(f"[Icon] Descargando: {favicon_url}")
                import ssl
                ctx = ssl.create_default_context()
                ctx.check_hostname = False
                ctx.verify_mode = ssl.CERT_NONE
                req = urllib.request.Request(favicon_url, headers={"User-Agent": "Mozilla/5.0"})
                resp = urllib.request.urlopen(req, timeout=8, context=ctx)
                data = resp.read()
                if len(data) < 100:  # Too small, probably an error
                    continue
                img = Image.open(io.BytesIO(data)).convert("RGBA")
                print(f"[Icon] OK - {img.size[0]}x{img.size[1]}")
                return img
            except Exception as e:
                print(f"[Icon] Fallo {favicon_url}: {e}")
                continue

        print(f"[Icon] No se pudo obtener favicon para {domain}")
        return None
    except Exception as e:
        print(f"[Icon] Error general: {e}")
        return None

def get_app_icon(app_name):
    """Get app icon as 32x32 PIL Image (macOS and Windows)."""
    if not HAS_PIL:
        return None
    try:
        import tempfile
        if IS_MAC:
            result = subprocess.run(
                ["mdfind", f"kMDItemDisplayName == '{app_name}' && kMDItemContentType == 'com.apple.application-bundle'"],
                capture_output=True, text=True, timeout=5
            )
            app_path = result.stdout.strip().split("\n")[0]
            if not app_path:
                return None
            icon_path = os.path.join(app_path, "Contents", "Resources")
            icns_files = globmod.glob(os.path.join(icon_path, "*.icns"))
            if not icns_files:
                return None
            tmp = tempfile.NamedTemporaryFile(suffix=".png", delete=False)
            tmp.close()
            subprocess.run(["sips", "-s", "format", "png", "-z", "64", "64",
                            icns_files[0], "--out", tmp.name],
                           capture_output=True, timeout=5)
            img = Image.open(tmp.name).convert("RGBA")
            os.unlink(tmp.name)
        elif IS_WINDOWS:
            try:
                import icoextract
                # Try common paths
                for base in [os.environ.get("PROGRAMFILES",""), os.environ.get("PROGRAMFILES(X86)",""), os.environ.get("LOCALAPPDATA","")]:
                    if not base:
                        continue
                    for exe in globmod.glob(os.path.join(base, "**", f"*{app_name}*.exe"), recursive=True):
                        extractor = icoextract.IconExtractor(exe)
                        tmp = tempfile.NamedTemporaryFile(suffix=".ico", delete=False)
                        tmp.close()
                        extractor.export_icon(tmp.name)
                        img = Image.open(tmp.name).convert("RGBA")
                        os.unlink(tmp.name)
                        return img.resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS)
            except ImportError:
                pass
            return None
        else:
            return None
        return img.resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS)
    except Exception as e:
        print(f"[Icon] Error obteniendo icono de app: {e}")
        return None

def image_to_rgb565_base64(img, bg_color=(0, 0, 0)):
    """Convert PIL RGBA Image to RGB565 base64 string for ESP32."""
    w, h = img.size
    pixels = img.load()
    data = bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            alpha = a / 255.0
            r = int(r * alpha + bg_color[0] * (1 - alpha))
            g = int(g * alpha + bg_color[1] * (1 - alpha))
            b = int(b * alpha + bg_color[2] * (1 - alpha))
            rgb565 = rgb_to_rgb565(r, g, b)
            data.append((rgb565 >> 8) & 0xFF)
            data.append(rgb565 & 0xFF)
    return base64.b64encode(bytes(data)).decode("ascii")

def process_icon_for_button(idx, custom_img=None):
    """Fetch/generate icon for button and send to ESP32."""
    btn = config[idx]
    action = btn.get("action", "").strip()
    action_type = btn.get("action_type", "url")
    pixel_size = ICON_SIZES[btn.get("icon_size", 1)]
    bg_color = tuple(btn.get("color", [0, 0, 0]))

    if not HAS_PIL:
        send_no_icon(idx)
        btn["has_icon"] = False
        return

    img = custom_img
    if img is None:
        if not action:
            send_no_icon(idx)
            btn["has_icon"] = False
            return
        if action_type == "url":
            img = fetch_favicon(action)
        elif action_type == "app":
            img = get_app_icon(action)

    if img:
        img = img.resize((pixel_size, pixel_size), Image.LANCZOS)
        b64 = image_to_rgb565_base64(img, bg_color)
        send_icon_to_esp32(idx, b64, pixel_size)
        os.makedirs(ICON_DIR, exist_ok=True)
        img.save(os.path.join(ICON_DIR, f"btn_{idx}.png"))
        btn["has_icon"] = True
        print(f"[Icon] Boton {idx+1}: icono {pixel_size}x{pixel_size} enviado")
    else:
        send_no_icon(idx)
        btn["has_icon"] = False

def send_icon_to_esp32(idx, b64_data, pixel_size):
    """Send icon data to ESP32 via serial, in chunks for Windows compatibility."""
    with ser_lock:
        if ser and ser.is_open:
            cmd = f"ICON:{idx}:{pixel_size}:{b64_data}\n"
            encoded = cmd.encode()
            print(f"[Icon] Enviando {len(encoded)} bytes por serial...")

            # Send in chunks to avoid Windows serial buffer issues
            CHUNK = 1024
            for i in range(0, len(encoded), CHUNK):
                ser.write(encoded[i:i+CHUNK])
                ser.flush()
                if len(encoded) > CHUNK:
                    time.sleep(0.02)  # Small delay between chunks

            # Wait for ESP32 response
            time.sleep(0.3)
            response = ""
            while ser.in_waiting:
                response += ser.read(ser.in_waiting).decode("utf-8", errors="ignore")
                time.sleep(0.05)
            response = response.strip()
            if response:
                print(f"[Icon] Respuesta ESP32: {response}")
            if "ERR" in response:
                print(f"[Icon] ERROR del ESP32: {response}")
            elif "OK" not in response:
                print(f"[Icon] Sin respuesta del ESP32 (puede que no haya recibido los datos)")

def send_no_icon(idx):
    with ser_lock:
        if ser and ser.is_open:
            ser.write(f"NOICON:{idx}\n".encode())
            ser.flush()

def resend_cached_icons():
    """Re-send cached icons after reconnection."""
    if not HAS_PIL:
        return
    for i in range(NUM_BUTTONS):
        icon_path = os.path.join(ICON_DIR, f"btn_{i}.png")
        if config[i].get("has_icon") and os.path.exists(icon_path):
            try:
                img = Image.open(icon_path).convert("RGBA")
                pixel_size = ICON_SIZES[config[i].get("icon_size", 1)]
                img = img.resize((pixel_size, pixel_size), Image.LANCZOS)
                bg_color = tuple(config[i].get("color", [0, 0, 0]))
                b64 = image_to_rgb565_base64(img, bg_color)
                send_icon_to_esp32(i, b64, pixel_size)
                time.sleep(0.05)
            except Exception:
                pass

# ─── Serial ───
def sync_button_to_esp32(idx):
    with ser_lock:
        if ser and ser.is_open:
            btn = config[idx]
            r, g, b = btn["color"]
            isz = btn.get("icon_size", 1)
            bst = btn.get("border_style", 0)
            shl = 1 if btn.get("show_label", True) else 0
            cmd = f"SET:{idx}:{btn['label']}:{r},{g},{b}:{isz},{bst},{shl}\n"
            ser.write(cmd.encode())
            ser.flush()

def sync_all_to_esp32():
    time.sleep(1)
    for i in range(NUM_BUTTONS):
        sync_button_to_esp32(i)
        time.sleep(0.05)
    resend_cached_icons()

def open_windows_app(app_name):
    """Find and open an app on Windows by name."""
    name_lower = app_name.lower()

    # 1. Search Start Menu shortcuts (.lnk) - most reliable
    start_dirs = [
        os.path.join(os.environ.get("PROGRAMDATA", "C:\\ProgramData"),
                     "Microsoft", "Windows", "Start Menu", "Programs"),
        os.path.join(os.environ.get("APPDATA", ""),
                     "Microsoft", "Windows", "Start Menu", "Programs"),
    ]
    for start_dir in start_dirs:
        if not os.path.isdir(start_dir):
            continue
        for root, dirs, files in os.walk(start_dir):
            for f in files:
                if f.lower().endswith(".lnk") and name_lower in f.lower():
                    os.startfile(os.path.join(root, f))
                    return

    # 2. Try as direct command (works for apps in PATH like notepad, calc)
    try:
        subprocess.Popen(app_name, shell=True)
        return
    except Exception:
        pass

    # 3. Try as URI protocol (spotify:, discord:, etc.)
    try:
        os.startfile(f"{name_lower}:")
    except Exception:
        print(f"[App] No se encontro: {app_name}")

def open_url_or_file(target):
    """Cross-platform open URL/file."""
    if IS_MAC:
        subprocess.Popen(["open", target])
    elif IS_WINDOWS:
        os.startfile(target)
    else:
        subprocess.Popen(["xdg-open", target])

# Mac key code map
MAC_KEYCODES = {
    "a":0,"s":1,"d":2,"f":3,"h":4,"g":5,"z":6,"x":7,"c":8,"v":9,
    "b":11,"q":12,"w":13,"e":14,"r":15,"y":16,"t":17,"1":18,"2":19,
    "3":20,"4":21,"6":22,"5":23,"=":24,"9":25,"7":26,"-":27,"8":28,
    "0":29,"o":31,"u":32,"i":34,"p":35,"l":37,"j":38,"k":40,"n":45,
    "m":46,
    "space":49,"return":36,"tab":48,"escape":53,"delete":51,
    "up":126,"down":125,"left":123,"right":124,
    "f1":122,"f2":120,"f3":99,"f4":118,"f5":96,"f6":97,
    "f7":98,"f8":100,"f9":101,"f10":109,"f11":103,"f12":111,
}

# Mac media key types (NX_KEYTYPE)
MAC_MEDIA_KEYS = {
    "vol_up": 0, "vol_down": 1, "vol_mute": 7,
    "play_pause": 16, "next_track": 17, "prev_track": 20,
    "brightness_up": 21, "brightness_down": 22,
}

def mac_media_key(key_type):
    """Simulate a media key press via Quartz CGEvent (no Accessibility perms needed)."""
    import Quartz
    # Key down
    ev = Quartz.NSEvent.otherEventWithType_location_modifierFlags_timestamp_windowNumber_context_subtype_data1_data2_(
        14, (0, 0), 0xa00, 0, 0, 0, 8, (key_type << 16) | (0xa << 8), -1)
    Quartz.CGEventPost(0, ev.CGEvent())
    # Key up
    ev_up = Quartz.NSEvent.otherEventWithType_location_modifierFlags_timestamp_windowNumber_context_subtype_data1_data2_(
        14, (0, 0), 0xb00, 0, 0, 0, 8, (key_type << 16) | (0xb << 8), -1)
    Quartz.CGEventPost(0, ev_up.CGEvent())

def mac_key_combo(key_code, modifiers=0):
    """Simulate a keyboard shortcut via Quartz CGEvent."""
    import Quartz
    src = Quartz.CGEventSourceCreate(Quartz.kCGEventSourceStateHIDSystemState)
    ev = Quartz.CGEventCreateKeyboardEvent(src, key_code, True)
    if modifiers:
        Quartz.CGEventSetFlags(ev, modifiers)
    Quartz.CGEventPost(Quartz.kCGHIDEventTap, ev)
    ev_up = Quartz.CGEventCreateKeyboardEvent(src, key_code, False)
    if modifiers:
        Quartz.CGEventSetFlags(ev_up, modifiers)
    Quartz.CGEventPost(Quartz.kCGHIDEventTap, ev_up)

def run_keyboard(action):
    """Send keyboard shortcut. Format: 'cmd+shift+a' or special keys like 'vol_up'."""
    action = action.strip().lower()

    # ─── Media/system keys ───
    if IS_MAC and action in MAC_MEDIA_KEYS:
        mac_media_key(MAC_MEDIA_KEYS[action])
        return

    if IS_MAC and action == "screenshot":
        import Quartz
        mac_key_combo(20, Quartz.kCGEventFlagMaskCommand | Quartz.kCGEventFlagMaskShift)  # cmd+shift+3
        return

    if IS_WINDOWS:
        win_special = {
            "vol_up": 175, "vol_down": 174, "vol_mute": 173,
            "play_pause": 179, "next_track": 176, "prev_track": 177,
        }
        if action in win_special:
            subprocess.Popen(f'powershell -c "(New-Object -ComObject WScript.Shell).SendKeys([char]{win_special[action]})"', shell=True)
            return
        if action == "screenshot":
            subprocess.Popen("snippingtool", shell=True)
            return

    # ─── Parse combo: "cmd+shift+a" ───
    parts = [p.strip() for p in action.split("+")]
    key = parts[-1]
    mods = [p for p in parts[:-1]]

    if IS_MAC:
        import Quartz
        mod_map = {
            "cmd": Quartz.kCGEventFlagMaskCommand,
            "command": Quartz.kCGEventFlagMaskCommand,
            "ctrl": Quartz.kCGEventFlagMaskControl,
            "control": Quartz.kCGEventFlagMaskControl,
            "alt": Quartz.kCGEventFlagMaskAlternate,
            "option": Quartz.kCGEventFlagMaskAlternate,
            "shift": Quartz.kCGEventFlagMaskShift,
        }
        flags = 0
        for m in mods:
            flags |= mod_map.get(m, 0)
        kc = MAC_KEYCODES.get(key)
        if kc is not None:
            mac_key_combo(kc, flags)
        else:
            print(f"[Keyboard] Tecla desconocida: {key}")

    elif IS_WINDOWS:
        key_map = {"cmd": "^", "ctrl": "^", "alt": "%", "shift": "+"}
        prefix = "".join(key_map.get(m, "") for m in mods)
        subprocess.Popen(f'powershell -c "(New-Object -ComObject WScript.Shell).SendKeys(\'{prefix}{key}\')"', shell=True)

def execute_action(idx):
    if idx < 0 or idx >= len(config):
        return
    btn = config[idx]
    action = btn.get("action", "").strip()
    action_type = btn.get("action_type", "url")
    if not action:
        return
    print(f"  Boton {idx+1} ({btn['label']}): {action_type} -> {action}")
    if action_type == "url":
        open_url_or_file(action)
    elif action_type == "app":
        if IS_MAC:
            subprocess.Popen(["open", "-a", action])
        elif IS_WINDOWS:
            open_windows_app(action)
        else:
            subprocess.Popen([action])
    elif action_type == "keyboard":
        run_keyboard(action)
    elif action_type == "shell":
        subprocess.Popen(action, shell=True)

def find_esp32_port():
    for port in serial.tools.list_ports.comports():
        desc = (port.description or "").lower()
        if "ch340" in desc or "ch341" in desc or "usbserial" in port.device.lower():
            return port.device
        # Windows: CH340 appears as COMx
        if IS_WINDOWS and ("ch340" in desc or "ch341" in desc or "usb-serial" in desc or "usb serial" in desc):
            return port.device
    # Mac/Linux fallback
    if not IS_WINDOWS:
        candidates = globmod.glob("/dev/cu.usbserial-*") + globmod.glob("/dev/cu.usbmodem*")
        return candidates[0] if candidates else None
    return None

browser_opened = False

def serial_listener():
    global ser, browser_opened
    while True:
        port = find_esp32_port()
        if not port:
            time.sleep(3)
            continue
        try:
            with ser_lock:
                ser = serial.Serial(port, BAUD, timeout=1)
            print(f"[Serial] Conectado a {port}")

            # Open browser when ESP32 is plugged in
            if not browser_opened or "--daemon" in sys.argv:
                webbrowser.open(f"http://localhost:{PORT_WEB}")
                browser_opened = True

            sync_all_to_esp32()
            last_ping = time.time()
            while True:
                # Send periodic PING to keep connection LED alive
                if time.time() - last_ping > 3:
                    with ser_lock:
                        if ser and ser.is_open:
                            ser.write(b"PING\n")
                            ser.flush()
                    last_ping = time.time()

                with ser_lock:
                    if not ser or not ser.is_open:
                        break
                    raw = ser.readline()
                if raw:
                    line = raw.decode("utf-8", errors="ignore").strip()
                    if line.startswith("BTN:"):
                        idx = int(line[4:])
                        execute_action(idx)
                    elif line == "OPENCONFIG":
                        webbrowser.open(f"http://localhost:{PORT_WEB}")
        except serial.SerialException:
            print("[Serial] Desconectado. Esperando reconexion...")
            with ser_lock:
                ser = None
            browser_opened = False  # Re-open browser on next plug-in
            time.sleep(2)
        except Exception as e:
            print(f"[Serial] Error: {e}")
            time.sleep(2)

# ─── HTML UI ───
HTML_PAGE = r"""<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>DIY Stream Deck</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
    background: #1a1a2e;
    color: #eee;
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 30px 20px;
  }
  h1 {
    font-size: 28px;
    margin-bottom: 8px;
    background: linear-gradient(135deg, #667eea, #764ba2);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
  }
  .subtitle { color: #888; margin-bottom: 30px; font-size: 14px; }
  .status {
    display: inline-block;
    padding: 4px 12px;
    border-radius: 20px;
    font-size: 12px;
    margin-bottom: 20px;
  }
  .status.connected { background: #0d3320; color: #4ade80; }
  .status.disconnected { background: #3b1111; color: #f87171; }

  .grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 14px;
    max-width: 700px;
    width: 100%;
    margin-bottom: 30px;
  }
  .btn-card {
    aspect-ratio: 1.3;
    border-radius: 16px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    transition: all 0.15s;
    position: relative;
    box-shadow: 0 4px 15px rgba(0,0,0,0.4);
    border: 2px solid transparent;
    gap: 6px;
  }
  .btn-card:hover {
    transform: translateY(-3px);
    box-shadow: 0 8px 25px rgba(0,0,0,0.5);
    border-color: rgba(255,255,255,0.3);
  }
  .btn-card .favicon {
    width: 32px;
    height: 32px;
    border-radius: 6px;
    object-fit: contain;
  }
  .btn-card .label {
    font-size: 16px;
    font-weight: 700;
    text-shadow: 0 1px 3px rgba(0,0,0,0.5);
  }
  .btn-card .action-hint {
    font-size: 11px;
    opacity: 0.7;
    max-width: 90%;
    text-align: center;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .btn-card .edit-icon {
    position: absolute;
    top: 8px;
    right: 10px;
    font-size: 14px;
    opacity: 0;
    transition: opacity 0.2s;
  }
  .btn-card:hover .edit-icon { opacity: 0.8; }

  .overlay {
    display: none;
    position: fixed;
    inset: 0;
    background: rgba(0,0,0,0.7);
    z-index: 100;
    align-items: center;
    justify-content: center;
  }
  .overlay.active { display: flex; }
  .modal {
    background: #16213e;
    border-radius: 20px;
    padding: 30px;
    width: 440px;
    max-width: 95vw;
    box-shadow: 0 20px 60px rgba(0,0,0,0.5);
  }
  .modal h2 { margin-bottom: 20px; font-size: 20px; }
  .field { margin-bottom: 16px; }
  .field label { display: block; font-size: 13px; color: #aaa; margin-bottom: 6px; }
  .field input, .field select {
    width: 100%;
    padding: 10px 14px;
    border-radius: 10px;
    border: 1px solid #333;
    background: #0f1a30;
    color: #eee;
    font-size: 15px;
    outline: none;
  }
  .field input:focus, .field select:focus { border-color: #667eea; }
  .field input[type="color"] { height: 44px; padding: 4px; cursor: pointer; }
  .modal-actions { display: flex; gap: 10px; margin-top: 24px; }
  .modal-actions button {
    flex: 1; padding: 12px; border-radius: 10px; border: none;
    font-size: 15px; font-weight: 600; cursor: pointer; transition: background 0.2s;
  }
  .btn-save { background: #667eea; color: white; }
  .btn-save:hover { background: #5a6fd6; }
  .btn-cancel { background: #2a2a3e; color: #aaa; }
  .btn-cancel:hover { background: #333350; }
  .btn-delete { background: #dc2626; color: white; }
  .btn-delete:hover { background: #b91c1c; }

  .preview {
    width: 70px; height: 70px; border-radius: 14px;
    margin: 0 auto 16px; display: flex; flex-direction: column;
    align-items: center; justify-content: center; gap: 4px;
    font-weight: 700; font-size: 14px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.3);
    overflow: hidden;
  }
  .preview img { width: 32px; height: 32px; border-radius: 4px; }

  .type-pills { display: flex; gap: 6px; }
  .type-pills label {
    flex: 1; text-align: center; padding: 8px; border-radius: 8px;
    background: #0f1a30; border: 1px solid #333; cursor: pointer;
    font-size: 13px; transition: all 0.2s;
  }
  .type-pills input { display: none; }
  .type-pills input:checked + span { color: #667eea; }
  .type-pills label:has(input:checked) { border-color: #667eea; background: #1a2540; }
  .action-help { font-size: 11px; color: #666; margin-top: 6px; }

  .icon-status {
    font-size: 12px; color: #888; margin-top: 4px; text-align: center;
  }
  .icon-status.loading { color: #667eea; }
  .icon-status.ok { color: #4ade80; }
  .icon-status.err { color: #f87171; }

  .saving .btn-save { opacity: 0.6; pointer-events: none; }

  .preset-grid {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 6px;
  }
  .preset-btn {
    padding: 8px 4px;
    border-radius: 8px;
    background: #0f1a30;
    border: 1px solid #333;
    color: #ccc;
    font-size: 12px;
    cursor: pointer;
    text-align: center;
    transition: all 0.15s;
  }
  .preset-btn:hover { border-color: #667eea; color: #fff; background: #1a2540; }
  .preset-btn .preset-icon { font-size: 18px; display: block; margin-bottom: 2px; }

  .separator { border-top: 1px solid #2a2a3e; margin: 20px 0; }

  .upload-row { display: flex; align-items: center; gap: 10px; }
  .upload-btn {
    padding: 8px 16px; border-radius: 8px; background: #667eea; color: white;
    font-size: 13px; font-weight: 600; cursor: pointer; transition: background 0.2s;
  }
  .upload-btn:hover { background: #5a6fd6; }
  .upload-name { font-size: 13px; color: #888; flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .upload-clear {
    background: #dc2626; color: white; border: none; border-radius: 6px;
    width: 24px; height: 24px; cursor: pointer; font-size: 14px; line-height: 1;
  }

  .toggle-row {
    display: flex; align-items: center; justify-content: space-between;
    cursor: pointer; position: relative;
  }
  .toggle-row input { display: none; }
  .toggle-switch {
    width: 44px; height: 24px; background: #333; border-radius: 12px;
    position: relative; transition: background 0.2s;
  }
  .toggle-switch::after {
    content: ""; position: absolute; top: 2px; left: 2px;
    width: 20px; height: 20px; background: #888; border-radius: 50%;
    transition: all 0.2s;
  }
  .toggle-row input:checked + .toggle-switch { background: #667eea; }
  .toggle-row input:checked + .toggle-switch::after { left: 22px; background: white; }
</style>
</head>
<body>

<h1>DIY Stream Deck</h1>
<p class="subtitle">Pulsa un boton para editarlo</p>
<div class="status disconnected" id="status">Desconectado</div>

<div class="grid" id="grid"></div>

<div class="overlay" id="overlay">
  <div class="modal" id="modal">
    <div class="preview" id="preview"></div>
    <div class="icon-status" id="iconStatus"></div>
    <h2>Editar boton <span id="editIdx"></span></h2>

    <div class="field">
      <label>Nombre</label>
      <input type="text" id="editLabel" maxlength="19" placeholder="Ej: Amazon">
    </div>

    <div class="field">
      <label>Color</label>
      <input type="color" id="editColor">
    </div>

    <div class="field">
      <label>Tipo de accion</label>
      <div class="type-pills">
        <label><input type="radio" name="actionType" value="url"><span>URL</span></label>
        <label><input type="radio" name="actionType" value="app"><span>App</span></label>
        <label><input type="radio" name="actionType" value="keyboard"><span>Teclado</span></label>
        <label><input type="radio" name="actionType" value="shell"><span>Comando</span></label>
      </div>
    </div>

    <div class="field" id="presetField" style="display:none">
      <label>Atajos rapidos</label>
      <div class="preset-grid" id="presetGrid"></div>
    </div>

    <div class="field">
      <label>Accion</label>
      <input type="text" id="editAction" placeholder="https://www.amazon.es">
      <div class="action-help" id="actionHelp">Escribe la URL que quieres abrir</div>
    </div>

    <div class="separator"></div>

    <div class="field">
      <label>Icono personalizado</label>
      <div class="upload-row">
        <label class="upload-btn" for="iconUpload">Subir imagen</label>
        <input type="file" id="iconUpload" accept="image/*" style="display:none">
        <span class="upload-name" id="uploadName">Sin icono propio</span>
        <button class="upload-clear" id="uploadClear" onclick="clearCustomIcon()" style="display:none">x</button>
      </div>
    </div>

    <div class="field">
      <label>Tamano del icono</label>
      <div class="type-pills">
        <label><input type="radio" name="iconSize" value="0"><span>S</span></label>
        <label><input type="radio" name="iconSize" value="1"><span>M</span></label>
        <label><input type="radio" name="iconSize" value="2"><span>L</span></label>
        <label><input type="radio" name="iconSize" value="3"><span>XL</span></label>
      </div>
    </div>

    <div class="field">
      <label>Marco</label>
      <div class="type-pills">
        <label><input type="radio" name="borderStyle" value="0"><span>Ninguno</span></label>
        <label><input type="radio" name="borderStyle" value="1"><span>Fino</span></label>
        <label><input type="radio" name="borderStyle" value="2"><span>Grueso</span></label>
        <label><input type="radio" name="borderStyle" value="3"><span>Glow</span></label>
      </div>
    </div>

    <div class="field">
      <label class="toggle-row">
        <span>Mostrar texto</span>
        <input type="checkbox" id="showLabel" checked>
        <span class="toggle-switch"></span>
      </label>
    </div>

    <div class="modal-actions">
      <button class="btn-cancel" onclick="closeModal()">Cancelar</button>
      <button class="btn-delete" onclick="clearButton()">Limpiar</button>
      <button class="btn-save" id="btnSave" onclick="saveButton()">Guardar</button>
    </div>
  </div>
</div>

<script>
let buttons = [];
let editingIdx = -1;

const helpTexts = {
  url: "Escribe la URL que quieres abrir (ej: https://youtube.com)",
  app: "Nombre de la app tal como aparece en tu PC (ej: Spotify, Discord, Chrome)",
  keyboard: "Atajo de teclado (ej: cmd+shift+a) o elige uno rapido arriba",
  shell: "Comando de terminal (ej: say hola)"
};
const placeholders = {
  url: "https://www.amazon.es",
  app: "Spotify",
  keyboard: "cmd+shift+a",
  shell: "say hola"
};
const keyboardPresets = [
  {icon: "🔊", label: "Vol +", value: "vol_up"},
  {icon: "🔉", label: "Vol -", value: "vol_down"},
  {icon: "🔇", label: "Mute", value: "vol_mute"},
  {icon: "⏯", label: "Play/Pausa", value: "play_pause"},
  {icon: "⏭", label: "Siguiente", value: "next_track"},
  {icon: "⏮", label: "Anterior", value: "prev_track"},
  {icon: "📸", label: "Captura", value: "screenshot"},
  {icon: "📋", label: "Copiar", value: "cmd+c"},
  {icon: "📌", label: "Pegar", value: "cmd+v"},
  {icon: "↩", label: "Deshacer", value: "cmd+z"},
  {icon: "💾", label: "Guardar", value: "cmd+s"},
  {icon: "🔍", label: "Buscar", value: "cmd+f"},
];

function faviconUrl(action) {
  try {
    let domain = action.replace("https://","").replace("http://","").split("/")[0];
    return `https://www.google.com/s2/favicons?domain=${domain}&sz=64`;
  } catch(e) { return ""; }
}

async function loadButtons() {
  const r = await fetch("/api/config");
  buttons = await r.json();
  renderGrid();
}

function rgbToHex(r, g, b) {
  return "#" + [r,g,b].map(x => x.toString(16).padStart(2,"0")).join("");
}
function hexToRgb(hex) {
  const m = hex.match(/^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i);
  return m ? [parseInt(m[1],16), parseInt(m[2],16), parseInt(m[3],16)] : [100,100,100];
}

function renderGrid() {
  const grid = document.getElementById("grid");
  grid.innerHTML = "";
  buttons.forEach((btn, i) => {
    const [r,g,b] = btn.color;
    const card = document.createElement("div");
    card.className = "btn-card";
    card.style.background = `rgb(${r},${g},${b})`;

    let iconHtml = "";
    if (btn.action && btn.action_type === "url") {
      iconHtml = `<img class="favicon" src="${faviconUrl(btn.action)}" onerror="this.style.display='none'">`;
    }

    let hintText = "Sin accion";
    if (btn.action) {
      hintText = btn.action_type === "url"
        ? btn.action.replace("https://","").replace("http://","").split("/")[0]
        : btn.action;
    }

    card.innerHTML = `
      <span class="edit-icon">&#9998;</span>
      ${iconHtml}
      <span class="label">${btn.label}</span>
      <span class="action-hint">${hintText}</span>
    `;
    card.onclick = () => openModal(i);
    grid.appendChild(card);
  });
}

let customIconData = null;

function openModal(idx) {
  editingIdx = idx;
  const btn = buttons[idx];
  document.getElementById("editIdx").textContent = idx + 1;
  document.getElementById("editLabel").value = btn.label;
  document.getElementById("editColor").value = rgbToHex(...btn.color);
  document.getElementById("editAction").value = btn.action || "";
  document.querySelector(`input[name="actionType"][value="${btn.action_type || "url"}"]`).checked = true;
  document.querySelector(`input[name="iconSize"][value="${btn.icon_size || 1}"]`).checked = true;
  document.querySelector(`input[name="borderStyle"][value="${btn.border_style || 0}"]`).checked = true;
  document.getElementById("showLabel").checked = btn.show_label !== false;
  document.getElementById("iconStatus").textContent = "";
  document.getElementById("modal").classList.remove("saving");
  customIconData = null;
  document.getElementById("uploadName").textContent = "Sin icono propio";
  document.getElementById("uploadClear").style.display = "none";
  document.getElementById("iconUpload").value = "";
  updatePreview();
  updateHelp();
  document.getElementById("overlay").classList.add("active");
  document.getElementById("editLabel").focus();
}

document.getElementById("iconUpload").addEventListener("change", function(e) {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = function(ev) {
    customIconData = ev.target.result.split(",")[1]; // base64 part
    document.getElementById("uploadName").textContent = file.name;
    document.getElementById("uploadClear").style.display = "";
    updatePreview();
  };
  reader.readAsDataURL(file);
});

function clearCustomIcon() {
  customIconData = null;
  document.getElementById("iconUpload").value = "";
  document.getElementById("uploadName").textContent = "Sin icono propio";
  document.getElementById("uploadClear").style.display = "none";
  updatePreview();
}

function closeModal() {
  document.getElementById("overlay").classList.remove("active");
  editingIdx = -1;
}

function updatePreview() {
  const p = document.getElementById("preview");
  const color = document.getElementById("editColor").value;
  const label = document.getElementById("editLabel").value || "?";
  const action = document.getElementById("editAction").value;
  const type = document.querySelector('input[name="actionType"]:checked').value;
  p.style.background = color;

  let html = "";
  if (action && type === "url") {
    html += `<img src="${faviconUrl(action)}" onerror="this.style.display='none'">`;
  }
  html += `<span>${label}</span>`;
  p.innerHTML = html;
}

function updateHelp() {
  const t = document.querySelector('input[name="actionType"]:checked').value;
  document.getElementById("actionHelp").textContent = helpTexts[t];
  document.getElementById("editAction").placeholder = placeholders[t];

  const presetField = document.getElementById("presetField");
  const presetGrid = document.getElementById("presetGrid");
  if (t === "keyboard") {
    presetGrid.innerHTML = "";
    keyboardPresets.forEach(p => {
      const btn = document.createElement("div");
      btn.className = "preset-btn";
      btn.innerHTML = `<span class="preset-icon">${p.icon}</span>${p.label}`;
      btn.onclick = () => {
        document.getElementById("editAction").value = p.value;
        document.getElementById("editLabel").value = p.label;
        updatePreview();
      };
      presetGrid.appendChild(btn);
    });
    presetField.style.display = "";
  } else {
    presetField.style.display = "none";
  }
}

document.getElementById("editLabel").addEventListener("input", updatePreview);
document.getElementById("editColor").addEventListener("input", updatePreview);
document.getElementById("editAction").addEventListener("input", updatePreview);
document.querySelectorAll('input[name="actionType"]').forEach(r => r.addEventListener("change", () => { updateHelp(); updatePreview(); }));
document.getElementById("overlay").addEventListener("click", e => { if (e.target.id === "overlay") closeModal(); });

async function saveButton() {
  const idx = editingIdx;
  const label = document.getElementById("editLabel").value || String(idx + 1);
  const color = hexToRgb(document.getElementById("editColor").value);
  const action = document.getElementById("editAction").value;
  const action_type = document.querySelector('input[name="actionType"]:checked').value;
  const icon_size = parseInt(document.querySelector('input[name="iconSize"]:checked').value);
  const border_style = parseInt(document.querySelector('input[name="borderStyle"]:checked').value);
  const show_label = document.getElementById("showLabel").checked;

  const iconStatus = document.getElementById("iconStatus");
  const modal = document.getElementById("modal");
  modal.classList.add("saving");

  if (customIconData || (action && (action_type === "url" || action_type === "app"))) {
    iconStatus.textContent = customIconData ? "Enviando icono..." : "Descargando icono...";
    iconStatus.className = "icon-status loading";
  }

  const payload = {label, color, action, action_type, icon_size, border_style, show_label};
  if (customIconData) payload.custom_icon = customIconData;

  const r = await fetch("/api/config/" + idx, {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(payload)
  });

  if (r.ok) {
    const result = await r.json();
    if (result.icon_sent) {
      iconStatus.textContent = "Icono enviado al Stream Deck";
      iconStatus.className = "icon-status ok";
    } else if (action && (action_type === "url" || action_type === "app")) {
      iconStatus.textContent = "No se pudo obtener el icono";
      iconStatus.className = "icon-status err";
    }
    buttons[idx] = {label, color, action, action_type, icon_size, border_style, show_label, has_icon: result.icon_sent || false};
    renderGrid();
    setTimeout(() => { closeModal(); modal.classList.remove("saving"); }, 600);
  } else {
    modal.classList.remove("saving");
  }
}

async function clearButton() {
  const idx = editingIdx;
  document.getElementById("editLabel").value = String(idx + 1);
  document.getElementById("editAction").value = "";
  document.querySelector('input[name="actionType"][value="url"]').checked = true;
  document.querySelector('input[name="iconSize"][value="1"]').checked = true;
  document.querySelector('input[name="borderStyle"][value="0"]').checked = true;
  document.getElementById("showLabel").checked = true;
  clearCustomIcon();
  updateHelp();
  updatePreview();
  await saveButton();
}

async function checkStatus() {
  try {
    const r = await fetch("/api/status");
    const data = await r.json();
    const el = document.getElementById("status");
    if (data.connected) {
      el.textContent = "Conectado";
      el.className = "status connected";
    } else {
      el.textContent = "Desconectado - conecta el Stream Deck por USB";
      el.className = "status disconnected";
    }
  } catch(e) {}
}

loadButtons();
setInterval(checkStatus, 2000);
checkStatus();
</script>
</body>
</html>"""

# ─── HTTP Server ───
class DeckHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def _send_json(self, data, code=200):
        body = json.dumps(data).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_html(self, html):
        body = html.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/":
            self._send_html(HTML_PAGE)
        elif self.path == "/api/config":
            self._send_json(config)
        elif self.path == "/api/status":
            with ser_lock:
                connected = ser is not None and ser.is_open
            self._send_json({"connected": connected, "has_pillow": HAS_PIL})
        else:
            self.send_error(404)

    def do_POST(self):
        if self.path.startswith("/api/config/"):
            idx = int(self.path.split("/")[-1])
            if idx < 0 or idx >= NUM_BUTTONS:
                self.send_error(400)
                return
            length = int(self.headers.get("Content-Length", 0))
            body = json.loads(self.rfile.read(length))

            config[idx]["label"] = body.get("label", str(idx+1))[:19]
            config[idx]["color"] = body.get("color", [100,100,100])
            config[idx]["action"] = body.get("action", "")
            config[idx]["action_type"] = body.get("action_type", "url")
            config[idx]["icon_size"] = body.get("icon_size", 1)
            config[idx]["border_style"] = body.get("border_style", 0)
            config[idx]["show_label"] = body.get("show_label", True)

            save_config()
            sync_button_to_esp32(idx)

            icon_sent = False
            try:
                custom_icon_b64 = body.get("custom_icon")
                if custom_icon_b64 and HAS_PIL:
                    print(f"[Icon] Boton {idx+1}: procesando icono subido ({len(custom_icon_b64)} chars)")
                    img_data = base64.b64decode(custom_icon_b64)
                    img = Image.open(io.BytesIO(img_data)).convert("RGBA")
                    print(f"[Icon] Imagen decodificada: {img.size}")
                    process_icon_for_button(idx, custom_img=img)
                    icon_sent = config[idx].get("has_icon", False)
                elif config[idx]["action"] and config[idx]["action_type"] in ("url", "app"):
                    print(f"[Icon] Boton {idx+1}: buscando icono para {config[idx]['action_type']} '{config[idx]['action']}'")
                    process_icon_for_button(idx)
                    icon_sent = config[idx].get("has_icon", False)
                else:
                    send_no_icon(idx)
                    config[idx]["has_icon"] = False
            except Exception as e:
                print(f"[Icon] ERROR procesando icono boton {idx+1}: {e}")
                import traceback
                traceback.print_exc()

            save_config()
            self._send_json({"ok": True, "icon_sent": icon_sent})
        else:
            self.send_error(404)

# ─── Auto-start setup ───
APP_DIR = APP_BASE
PLIST_NAME = "com.diy.streamdeck"
PLIST_PATH = os.path.expanduser(f"~/Library/LaunchAgents/{PLIST_NAME}.plist")
WIN_STARTUP = os.path.join(os.environ.get("APPDATA", ""), "Microsoft", "Windows", "Start Menu", "Programs", "Startup")
WIN_VBS_PATH = os.path.join(WIN_STARTUP, "StreamDeck.vbs") if IS_WINDOWS else ""

def install_autostart():
    """Install auto-start so the app runs at login."""
    if getattr(sys, 'frozen', False):
        exe_path = sys.executable
    else:
        exe_path = None

    python_path = sys.executable
    script_path = os.path.abspath(__file__)

    if IS_MAC:
        if exe_path and 'StreamDeck' in exe_path:
            # Running as compiled .app
            prog_args = f"""<string>{exe_path}</string>
        <string>--daemon</string>"""
        else:
            # Running as Python script
            prog_args = f"""<string>{python_path}</string>
        <string>{script_path}</string>
        <string>--daemon</string>"""

        plist = f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>{PLIST_NAME}</string>
    <key>ProgramArguments</key>
    <array>
        {prog_args}
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>WorkingDirectory</key>
    <string>{APP_DIR}</string>
    <key>StandardOutPath</key>
    <string>/tmp/streamdeck.log</string>
    <key>StandardErrorPath</key>
    <string>/tmp/streamdeck.log</string>
</dict>
</plist>"""
        os.makedirs(os.path.dirname(PLIST_PATH), exist_ok=True)
        with open(PLIST_PATH, "w") as f:
            f.write(plist)
        subprocess.run(["launchctl", "load", PLIST_PATH])
        print(f"  Auto-arranque instalado (Mac LaunchAgent)")
        print(f"  El Stream Deck se abrira solo al conectar el USB.")
        print(f"  Para desinstalar: python3 streamdeck_app.py --uninstall")

    elif IS_WINDOWS:
        if exe_path and exe_path.endswith('.exe'):
            run_cmd = f'"""{exe_path}"" --daemon"'
        else:
            run_cmd = f'"""{python_path}"" ""{script_path}"" --daemon"'
        vbs = f'Set ws = CreateObject("Wscript.Shell")\n'
        vbs += f'ws.Run {run_cmd}, 0, False\n'
        os.makedirs(WIN_STARTUP, exist_ok=True)
        with open(WIN_VBS_PATH, "w") as f:
            f.write(vbs)
        print(f"  Auto-arranque instalado (Windows Startup)")
        print(f"  El Stream Deck se abrira solo al conectar el USB.")

    else:
        print("  Auto-arranque no soportado en este OS.")

def uninstall_autostart():
    """Remove auto-start."""
    if IS_MAC and os.path.exists(PLIST_PATH):
        subprocess.run(["launchctl", "unload", PLIST_PATH], capture_output=True)
        os.remove(PLIST_PATH)
        print("  Auto-arranque desinstalado.")
    elif IS_WINDOWS and os.path.exists(WIN_VBS_PATH):
        os.remove(WIN_VBS_PATH)
        print("  Auto-arranque desinstalado.")
    else:
        print("  No hay auto-arranque instalado.")

# ─── Main ───
def main():
    # Handle --install / --uninstall
    if "--install" in sys.argv:
        install_autostart()
        return
    if "--uninstall" in sys.argv:
        uninstall_autostart()
        return

    load_config()
    os.makedirs(ICON_DIR, exist_ok=True)

    daemon_mode = "--daemon" in sys.argv

    t = threading.Thread(target=serial_listener, daemon=True)
    t.start()

    class ReusableHTTPServer(http.server.HTTPServer):
        allow_reuse_address = True
    server = ReusableHTTPServer(("0.0.0.0", PORT_WEB), DeckHandler)

    if daemon_mode:
        print(f"  DIY Stream Deck (modo servicio)")
        print(f"  Esperando conexion USB del Stream Deck...")
    else:
        print(f"\n  DIY Stream Deck")
        print(f"  http://localhost:{PORT_WEB}")
        print(f"  Ctrl+C para salir\n")
        webbrowser.open(f"http://localhost:{PORT_WEB}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nDetenido.")
        server.shutdown()

if __name__ == "__main__":
    main()
