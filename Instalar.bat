@echo off
cd /d "%~dp0"
cls
echo.
echo   ===================================
echo     DIY Stream Deck - Instalador
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
    echo.
    echo   1. Ve a https://www.python.org/downloads/
    echo   2. Descarga Python 3
    echo   3. MARCA "Add Python to PATH" al instalar
    echo   4. Vuelve a ejecutar este instalador
    echo.
    start https://www.python.org/downloads/
    pause
    exit /b 1
)

for /f "tokens=*" %%i in ('python --version 2^>^&1') do echo   [OK] %%i

echo.
echo   Instalando dependencias...
python -m pip install pyserial Pillow

echo.
python -c "import serial" >nul 2>&1
if %errorlevel% equ 0 (
    echo   [OK] pyserial instalado
) else (
    echo   [X] Error instalando pyserial
    pause
    exit /b 1
)

python -c "from PIL import Image" >nul 2>&1
if %errorlevel% equ 0 (
    echo   [OK] Pillow instalado
) else (
    echo   [!] Pillow no instalado (sin iconos)
)

echo.
echo   -------------------------------------------
echo   NOTA: Si el Stream Deck no se detecta,
echo   instala el driver CH340:
echo   https://www.wch.cn/downloads/CH341SER_EXE.html
echo   -------------------------------------------
echo.

set /p AUTOSTART="  Activar auto-arranque? (s/n): "
if /i "%AUTOSTART%"=="s" (
    python streamdeck_app.py --install
)

echo.
echo   [OK] Listo! Ejecuta "Stream Deck.bat"
echo.
pause
