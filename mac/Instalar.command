#!/bin/bash
cd "$(dirname "$0")/.."

clear
echo ""
echo "  ==================================="
echo "    DIY Stream Deck - Instalador"
echo "  ==================================="
echo ""

if [ ! -f "streamdeck_app.py" ]; then
    echo "  [ERROR] No se encuentra streamdeck_app.py"
    echo "  Asegurate de haber extraido el ZIP completo."
    echo ""
    read -p "  Pulsa Enter para salir..."
    exit 1
fi

# Check Python 3
if ! command -v python3 &> /dev/null; then
    echo "  [ERROR] Python 3 no encontrado."
    echo "  Descargalo de: https://www.python.org/downloads/"
    echo ""
    read -p "  Pulsa Enter para abrir la web..."
    open "https://www.python.org/downloads/"
    exit 1
fi

echo "  [OK] $(python3 --version)"
echo ""
echo "  Instalando dependencias..."
python3 -m pip install --user pyserial Pillow pyobjc-framework-Quartz --break-system-packages 2>/dev/null || \
python3 -m pip install --user pyserial Pillow pyobjc-framework-Quartz 2>/dev/null || \
pip3 install pyserial Pillow pyobjc-framework-Quartz 2>/dev/null

echo ""
if python3 -c "import serial" 2>/dev/null; then
    echo "  [OK] pyserial instalado"
else
    echo "  [ERROR] pyserial no se instalo"
    echo "  Intenta: pip3 install pyserial"
    read -p "  Pulsa Enter para salir..."
    exit 1
fi

if python3 -c "from PIL import Image" 2>/dev/null; then
    echo "  [OK] Pillow instalado"
else
    echo "  [!] Pillow no se instalo (sin iconos)"
fi

if python3 -c "import Quartz" 2>/dev/null; then
    echo "  [OK] Quartz instalado (atajos de teclado)"
else
    echo "  [!] Quartz no se instalo (sin atajos de teclado)"
fi

echo ""
echo "  -------------------------------------------"
echo "  NOTA: Si el ESP32 no se detecta, instala"
echo "  el driver CH340:"
echo "  wch.cn/downloads/CH341SER_MAC_ZIP.html"
echo "  -------------------------------------------"
echo ""
echo "  [OK] Instalacion completada!"
echo ""
read -p "  Activar auto-arranque al conectar USB? (s/n): " AUTOSTART
if [ "$AUTOSTART" = "s" ] || [ "$AUTOSTART" = "S" ]; then
    python3 streamdeck_app.py --install
    echo ""
fi

echo ""
echo "  Ahora ejecuta 'Stream Deck' en la carpeta mac"
echo ""
read -p "  Pulsa Enter para salir..."
