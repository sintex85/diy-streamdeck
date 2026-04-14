@echo off
cd /d "%~dp0"
cls
echo.
echo   ===================================
echo        DIY Stream Deck
echo   ===================================
echo.

:: Check if running from inside a ZIP (temp folder)
echo "%~dp0" | findstr /i "Temp" >nul 2>&1
if %errorlevel% equ 0 (
    if not exist "%~dp0streamdeck_app.py" (
        echo   [X] Estas ejecutando desde dentro del ZIP!
        echo.
        echo   1. Haz click derecho en el archivo .zip
        echo   2. Selecciona "Extraer todo..."
        echo   3. Abre la carpeta extraida
        echo   4. Ejecuta "Stream Deck.bat" desde ahi
        echo.
        pause
        exit /b 1
    )
)

:: Check streamdeck_app.py exists next to this bat
if not exist "%~dp0streamdeck_app.py" (
    echo   [X] No se encuentra streamdeck_app.py
    echo   Asegurate de que todos los archivos estan
    echo   en la misma carpeta.
    echo.
    pause
    exit /b 1
)

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
