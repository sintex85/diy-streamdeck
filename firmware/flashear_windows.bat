@echo off
chcp 65001 >nul
cd /d "%~dp0"
cls
echo.
echo   ╔═══════════════════════════════════╗
echo   ║  Flashear Stream Deck ESP32       ║
echo   ╚═══════════════════════════════════╝
echo.

:: Find COM port
set "PORT="
for /f "tokens=1" %%a in ('python -c "import serial.tools.list_ports; [print(p.device) for p in serial.tools.list_ports.comports() if 'ch340' in (p.description or '').lower() or 'ch341' in (p.description or '').lower()]" 2^>nul') do set "PORT=%%a"

if "%PORT%"=="" (
    echo   X No se detecta ningun ESP32.
    echo   Conectalo por USB e intenta de nuevo.
    echo   Si no funciona, instala el driver CH340.
    echo.
    pause
    exit /b 1
)

echo   ESP32 detectado en: %PORT%
echo.
echo   Paso 1/2: Borrando flash...

python esptool.py --chip esp32s3 --port %PORT% erase_flash

if %errorlevel% neq 0 (
    echo   X Error borrando flash.
    pause
    exit /b 1
)

echo.
echo   Paso 2/2: Escribiendo firmware...

python esptool.py --chip esp32s3 --port %PORT% --baud 460800 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0000 button_counter.ino.bootloader.bin 0x8000 button_counter.ino.partitions.bin 0xe000 boot_app0.bin 0x10000 button_counter.ino.bin

if %errorlevel% equ 0 (
    echo.
    echo   OK Flasheado correctamente!
    echo   El Stream Deck esta listo para usar.
) else (
    echo.
    echo   X Error escribiendo firmware.
)

echo.
pause
