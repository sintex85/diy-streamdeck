#!/bin/bash
cd "$(dirname "$0")/.."

clear
echo ""
echo "  ==================================="
echo "       DIY Stream Deck"
echo "  ==================================="
echo ""

if [ ! -f "streamdeck_app.py" ]; then
    echo "  [ERROR] No se encuentra streamdeck_app.py"
    echo "  Asegurate de haber extraido el ZIP completo."
    echo ""
    read -p "  Pulsa Enter para salir..."
    exit 1
fi

if ! python3 -c "import serial" 2>/dev/null; then
    echo "  [ERROR] Falta instalar dependencias."
    echo "  Ejecuta 'Instalar' primero."
    echo ""
    read -p "  Pulsa Enter para salir..."
    exit 1
fi

echo "  Stream Deck iniciado."
echo "  Se abrira el navegador cuando detecte el ESP32."
echo "  Ctrl+C o cierra la ventana para salir."
echo ""
python3 streamdeck_app.py
