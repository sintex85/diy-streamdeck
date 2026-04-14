# DIY Stream Deck con ESP32-8048S043

## Que es esto

Stream Deck casero con pantalla tactil. 12 botones configurables desde una web. Al pulsar un boton en la pantalla, el PC ejecuta una accion (abrir URL, lanzar app, comando de terminal). Los iconos de las webs se descargan automaticamente.

## Hardware

- **Placa**: ESP32-8048S043 (Sunton) — ESP32-S3 con pantalla tactil 4.3" 800x480 RGB, touch GT911, chip USB CH340
- **Conexion**: Cable USB al PC (datos, no solo carga)

## Flashear un ESP32 nuevo

### Requisitos previos

- Python 3 instalado
- pyserial instalado (`pip3 install pyserial`)
- El ESP32-8048S043 conectado por USB

### Pasos

1. **Detectar el puerto serial**:
   - Mac: `ls /dev/cu.usbserial-*`
   - Windows: abrir Administrador de dispositivos > Puertos COM, buscar "CH340"
   - Linux: `ls /dev/ttyUSB*`
   - Si no aparece, instalar driver CH340:
     - Mac: https://www.wch.cn/downloads/CH341SER_MAC_ZIP.html
     - Windows: https://www.wch.cn/downloads/CH341SER_EXE.html

2. **Borrar la flash**:
```bash
python3 firmware/esptool.py --chip esp32s3 --port PUERTO erase_flash
```

3. **Escribir el firmware**:
```bash
python3 firmware/esptool.py --chip esp32s3 --port PUERTO --baud 460800 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0000 firmware/button_counter.ino.bootloader.bin \
  0x8000 firmware/button_counter.ino.partitions.bin \
  0xe000 firmware/boot_app0.bin \
  0x10000 firmware/button_counter.ino.bin
```

Reemplazar `PUERTO` por el puerto detectado (ej: `/dev/cu.usbserial-8340` o `COM3`).

4. **Verificar**: la pantalla debe encenderse y mostrar una cuadricula de 12 botones de colores.

### Scripts automaticos (alternativa)

En vez de los comandos manuales, hay scripts que auto-detectan el puerto:
- Mac: doble-click en `firmware/flashear_mac.command`
- Windows: doble-click en `firmware/flashear_windows.bat`

## Recompilar el firmware (solo si modificas el codigo)

Solo necesario si cambias `button_counter/button_counter.ino`.

### Requisitos

- Arduino CLI (`brew install arduino-cli` o https://arduino.github.io/arduino-cli/)
- Core ESP32: `arduino-cli core install esp32:esp32@2.0.17`
- Libreria: `arduino-cli lib install "LovyanGFX@1.2.19"`

### Compilar

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,PartitionScheme=default_8MB,UploadSpeed=460800" \
  button_counter/
```

### Compilar y subir directo

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,PartitionScheme=default_8MB,UploadSpeed=460800" \
  button_counter/ && \
arduino-cli upload \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,PartitionScheme=default_8MB,UploadSpeed=460800" \
  --port PUERTO \
  button_counter/
```

### Exportar binarios nuevos

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,PartitionScheme=default_8MB,UploadSpeed=460800" \
  --output-dir firmware/ \
  button_counter/
```

Luego borrar `firmware/*.elf` y `firmware/*.map` (son enormes y no hacen falta para flashear).

## Configuracion tecnica del hardware

Estos valores son especificos del ESP32-8048S043. No cambiar a menos que se use otra placa.

- **Display**: RGB paralelo 16-bit, 800x480, pines definidos en `LGFX_ESP32S3_RGB_ESP32-8048S043.h`
- **Touch GT911**: I2C addr `0x5D`, I2C_NUM_0, SDA=GPIO19, SCL=GPIO20, pin_int=-1
- **Backlight**: GPIO2 (PWM)
- **PSRAM**: OPI mode (8MB)
- **Flash**: QIO mode (16MB)

## Protocolo serial (115200 baud)

Comunicacion entre el ESP32 y la app del PC:

| Direccion | Comando | Descripcion |
|-----------|---------|-------------|
| ESP32 → PC | `BTN:N` | Boton N pulsado (0-11) |
| PC → ESP32 | `SET:N:label:R,G,B` | Cambiar nombre y color del boton N |
| PC → ESP32 | `ICON:N:base64` | Enviar icono 32x32 RGB565 en base64 |
| PC → ESP32 | `NOICON:N` | Quitar icono del boton N |
| PC → ESP32 | `GETALL` | Pedir config de todos los botones |
| ESP32 → PC | `CFG:N:label:R,G,B:hasIcon` | Respuesta a GETALL |
| PC → ESP32 | `RESETALL` | Restaurar config de fabrica |

## App del PC (streamdeck_app.py)

- Servidor web en `localhost:1313`
- Auto-detecta el ESP32 por USB
- Descarga favicons automaticamente al configurar una URL
- Multiplataforma: Mac, Windows, Linux
- Dependencias: `pyserial`, `Pillow` (para iconos)

### Modos de ejecucion

```bash
python3 streamdeck_app.py              # Normal: abre navegador al arrancar
python3 streamdeck_app.py --daemon     # Servicio: abre navegador solo al detectar USB
python3 streamdeck_app.py --install    # Instalar auto-arranque al iniciar sesion
python3 streamdeck_app.py --uninstall  # Desinstalar auto-arranque
```

### Auto-arranque

Con `--install`, la app se registra para ejecutarse al iniciar sesion del usuario:
- **Mac**: crea un LaunchAgent en `~/Library/LaunchAgents/com.diy.streamdeck.plist`
- **Windows**: crea un VBS en la carpeta Startup

La app corre en segundo plano y abre el navegador automaticamente cada vez que se conecta el ESP32 por USB. Al desconectar, queda en espera. Al reconectar, vuelve a abrir el navegador.

## Estructura de archivos

```
firmware/                  → Binarios para flashear (no recompilar)
button_counter/            → Codigo fuente Arduino
streamdeck_app.py          → App PC (configurador + listener)
Instalar.command/.bat      → Instalador dependencias usuario final
Stream Deck.command/.bat   → Launcher usuario final
```
