#!/usr/bin/env python3
# ============================================================================
#  StreamDeck agent (macOS)  ·  by Bits y Tornillos
#  Escucha el puerto serie del ESP32 y abre URLs / apps al pulsar botones.
#  Asi el Stream Deck funciona con solo enchufar el cable, sin Chrome.
#  Los atajos de teclado / volumen (tipo 2 y 4) los manda la placa por BLE.
# ============================================================================
import glob, time, subprocess, sys, os

try:
    import serial
except ImportError:
    sys.stderr.write("Falta pyserial. Instala con: pip3 install --user pyserial\n")
    sys.exit(1)

CONFIG_URL = "https://sintex85.github.io/diy-streamdeck/"
PORT_PATTERNS = ["/dev/cu.usbserial-*", "/dev/cu.wchusbserial*", "/dev/cu.SLAB_USBtoUART*"]

def log(msg):
    print(time.strftime("%Y-%m-%d %H:%M:%S"), msg, flush=True)

def find_port():
    for pat in PORT_PATTERNS:
        matches = glob.glob(pat)
        if matches:
            return matches[0]
    return None

def open_target(target):
    try:
        subprocess.Popen(["open", target])
    except Exception as e:
        log(f"error al abrir {target!r}: {e}")

def handle_line(line):
    # Formato: BTN:idx:type:action   (type 1=URL, 3=App -> los abre el Mac)
    if not line.startswith("BTN:"):
        return
    parts = line.split(":", 3)
    if len(parts) < 4:
        return
    idx, typ, action = parts[1], parts[2], parts[3]
    if typ in ("1", "3") and action:
        log(f"boton {idx} (tipo {typ}) -> open {action}")
        open_target(action)
    elif idx == "99":            # engranaje de config en la placa
        open_target(CONFIG_URL)

def main():
    log("StreamDeck agent iniciado")
    ser = None
    while True:
        port = find_port()
        if not port:
            time.sleep(2)
            continue
        try:
            log(f"conectando a {port}")
            ser = serial.Serial(port, 115200, timeout=1)
            log(f"conectado a {port}")
            buf = ""
            while True:
                data = ser.read(256)
                if data:
                    buf += data.decode(errors="replace")
                    while "\n" in buf:
                        line, buf = buf.split("\n", 1)
                        handle_line(line.strip())
                elif not os.path.exists(port):
                    raise IOError("puerto desaparecido (desenchufado)")
        except Exception as e:
            log(f"desconectado: {e}")
            try:
                if ser: ser.close()
            except Exception:
                pass
            time.sleep(2)    # reintenta -> se reconecta al reenchufar

if __name__ == "__main__":
    main()
