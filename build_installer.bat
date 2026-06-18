@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "DIST_DIR=%ROOT%dist"
set "SETUP_PATH=%DIST_DIR%\OBS-Prompter-Studio-Setup.exe"

cd /d "%ROOT%"

if not exist "%DIST_DIR%\OBS-Prompter-Studio\obs-plugins\64bit\obs-prompter-studio.dll" (
    echo Paquete ZIP/dist no encontrado. Generando primero...
    call "%ROOT%package_zip.bat"
    if errorlevel 1 exit /b 1
)

where ISCC.exe >nul 2>nul
if errorlevel 1 (
    if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" (
        set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
    ) else if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" (
        set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
    ) else (
        echo ERROR: Inno Setup 6 no encontrado.
        echo Instale Inno Setup 6 o agregue ISCC.exe al PATH.
        exit /b 1
    )
) else (
    set "ISCC=ISCC.exe"
)

echo === Compilando instalador con Inno Setup ===
"!ISCC!" "%ROOT%installer\obs-prompter-studio.iss"
if errorlevel 1 exit /b 1

if not exist "%SETUP_PATH%" (
    echo ERROR: No se genero el instalador esperado:
    echo "%SETUP_PATH%"
    exit /b 1
)

echo.
echo Instalador generado:
echo "%SETUP_PATH%"

endlocal
