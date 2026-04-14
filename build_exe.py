#!/usr/bin/env python3
"""
Build standalone executable for Stream Deck app.
Run this on the TARGET platform (Windows for .exe, Mac for .app).

Requirements:
    pip install pyinstaller pyserial Pillow pyobjc-framework-Quartz

Usage:
    python build_exe.py
"""
import subprocess
import sys
import platform

def main():
    system = platform.system()
    print(f"Building for {system}...")

    # Base PyInstaller command
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--onedir",
        "--name", "StreamDeck",
        "--clean",
        "--noconfirm",
        # Hidden imports that PyInstaller misses
        "--hidden-import", "serial.tools.list_ports",
        "--hidden-import", "serial.tools.list_ports_common",
        "--hidden-import", "serial.tools.list_ports_posix",
        "--hidden-import", "serial.tools.list_ports_windows",
        "--hidden-import", "PIL",
    ]

    if system == "Windows":
        cmd += [
            "--noconsole",  # No terminal window
            "--icon", "NONE",
        ]
    elif system == "Darwin":
        cmd += [
            "--hidden-import", "Quartz",
            "--hidden-import", "objc",
        ]

    cmd.append("streamdeck_app.py")

    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd)

    if result.returncode == 0:
        print(f"\n  Build OK!")
        print(f"  Output in: dist/StreamDeck/")
        if system == "Windows":
            print(f"\n  Para auto-arranque, copia la carpeta StreamDeck")
            print(f"  y ejecuta: StreamDeck.exe --install")
        elif system == "Darwin":
            print(f"\n  Para auto-arranque ejecuta:")
            print(f"  ./dist/StreamDeck/StreamDeck --install")
    else:
        print(f"\n  Build FAILED (exit code {result.returncode})")

if __name__ == "__main__":
    main()
