#!/bin/bash
# Desinstala el agente del Stream Deck.
LABEL="com.bitsytornillos.streamdeck"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
launchctl unload "$PLIST" 2>/dev/null || true
rm -f "$PLIST"
rm -rf "$HOME/Library/Application Support/StreamDeck"
echo "Agente del Stream Deck desinstalado."
