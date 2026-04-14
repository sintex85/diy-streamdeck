@echo off
cd /d "%~dp0"
cls
echo.
echo   ===================================
echo        DIY Stream Deck
echo   ===================================
echo.

:: Check files exist
if not exist "%~dp0streamdeck_app.py" (
    echo   [X] No se encuentra streamdeck_app.py
    echo.
    echo   Si descargaste un ZIP:
    echo   1. Click derecho en el .zip
    echo   2. "Extraer todo..."
    echo   3. Entra en la carpeta extraida
    echo   4. Ejecuta este .bat desde ahi
    echo.
    echo   Carpeta actual: %~dp0
    echo.
    pause
    exit /b 1
)

:: Check Python
python -c "import sys" >nul 2>&1
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

echo   Iniciando Stream Deck...
echo.
python streamdeck_app.py
echo.
echo   La app se ha cerrado.
echo.
pause
