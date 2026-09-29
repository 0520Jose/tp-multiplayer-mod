@echo off
setlocal enabledelayedexpansion

echo =========================================================
echo  Twilight Princess Multiplayer Mod - Build ^& Deploy
echo =========================================================

REM 1. Check if cl.exe is already available in PATH
where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [OK] MSVC compiler found in PATH.
    goto :BUILD
)

REM 2. Look for vcvars64.bat in standard installation paths
set "VS_PATH="
if exist "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)

if not "%VS_PATH%"=="" (
    echo [INFO] Initializing Visual Studio environment from:
    echo        "%VS_PATH%"
    call "%VS_PATH%"
) else (
    echo [ERROR] No MSVC x64 compiler found in PATH or standard VS install paths.
    echo Please run this script from the x64 Native Tools Command Prompt.
    pause
    exit /b 1
)

:BUILD
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

REM 3. Configure CMake if build-msvc directory or build.ninja doesn't exist
if not exist "build-msvc\build.ninja" (
    echo [INFO] Configuring CMake with Ninja...
    cmake -G Ninja -B build-msvc -DCMAKE_BUILD_TYPE=RelWithDebInfo
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] CMake configuration failed.
        pause
        exit /b 1
    )
)

REM 4. Build targets
echo.
echo [INFO] Compiling mod and test targets...
cmake --build build-msvc
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Compilation failed.
    pause
    exit /b 1
)

REM 5. Run tests
echo.
echo [INFO] Running unit tests...
build-msvc\test_main.exe

REM 6. Deploy to Dusklight mods directory
echo.
echo =========================================================
echo  Deploying Artifacts
echo =========================================================

set "DEPLOYED=0"
if exist "build-msvc\mods\tp_multiplayer_mod.dusk" (
    if exist "..\dusklight\mods" (
        copy /Y "build-msvc\mods\tp_multiplayer_mod.dusk" "..\dusklight\mods\tp_multiplayer_mod.dusk"
        echo [SUCCESS] Copied to ..\dusklight\mods\tp_multiplayer_mod.dusk
        set "DEPLOYED=1"
    ) else if exist "..\dusklight" (
        mkdir "..\dusklight\mods"
        copy /Y "build-msvc\mods\tp_multiplayer_mod.dusk" "..\dusklight\mods\tp_multiplayer_mod.dusk"
        echo [SUCCESS] Created ..\dusklight\mods and copied mod package.
        set "DEPLOYED=1"
    )
    if exist "dusklight\mods" (
        copy /Y "build-msvc\mods\tp_multiplayer_mod.dusk" "dusklight\mods\tp_multiplayer_mod.dusk"
        echo [SUCCESS] Copied to dusklight\mods\tp_multiplayer_mod.dusk
        set "DEPLOYED=1"
    )
    if exist "%APPDATA%\TwilitRealm\Dusklight\mods\.cache" (
        rmdir /S /Q "%APPDATA%\TwilitRealm\Dusklight\mods\.cache" >nul 2>&1
        echo [INFO] Cleared Dusklight mod unpack cache.
    )
)

if "!DEPLOYED!"=="0" (
    echo [NOTICE] Packaged mod created at: build-msvc\mods\tp_multiplayer_mod.dusk
    echo Copy it manually to your Dusklight mods\ directory.
)

echo.
echo =========================================================
echo  Build complete! Run server_simulator.exe to start relay.
echo =========================================================
if "%1"=="" pause
