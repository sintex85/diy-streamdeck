#!/bin/bash
# ═══════════════════════════════════════════
#   DIY Stream Deck - Instalador
#   Solo necesitas ejecutar esto UNA VEZ
# ═══════════════════════════════════════════

clear
echo ""
echo "  ╔═══════════════════════════════════╗"
echo "  ║   DIY Stream Deck - Instalador    ║"
echo "  ╚═══════════════════════════════════╝"
echo ""

# Check Python 3
if ! command -v python3 &> /dev/null; then
    echo "  ❌ Python 3 no encontrado."
    echo "  Descargalo de: https://www.python.org/downloads/"
    echo ""
    read -p "  Pulsa Enter para abrir la web de descarga..."
    open "https://www.python.org/downloads/"
    exit 1
fi

PYTHON_VERSION=$(python3 --version 2>&1)
echo "  ✅ $PYTHON_VERSION encontrado"

# Install pyserial
echo ""
echo "  Instalando dependencias..."
python3 -m pip install --user pyserial Pillow pyobjc-framework-Quartz --break-system-packages 2>/dev/null || \
python3 -m pip install --user pyserial Pillow pyobjc-framework-Quartz 2>/dev/null || \
pip3 install pyserial Pillow pyobjc-framework-Quartz 2>/dev/null

if python3 -c "import serial" 2>/dev/null; then
    echo "  ✅ pyserial instalado correctamente"
else
    echo "  ❌ Error instalando pyserial"
    echo "  Intenta manualmente: pip3 install pyserial"
    read -p "  Pulsa Enter para salir..."
    exit 1
fi

if python3 -c "from PIL import Image" 2>/dev/null; then
    echo "  ✅ Pillow instalado (iconos activados)"
else
    echo "  ⚠️  Pillow no se pudo instalar (los iconos no funcionaran)"
    echo "  Intenta: pip3 install Pillow"
fi

# Install CH340 driver reminder
echo ""
echo "  ╔═══════════════════════════════════════════════╗"
echo "  ║  IMPORTANTE: Driver USB                       ║"
echo "  ║                                               ║"
echo "  ║  Si el Stream Deck no se detecta, necesitas   ║"
echo "  ║  instalar el driver CH340:                    ║"
echo "  ║  https://www.wch.cn/downloads/CH341SER_MAC_ZIP.html  ║"
echo "  ╚═══════════════════════════════════════════════╝"
echo ""
echo "  ✅ Dependencias instaladas!"
echo ""
echo "  ╔═══════════════════════════════════════════════╗"
echo "  ║  ¿Quieres que se abra automaticamente         ║"
echo "  ║  al conectar el Stream Deck por USB?          ║"
echo "  ╚═══════════════════════════════════════════════╝"
echo ""
read -p "  Activar auto-arranque? (s/n): " AUTOSTART

if [ "$AUTOSTART" = "s" ] || [ "$AUTOSTART" = "S" ] || [ "$AUTOSTART" = "si" ]; then
    cd "$(dirname "$0")"
    python3 streamdeck_app.py --install
    echo ""
fi

echo "  ✅ Instalacion completada!"
echo ""
echo "  Si activaste el auto-arranque, se abrira solo"
echo "  al enchufar el Stream Deck por USB."
echo ""
echo "  Si no, haz doble-click en 'Stream Deck' para usarlo."
echo ""
read -p "  Pulsa Enter para salir..."
