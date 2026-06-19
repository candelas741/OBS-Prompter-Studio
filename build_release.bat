@echo off
setlocal

set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build_x64"
set "CONFIG=RelWithDebInfo"
set "DLL_PATH=%BUILD_DIR%\%CONFIG%\obs-prompter-studio.dll"

cd /d "%ROOT%"

echo === Prompter Studio: configurar Release ===
cmake --preset windows-x64
if errorlevel 1 exit /b 1

echo === Prompter Studio: compilar Release ===
cmake --build --preset windows-x64
if errorlevel 1 exit /b 1

if not exist "%DLL_PATH%" (
    echo ERROR: No se encontro la DLL esperada:
    echo "%DLL_PATH%"
    exit /b 1
)

echo.
echo DLL generada correctamente:
echo "%DLL_PATH%"

endlocal
