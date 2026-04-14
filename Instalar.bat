@echo off
cd /d "%~dp0"
cls
echo.
echo   ===================================
echo     DIY Stream Deck - Instalador
echo   ===================================
echo.

:: Check Python (avoid Windows Store alias)
where python >nul 2>&1
if %errorlevel% neq 0 goto nopython

python --version >nul 2>&1
if %errorlevel% neq 0 goto nopython

:: Verify it's real Python, not the Store alias
python -c "import sys; sys.exit(0)" >nul 2>&1
if %errorlevel% neq 0 goto nopython

for /f "tokens=*" %%i in ('python --version 2^>^&1') do echo   [OK] %%i encontrado
goto haspython

:nopython
echo   [X] Python no encontrado.
echo.
echo   IMPORTANTE:
echo   1. Ve a https://www.python.org/downloads/
echo   2. Descarga Python 3
echo   3. Al instalar, MARCA la casilla "Add Python to PATH"
echo   4. Despues vuelve a ejecutar este instalador
echo.
echo   Abriendo la web de descarga...
start https://www.python.org/downloads/
pause
exit /b 1

:haspython
echo.
echo   Instalando dependencias...
python -m pip install --user pyserial Pillow
if %errorlevel% neq 0 (
    echo   [X] Error instalando dependencias
    echo   Intenta: python -m pip install pyserial Pillow
    pause
    exit /b 1
)

python -c "import serial" >nul 2>&1
if %errorlevel% equ 0 (
    echo   [OK] pyserial instalado
) else (
    echo   [X] Error con pyserial
    pause
    exit /b 1
)

python -c "from PIL import Image" >nul 2>&1
if %errorlevel% equ 0 (
    echo   [OK] Pillow instalado (iconos activados)
) else (
    echo   [!] Pillow no instalado (sin iconos)
)

echo.
echo   -------------------------------------------
echo   IMPORTANTE: Driver USB
echo.
echo   Si el Stream Deck no se detecta, instala
echo   el driver CH340 de:
echo   https://www.wch.cn/downloads/CH341SER_EXE.html
echo   -------------------------------------------
echo.
echo   [OK] Dependencias instaladas!
echo.
echo   Quieres que se abra automaticamente
echo   al conectar el Stream Deck por USB?
echo.
set /p AUTOSTART="  Activar auto-arranque? (s/n): "

if /i "%AUTOSTART%"=="s" (
    python streamdeck_app.py --install
    echo.
)

echo   [OK] Instalacion completada!
echo.
echo   Ahora haz doble-click en "Stream Deck.bat"
echo.
pause
