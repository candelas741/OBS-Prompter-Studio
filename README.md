# OBS Prompter Studio

OBS Prompter Studio is a native OBS Studio plugin for Windows that adds a professional teleprompter workflow directly inside OBS.

It provides:

- `Prompter Source`: an OBS source that renders scrolling text inside scenes.
- `Prompter Studio Control`: a dock for editing and controlling the prompter.
- `Prompter Studio Vista Privada`: a private operator preview dock that is not captured in recordings or streams.

The plugin is independent from PowerPoint and does not require Microsoft Office.

## Features

- Native OBS source: move, resize, crop, hide, lock and transform it like any other source.
- Transparent or colored background.
- Smooth vertical scrolling.
- Private dock preview for the operator.
- Text editor with `.txt` load/save.
- Font size, text color, background color and opacity controls.
- Horizontal and vertical mirror modes.
- Manual controls: start, pause, reset, forward and backward.
- OBS frontend hotkeys.
- Automatic configuration persistence across OBS restarts.
- Portable ZIP packaging and Inno Setup installer scripts.

## OBS Sources And Docks

After installation, OBS should show:

- `Sources > + > Prompter Source`
- `View > Docks > Prompter Studio Control`
- `View > Docks > Prompter Studio Vista Privada`

The docks are private OBS UI panels. They do not appear in recordings or livestreams. Only `Prompter Source`, when added to a scene, appears in video output.

## Hotkeys

The plugin registers these OBS hotkeys:

- Prompter Studio: iniciar
- Prompter Studio: pausar/reanudar
- Prompter Studio: pausar
- Prompter Studio: reanudar
- Prompter Studio: reiniciar
- Prompter Studio: aumentar velocidad
- Prompter Studio: reducir velocidad
- Prompter Studio: avanzar
- Prompter Studio: retroceder
- Prompter Studio: volver al inicio
- Prompter Studio: ir al final

Configure them in:

```text
File > Settings > Hotkeys
```

Suggested assignments:

- Start: `Ctrl + Alt + P`
- Pause/Resume: `Space` or `Ctrl + Alt + Space`
- Reset: `Ctrl + Alt + R`
- Increase speed: `Ctrl + Alt + Up`
- Decrease speed: `Ctrl + Alt + Down`
- Forward: `Ctrl + Alt + Right`
- Backward: `Ctrl + Alt + Left`
- Go to start: `Ctrl + Alt + Home`
- Go to end: `Ctrl + Alt + End`

## Configuration Persistence

The dock state is saved automatically to the OBS module configuration directory as:

```text
prompter_config.json
```

The saved state includes text, last TXT file path, selected source, colors, background opacity, font size, scroll position, speed and mirror settings.

`Prompter Source` settings are also saved in OBS scene/source settings.

## Requirements

- Windows 10/11
- OBS Studio 64-bit
- Visual Studio 2022 with Desktop development with C++
- CMake 3.28 or newer
- Qt and OBS plugin template dependencies

## Build

From the project root:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

The plugin DLL is generated at:

```text
build_x64\RelWithDebInfo\obs-prompter-studio.dll
```

You can also use:

```bat
build_release.bat
```

## Manual Installation

Close OBS Studio first.

Copy:

```text
build_x64\RelWithDebInfo\obs-prompter-studio.dll
```

to:

```text
C:\Program Files\obs-studio\obs-plugins\64bit\obs-prompter-studio.dll
```

Copy the plugin data directory to:

```text
C:\Program Files\obs-studio\data\obs-plugins\obs-prompter-studio\
```

Then reopen OBS Studio.

## Portable ZIP

Generate a portable package with:

```bat
package_zip.bat
```

Output:

```text
dist\OBS-Prompter-Studio.zip
```

The ZIP contains an OBS-compatible structure:

```text
OBS-Prompter-Studio/
  obs-plugins/
    64bit/
      obs-prompter-studio.dll
  data/
    obs-plugins/
      obs-prompter-studio/
        locale/
        README.txt
  README_INSTALACION.txt
```

## Installer

Build the Inno Setup installer with:

```bat
build_installer.bat
```

Output:

```text
dist\OBS-Prompter-Studio-Setup.exe
```

The installer is intended to install directly into the OBS Studio installation folder.

## Project Structure

```text
src/
  plugin-main.cpp
  prompter-source.cpp
  prompter-source.hpp
  prompter-dock.cpp
  prompter-dock.hpp
  prompter-state.cpp
  prompter-state.hpp
  prompter-hotkeys.cpp
  prompter-hotkeys.hpp

data/
  locale/

installer/
  obs-prompter-studio.iss
```

## License

This project is licensed under GPL-2.0-or-later. See [LICENSE](LICENSE).
