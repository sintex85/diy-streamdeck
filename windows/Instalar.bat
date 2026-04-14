@echo off
title DIY Stream Deck - Instalador
color 0F
cd /d "%~dp0"
cd ..
call :main
goto :end

:main
cls
echo.
echo   ===================================
echo     DIY Stream Deck - Instalador
echo   ===================================
echo.

if not exist "streamdeck_app.py" (
    echo   [ERROR] No se encuentra streamdeck_app.py
    echo.
    echo   Asegurate de haber extraido el ZIP completo.
    echo   Click derecho en el .zip, "Extraer todo..."
    echo.
    goto :eof
)

echo   Buscando Python...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo   [ERROR] Python no esta instalado.
    echo.
    echo   1. Ve a python.org/downloads
    echo   2. Descarga e instala Python 3
    echo   3. MUY IMPORTANTE: marca "Add Python to PATH"
    echo   4. Reinicia y ejecuta este instalador de nuevo
    echo.
    echo   Abriendo python.org...
    start https://www.python.org/downloads/
    goto :eof
)

for /f "tokens=*" %%i in ('python --version 2^>^&1') do echo   [OK] %%i

echo.
echo   Instalando pyserial y Pillow...
echo.
python -m pip install pyserial Pillow
echo.

python -c "import serial" >nul 2>&1
if %errorlevel% neq 0 (
    echo   [ERROR] pyserial no se instalo bien.
    echo   Intenta manualmente: python -m pip install pyserial
    goto :eof
)
echo   [OK] pyserial listo

python -c "from PIL import Image" >nul 2>&1
if %errorlevel% equ 0 (
    echo   [OK] Pillow listo
) else (
    echo   [!] Pillow no se instalo (los iconos no funcionaran)
)

echo.
echo   -------------------------------------------
echo   NOTA: Si el ESP32 no se detecta, instala
echo   el driver CH340:
echo   wch.cn/downloads/CH341SER_EXE.html
echo   -------------------------------------------
echo.
echo   [OK] Instalacion completada!
echo.
set /p AUTOSTART="  Auto-arranque al conectar USB? (s/n): "
if /i "%AUTOSTART%"=="s" (
    echo.
    python streamdeck_app.py --install
)
echo.
echo   Ahora ejecuta "Stream Deck.bat" en la carpeta windows
echo.
goto :eof

:end
echo.
echo   Pulsa una tecla para cerrar...
pause >nul
