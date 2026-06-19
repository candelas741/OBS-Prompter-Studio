@echo off
setlocal

set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build_x64"
set "CONFIG=RelWithDebInfo"
set "DLL_SOURCE=%BUILD_DIR%\%CONFIG%\obs-prompter-studio.dll"
set "DIST_DIR=%ROOT%dist"
set "PACKAGE_ROOT=%DIST_DIR%\Prompter-Studio"
set "PLUGIN_BIN=%PACKAGE_ROOT%\obs-plugins\64bit"
set "PLUGIN_DATA=%PACKAGE_ROOT%\data\obs-plugins\obs-prompter-studio"
set "ZIP_PATH=%DIST_DIR%\Prompter-Studio.zip"

cd /d "%ROOT%"

if not exist "%DLL_SOURCE%" (
    echo DLL no encontrada. Compilando primero...
    call "%ROOT%build_release.bat"
    if errorlevel 1 exit /b 1
)

echo === Preparando carpeta dist ===
if exist "%PACKAGE_ROOT%" rmdir /s /q "%PACKAGE_ROOT%"
mkdir "%PLUGIN_BIN%"
mkdir "%PLUGIN_DATA%"

echo === Copiando DLL ===
copy /y "%DLL_SOURCE%" "%PLUGIN_BIN%\obs-prompter-studio.dll"
if errorlevel 1 exit /b 1

echo === Copiando data del plugin ===
xcopy /e /i /y "%ROOT%data" "%PLUGIN_DATA%"
if errorlevel 1 exit /b 1

copy /y "%ROOT%LICENSE" "%PACKAGE_ROOT%\LICENSE.txt" >nul
copy /y "%ROOT%NOTICE.md" "%PACKAGE_ROOT%\NOTICE.txt" >nul
copy /y "%ROOT%CHANGELOG.md" "%PACKAGE_ROOT%\CHANGELOG.txt" >nul

echo === Generando README del plugin ===
(
echo Prompter Studio
echo Version 1.1.0
echo.
echo Este plugin agrega:
echo - Fuente OBS: Prompter Source
echo - Dock: Prompter Studio Control
echo - Dock: Prompter Studio Vista Privada
echo.
echo Requisitos:
echo - Windows 10/11
echo - OBS Studio 64 bits
echo.
echo Desarrollado por Psicocartoon Studio.
echo Proyecto de terceros, no afiliado ni respaldado por OBS Studio.
echo Codigo fuente y soporte:
echo https://github.com/candelas741/Prompter-Studio
echo Licencia: GPL-2.0-or-later. Consulte LICENSE.txt.
) > "%PLUGIN_DATA%\README.txt"

echo === Generando README de instalacion ===
(
echo Prompter Studio - README DE INSTALACION
echo ========================================
echo.
echo Prompter Studio es un plugin de terceros para OBS Studio.
echo No esta afiliado ni respaldado por OBS Studio o el OBS Project.
echo Codigo fuente y soporte:
echo https://github.com/candelas741/Prompter-Studio
echo.
echo INSTALACION ZIP:
echo 1. Cerrar OBS.
echo 2. Copiar las carpetas obs-plugins y data dentro de:
echo    C:\Program Files\obs-studio\
echo 3. Abrir OBS.
echo 4. Verificar:
echo    Fuentes ^> + ^> Prompter Source
echo    Ver ^> Paneles/Docks ^> Prompter Studio Control
echo    Ver ^> Paneles/Docks ^> Prompter Studio Vista Privada
echo.
echo INSTALACION EXE:
echo 1. Cerrar OBS.
echo 2. Ejecutar Prompter-Studio-Setup.exe como administrador.
echo 3. Seleccionar la carpeta de OBS si se solicita.
echo 4. Abrir OBS.
echo.
echo DESINSTALACION:
echo Eliminar:
echo C:\Program Files\obs-studio\obs-plugins\64bit\obs-prompter-studio.dll
echo C:\Program Files\obs-studio\data\obs-plugins\obs-prompter-studio\
echo.
echo VERIFICACIONES POST-INSTALACION:
echo - Prompter Source debe aparecer en Fuentes.
echo - Prompter Studio Control debe aparecer en Ver ^> Paneles/Docks.
echo - Prompter Studio Vista Privada debe aparecer en Ver ^> Paneles/Docks.
) > "%PACKAGE_ROOT%\README_INSTALACION.txt"

copy /y "%PACKAGE_ROOT%\README_INSTALACION.txt" "%DIST_DIR%\README_INSTALACION.txt" >nul

echo === Creando ZIP portable ===
if exist "%ZIP_PATH%" del /q "%ZIP_PATH%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%PACKAGE_ROOT%' -DestinationPath '%ZIP_PATH%' -Force"
if errorlevel 1 exit /b 1

echo.
echo ZIP generado:
echo "%ZIP_PATH%"
echo Carpeta lista:
echo "%PACKAGE_ROOT%"

endlocal
