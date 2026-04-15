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
