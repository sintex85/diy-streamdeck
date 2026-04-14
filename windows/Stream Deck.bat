@echo off
title DIY Stream Deck
color 0F
cd /d "%~dp0"
cd ..
call :main
goto :end

:main
cls
echo.
echo   ===================================
echo        DIY Stream Deck
echo   ===================================
echo.

if not exist "streamdeck_app.py" (
    echo   [ERROR] No se encuentra streamdeck_app.py
    echo.
    echo   Asegurate de haber extraido el ZIP completo.
    echo.
    goto :eof
)

python -c "import serial" >nul 2>&1
if %errorlevel% neq 0 (
    echo   [ERROR] Falta instalar dependencias.
    echo   Ejecuta "Instalar.bat" primero.
    echo.
    goto :eof
)

echo   Stream Deck iniciado.
echo   Se abrira el navegador cuando detecte el ESP32.
echo   Cierra esta ventana para salir.
echo.
python streamdeck_app.py
echo.
echo   Stream Deck cerrado.
goto :eof

:end
echo.
echo   Pulsa una tecla para cerrar...
pause >nul
