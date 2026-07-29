@echo off
echo ==============================================
echo Compilando el Mod Multijugador para Twilight Princess...
echo ==============================================

cd /d C:\Games\tp-multiplayer-mod\build
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
cmake --build . --config Release

echo.
echo ==============================================
echo Copiando a la carpeta de pruebas de Dusklight...
echo ==============================================
copy /Y C:\Games\tp-multiplayer-mod\build\mods\tp_multiplayer_mod.dusk C:\Games\tp-multiplayer-mod\build\dusklight\mods\tp_multiplayer_mod.dusk

echo.
echo ¡Todo listo! Ya puedes abrir server_simulator.exe y luego el juego.
pause
