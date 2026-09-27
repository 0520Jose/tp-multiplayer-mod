@echo off
echo ==============================================
echo Compilando el Mod Multijugador para Twilight Princess...
echo ==============================================

set SCRIPT_DIR=%~dp0
if not exist "%SCRIPT_DIR%build" (
    mkdir "%SCRIPT_DIR%build"
)

cd /d "%SCRIPT_DIR%build"
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
cmake --build . --config Release

echo.
echo ==============================================
echo Copiando a la carpeta de pruebas de Dusklight...
echo ==============================================
if exist "%SCRIPT_DIR%build\mods\tp_multiplayer_mod.dusk" (
    copy /Y "%SCRIPT_DIR%build\mods\tp_multiplayer_mod.dusk" "%SCRIPT_DIR%dusklight\mods\tp_multiplayer_mod.dusk"
) else (
    echo [AVISO] No se encontro build\mods\tp_multiplayer_mod.dusk para copiar.
)

echo.
echo ¡Todo listo! Ya puedes abrir server_simulator.exe y luego el juego.
pause
