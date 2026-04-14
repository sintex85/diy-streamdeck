@echo off
chcp 65001 >nul
cd /d "%~dp0"
cls
echo.
echo   ╔═══════════════════════════════════╗
echo   ║      DIY Stream Deck              ║
echo   ╚═══════════════════════════════════╝
echo.

:: Check dependencies
python -c "import serial" 2>nul
if %errorlevel% neq 0 (
    python3 -c "import serial" 2>nul
    if %errorlevel% neq 0 (
        echo   X Falta instalar dependencias.
        echo   Haz doble-click en "Instalar.bat" primero.
        echo.
        pause
        exit /b 1
    )
)

echo   Conecta el Stream Deck por USB y espera...
echo   Se abrira el navegador automaticamente.
echo.
echo   Para cerrar: cierra esta ventana.
echo.

echo   Iniciando...
echo.
python streamdeck_app.py
if %errorlevel% neq 0 (
    echo.
    echo   ── Error. Comprueba que:
    echo   1. Python esta instalado y en el PATH
    echo   2. Ejecutaste 'Instalar.bat' primero
    echo   3. El ESP32 esta conectado por USB
    echo.
)
pause
