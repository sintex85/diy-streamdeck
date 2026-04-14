#!/bin/bash
# ═══════════════════════════════════════════
#   Flashear ESP32-8048S043 - Stream Deck
#   Conecta el ESP32 por USB y ejecuta esto
# ═══════════════════════════════════════════

cd "$(dirname "$0")"

clear
echo ""
echo "  ╔═══════════════════════════════════╗"
echo "  ║  Flashear Stream Deck ESP32       ║"
echo "  ╚═══════════════════════════════════╝"
echo ""

# Auto-detect port
PORT=$(ls /dev/cu.usbserial-* 2>/dev/null | head -1)
if [ -z "$PORT" ]; then
    PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
fi

if [ -z "$PORT" ]; then
    echo "  ❌ No se detecta ningun ESP32."
    echo "  Conectalo por USB e intenta de nuevo."
    echo ""
    read -p "  Pulsa Enter para salir..."
    exit 1
fi

echo "  ESP32 detectado en: $PORT"
echo ""
echo "  Paso 1/2: Borrando flash..."

python3 esptool.py --chip esp32s3 --port "$PORT" erase_flash

if [ $? -ne 0 ]; then
    echo "  ❌ Error borrando flash."
    read -p "  Pulsa Enter para salir..."
    exit 1
fi

echo ""
echo "  Paso 2/2: Escribiendo firmware..."

python3 esptool.py --chip esp32s3 --port "$PORT" --baud 460800 \
    write_flash \
    --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x0000 button_counter.ino.bootloader.bin \
    0x8000 button_counter.ino.partitions.bin \
    0xe000 boot_app0.bin \
    0x10000 button_counter.ino.bin

if [ $? -eq 0 ]; then
    echo ""
    echo "  ✅ ¡Flasheado correctamente!"
    echo "  El Stream Deck esta listo para usar."
else
    echo ""
    echo "  ❌ Error escribiendo firmware."
fi

echo ""
read -p "  Pulsa Enter para salir..."
