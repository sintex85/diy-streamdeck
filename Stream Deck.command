#!/bin/bash
# ═══════════════════════════════════════════
#   DIY Stream Deck - Launcher
# ═══════════════════════════════════════════

cd "$(dirname "$0")"

clear
echo ""
echo "  ╔═══════════════════════════════════╗"
echo "  ║      DIY Stream Deck              ║"
echo "  ╚═══════════════════════════════════╝"
echo ""

# Check dependencies
if ! python3 -c "import serial" 2>/dev/null; then
    echo "  ❌ Falta instalar dependencias."
    echo "  Haz doble-click en 'Instalar' primero."
    echo ""
    read -p "  Pulsa Enter para salir..."
    exit 1
fi

echo "  Conecta el Stream Deck por USB y espera..."
echo "  Se abrira el navegador automaticamente."
echo ""
echo "  Para cerrar: pulsa Ctrl+C o cierra esta ventana."
echo ""

python3 streamdeck_app.py
