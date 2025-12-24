# OBS Source Resizer Dock

A dockable panel plugin for OBS Studio that provides quick access to source transform controls.

![OBS Studio](https://img.shields.io/badge/OBS%20Studio-30+-302E31?logo=obsstudio&logoColor=white)
![License](https://img.shields.io/badge/License-GPLv2-blue)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey)

## Features

- 📐 **Quick Transform Controls** - Resize and reposition sources directly from a dock panel
- 🎯 **Anchor Presets** - Unity-style anchor system for precise positioning
- 👁️ **Visibility Toggle** - Quickly show/hide selected sources
- ✏️ **Rename Sources** - Edit source names without opening properties
- 📦 **Group Support** - Works with sources nested inside groups
- ⌨️ **Modifier Keys** - Hold Shift for pivot, Alt for position presets

## Screenshot

*Select a source in your scene to edit its transform properties*

## Installation

### Windows
1. Download the latest release from [Releases](../../releases)
2. Extract `obs-source-resizer-dock.dll` to your OBS plugins folder:
   - `C:\Program Files\obs-studio\obs-plugins\64bit\`
3. Restart OBS Studio
4. Enable the dock via **View → Docks → Source Resizer**

### macOS
1. Download the latest release
2. Copy to `/Library/Application Support/obs-studio/plugins/`
3. Restart OBS Studio

### Linux
1. Download the latest release
2. Copy to `~/.config/obs-studio/plugins/`
3. Restart OBS Studio

## Usage

1. Open the dock via **View → Docks → Source Resizer**
2. Select a source in your scene
3. Use the controls to adjust:
   - **Position X/Y** - Set exact pixel coordinates
   - **Width/Height** - Resize the source
   - **Visibility checkbox** - Toggle source visibility
   - **Name field** - Rename the source

### Anchor Presets

Click the anchor button to open the preset grid:
- **Normal click** - No action (select preset first)
- **Shift + click** - Set source pivot/alignment
- **Alt + click** - Snap source to canvas position
- **Shift + Alt + click** - Set both pivot and position

## Building from Source

### Requirements
- CMake 3.28+
- OBS Studio 30+ source/libraries
- Qt 6
- C++17 compatible compiler

### Build Steps

```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/obs-source-resizer-dock.git
cd obs-source-resizer-dock

# Configure
cmake --preset windows-x64  # or macos, linux

# Build
cmake --build build_x64 --config Release
```

Or use the provided batch file on Windows:
```bash
setup_and_build.bat
```

## License

This project is licensed under the GNU General Public License v2.0 - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Acknowledgments

- Built using the [OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate)
- Inspired by Unity's RectTransform component
