# DIY Stream Deck con ESP32-8048S043

## Que es

Stream Deck casero con pantalla tactil 4.3". 12 botones configurables desde Chrome. Abre URLs, lanza apps, controla volumen, atajos de teclado. Iconos personalizados.

## Hardware

- **Placa**: ESP32-8048S043 (ESP32-S3, pantalla 800x480 RGB, touch GT911, CH340 USB)
- **Conexion**: USB para alimentacion + config. Bluetooth para atajos de teclado.

## Como funciona

1. **USB**: conecta al PC, abre `sintex85.github.io/diy-streamdeck` en Chrome, pulsa "Conectar USB"
2. **Config**: edita botones desde la web (URL, app, teclado, texto, iconos)
3. **Bluetooth**: empareja "StreamDeck" para volumen y atajos (sin Chrome)
4. **Deja Chrome abierto** para que URLs y apps funcionen al pulsar botones

### Tipos de accion

| Tipo | Como funciona | Necesita Chrome |
|------|---------------|-----------------|
| URL | Chrome abre la web | Si |
| App | Chrome abre protocolo (spotify:, discord:) | Si |
| Teclado | BLE envia teclas directo al OS | No |
| Texto | BLE escribe texto | No |

## Flashear un ESP32 nuevo

Conecta el ESP32 por USB. Detecta el puerto:
- Mac: `ls /dev/cu.usbserial-*`
- Windows: Administrador de dispositivos > Puertos COM
- Driver CH340 si no aparece: wch.cn/downloads/CH341SER_EXE.html

```bash
python3 firmware/esptool.py --chip esp32s3 --port PUERTO --baud 460800 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0000 firmware/button_counter.ino.bootloader.bin \
  0x8000 firmware/button_counter.ino.partitions.bin \
  0xe000 firmware/boot_app0.bin \
  0x10000 firmware/button_counter.ino.bin
```

O usa los scripts: `firmware/flashear_mac.command` / `firmware/flashear_windows.bat`

## Recompilar (solo si modificas el codigo)

```bash
# Requisitos
arduino-cli core install esp32:esp32@2.0.17
arduino-cli lib install "LovyanGFX@1.2.19"
# NimBLE 2.4.0 + ESP32-BLE-Keyboard 0.4.0 (wakwak-koba fork) desde GitHub

# Compilar
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,PartitionScheme=default_8MB,UploadSpeed=460800" \
  button_counter/

# Subir (para el servicio Python/Chrome antes de subir)
arduino-cli upload \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,PartitionScheme=default_8MB,UploadSpeed=460800" \
  --port PUERTO button_counter/
```

## Variante ESP32 clasico (ESP32-32E 4" ST7796) — `streamdeck_esp32/`

Segunda placa soportada: **ESP32 clasico (WROOM-32E), pantalla SPI ST7796 480x320 + touch XPT2046** (la misma placa del proyecto SPL Meter, sin PSRAM). Mismo protocolo serie -> la misma web `docs/index.html` sirve sin cambios.

- **Sketch**: `streamdeck_esp32/streamdeck_esp32.ino` + `LGFX_ESP32_ST7796.h`
- **Pinout pantalla (HSPI)**: SCLK=14, MOSI=13, MISO=12, DC=2, CS=15, RST=-1, BL=27
- **Touch XPT2046 (HSPI compartido)**: CS=33, INT=36
- **Diferencias vs S3**: layout 480x320 (4x3 = 12 botones), `malloc` en vez de `ps_malloc` (sin PSRAM), `setRotation(1)`
- **Calibracion tactil**: el XPT2046 necesita calibrarse o los toques caen en el boton vecino. En el 1er arranque (sin cal en NVS) se auto-calibra: tocar las marcas de las esquinas. Recalibrar en cualquier momento con el comando serie `CALIB`. La calibracion (`CALDATA:...`, 8 valores affine) se guarda en NVS namespace `deck` key `cal`.
- **Paginas**: 3 paginas de 12 botones (36 en total, indices globales 0-35). Navegacion con las flechas `Pag ◀/▶` de la barra lateral; indicador `1/3`. Comando serie `PAGE:n` (0-2) para cambiar de pagina (util para test). La web `docs/index.html` tiene pestañas Pagina 1/2/3 que mapean a los mismos indices globales — protocolo sin cambios, solo mas indices.
- **Iconos**: la web manda RGB565 **big-endian**; el firmware hace `lcd.setSwapBytes(true)` para que los colores salgan bien. Sin eso los iconos se ven con colores rotos.

## Agente de macOS (`mac-agent/`) — funciona con solo enchufar el cable

Sin la pestaña de Chrome abierta, las URLs/apps no se abren (la placa solo emite `BTN:` por serie; alguien en el Mac tiene que escuchar). `mac-agent/streamdeck_agent.py` es un daemon que escucha el puerto serie y hace `open <url|app>` con los botones tipo 1 (URL) y 3 (App). Los tipos 2 (teclado) y 4 (texto) los manda la placa por BLE, el agente los ignora.

- `install.command`: copia el agente a `~/Library/Application Support/StreamDeck/`, crea el plist launchd `com.bitsytornillos.streamdeck` en `~/Library/LaunchAgents/` y lo carga (arranca al iniciar sesion, `KeepAlive`). Log en `~/Library/Logs/streamdeck-agent.log`.
- `uninstall.command`: descarga y borra el agente.
- **Conflicto de puerto**: el agente y Chrome no pueden abrir el puerto a la vez. Para reconfigurar botones en la web, pausar el agente (`launchctl unload <plist>`), configurar, y reanudar (`launchctl load <plist>`).
- Requiere `pyserial` (el installer lo instala si falta).

```bash
# Compilar
arduino-cli compile \
  --fqbn "esp32:esp32:esp32:FlashSize=4M,PartitionScheme=huge_app,PSRAM=disabled,UploadSpeed=460800" \
  streamdeck_esp32/
# Subir (usar 460800; 921600 puede dar "Invalid head of packet" con algunos cables CH340)
arduino-cli upload \
  --fqbn "esp32:esp32:esp32:FlashSize=4M,PartitionScheme=huge_app,PSRAM=disabled,UploadSpeed=460800" \
  --port /dev/cu.usbserial-XXXX streamdeck_esp32/
```

## Config tecnica del hardware

- **Touch GT911**: I2C addr 0x5D, I2C_NUM_0, SDA=GPIO19, SCL=GPIO20, pin_int=-1
- **Display**: LovyanGFX, `LGFX_ESP32S3_RGB_ESP32-8048S043.h`
- **BLE**: NimBLE 2.4.0 + ESP32-BLE-Keyboard 0.4.0 (wakwak-koba fork)
- **PSRAM**: OPI (8MB), **Flash**: QIO (16MB)

## Protocolo serial (115200 baud)

| Direccion | Comando | Descripcion |
|-----------|---------|-------------|
| ESP32 -> PC | `BTN:idx:type:action` | Boton pulsado |
| PC -> ESP32 | `SET:N:label:R,G,B:sz,brd,lbl` | Config visual |
| PC -> ESP32 | `ACT:N:type:action` | Config accion |
| PC -> ESP32 | `ICON:N:size:base64` | Enviar icono RGB565 |
| PC -> ESP32 | `GETALL` | Pedir toda la config |
| PC -> ESP32 | `STATUS` | Estado BLE |

## Estructura

```
docs/index.html         -> Web de config (GitHub Pages)
button_counter/         -> Codigo fuente Arduino
firmware/               -> Binarios para flashear sin recompilar
```
