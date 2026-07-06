#!/bin/bash
# Instala el agente del Stream Deck como servicio de inicio de sesion (launchd).
set -e
LABEL="com.bitsytornillos.streamdeck"
DEST="$HOME/Library/Application Support/StreamDeck"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
LOG="$HOME/Library/Logs/streamdeck-agent.log"
SRC="$(cd "$(dirname "$0")" && pwd)/streamdeck_agent.py"

PYBIN="$(command -v python3 || echo /usr/bin/python3)"
echo "Python: $PYBIN"
"$PYBIN" -c 'import serial' 2>/dev/null || { echo "Instalando pyserial..."; "$PYBIN" -m pip install --user pyserial; }

mkdir -p "$DEST" "$HOME/Library/LaunchAgents"
cp "$SRC" "$DEST/streamdeck_agent.py"

cat > "$PLIST" <<PL
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>$LABEL</string>
  <key>ProgramArguments</key>
  <array>
    <string>$PYBIN</string>
    <string>$DEST/streamdeck_agent.py</string>
  </array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>StandardOutPath</key><string>$LOG</string>
  <key>StandardErrorPath</key><string>$LOG</string>
</dict></plist>
PL

launchctl unload "$PLIST" 2>/dev/null || true
launchctl load "$PLIST"
echo ""
echo "Instalado. El agente arranca solo al iniciar sesion."
echo "Log en: $LOG"
echo "Para reconfigurar botones en Chrome, pausa el agente:  launchctl unload \"$PLIST\""
echo "y reanudalo despues con:  launchctl load \"$PLIST\""
