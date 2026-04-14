@echo off
chcp 65001 >nul
cls
echo.
echo   ╔═══════════════════════════════════╗
echo   ║   DIY Stream Deck - Instalador    ║
echo   ╚═══════════════════════════════════╝
echo.

:: Check Python
python --version >nul 2>&1
if %errorlevel% neq 0 (
    python3 --version >nul 2>&1
    if %errorlevel% neq 0 (
        echo   X Python no encontrado.
        echo   Descargalo de: https://www.python.org/downloads/
        echo.
        echo   IMPORTANTE: Marca "Add Python to PATH" al instalar!
        echo.
        start https://www.python.org/downloads/
        pause
        exit /b 1
    )
)

for /f "tokens=*" %%i in ('python --version 2^>^&1') do echo   OK %%i encontrado

echo.
echo   Instalando dependencias...
python -m pip install pyserial Pillow 2>nul || python3 -m pip install pyserial Pillow 2>nul

python -c "import serial" 2>nul
if %errorlevel% equ 0 (
    echo   OK pyserial instalado
) else (
    echo   X Error instalando pyserial
    pause
    exit /b 1
)

python -c "from PIL import Image" 2>nul
if %errorlevel% equ 0 (
    echo   OK Pillow instalado (iconos activados^)
) else (
    echo   !! Pillow no se pudo instalar (sin iconos^)
)

echo.
echo   ╔═══════════════════════════════════════════════╗
echo   ║  IMPORTANTE: Driver USB                       ║
echo   ║                                               ║
echo   ║  Si el Stream Deck no se detecta, instala     ║
echo   ║  el driver CH340 de:                          ║
echo   ║  https://www.wch.cn/downloads/CH341SER_EXE.html  ║
echo   ╚═══════════════════════════════════════════════╝
echo.
echo   OK Dependencias instaladas!
echo.
echo   Quieres que se abra automaticamente
echo   al conectar el Stream Deck por USB?
echo.
set /p AUTOSTART="  Activar auto-arranque? (s/n): "

if /i "%AUTOSTART%"=="s" (
    cd /d "%~dp0"
    python streamdeck_app.py --install 2>nul || python3 streamdeck_app.py --install
    echo.
)

echo   OK Instalacion completada!
echo.
echo   Si activaste el auto-arranque, se abrira solo
echo   al enchufar el Stream Deck por USB.
echo.
echo   Si no, haz doble-click en "Stream Deck.bat" para usarlo.
echo.
pause
