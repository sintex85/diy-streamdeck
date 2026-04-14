@echo off
cd /d "%~dp0"
cls
echo.
echo   ===================================
echo        DIY Stream Deck
echo   ===================================
echo.

:: Check Python is real (not Windows Store alias)
python -c "import sys; sys.exit(0)" >nul 2>&1
if %errorlevel% neq 0 (
    echo   [X] Python no encontrado.
    echo   Haz doble-click en "Instalar.bat" primero.
    echo.
    pause
    exit /b 1
)

:: Check pyserial
python -c "import serial" >nul 2>&1
if %errorlevel% neq 0 (
    echo   [X] Falta pyserial.
    echo   Haz doble-click en "Instalar.bat" primero.
    echo.
    pause
    exit /b 1
)

echo   Conecta el Stream Deck por USB y espera...
echo   Se abrira el navegador automaticamente.
echo.
echo   Para cerrar: cierra esta ventana.
echo.

python streamdeck_app.py
if %errorlevel% neq 0 (
    echo.
    echo   -- Error. Comprueba que:
    echo   1. Python esta instalado (con "Add to PATH")
    echo   2. Ejecutaste "Instalar.bat" primero
    echo   3. El ESP32 esta conectado por USB
    echo.
)
pause
