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

python streamdeck_app.py 2>nul || python3 streamdeck_app.py
pause
