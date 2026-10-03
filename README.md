# 3radials

A radial equipment and item menu SKSE plugin for **The Elder Scrolls V: Skyrim Special Edition / Anniversary Edition / VR Edition**, built with [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG).

## Requirements

- Skyrim Special Edition / Anniversary Edition / VR Edition
- Address Library for SKSE Plugins

## Installation

Automatic install with Vortex or ModOrganizer2 or manually:

Copy the binary file `3radials.dll` and the folder `3radials` to
		`SKYRIM_FOLDER\Data\SKSE\Plugins\`
		
	The main configuration .ini file is located in \3radials
	
	Translations should be located in \3radials\languages
	Icons can be modified in folder \3radials\icons
	Radial formats are saved in \3radials\radials
	Layouts with positions and colors are saved in \3radials\layouts

## Controls

All bindings can be changed from **WheelSettings → Settings**. `First Key`,
`Second Key`, and `Alt Config Key` have no fixed keyboard default in this
table because they are user-configurable.

| Action | Keyboard / Mouse | Xbox / XInput Controller |
| --- | --- | --- |
| Open a radial | Hold **First Key** or **Second Key**, then choose a direction | Hold the configured **First/Second Key**, then use the D-pad or a stick direction |
| Open Top / Bottom / Left / Right directly | **Arrow keys** or **Numpad 8 / 2 / 4 / 6** while the activation key is held; they can also open directly when **Open Menu with arrows** is enabled | **D-pad** direction while the activation key is held |
| Open WheelSettings | Hold an activation key and use the configuration gesture, or press **Alt Config Key** | Press the configured **Alt Config Key**, or use the configured settings shortcut |
| Select and use an item | Move the cursor over the item, then click the applicable mouse button | Move selection with the stick / D-pad, then use **A** or **X** |
| Cycle items in a radial | **Mouse wheel** | **D-pad**, **L1**, or **R1** depending on the active radial |
| Move an item in WheelSettings | Drag with **Left Mouse Button** | Select with **A/X**, then navigate with the D-pad |
| Remove an item in WheelSettings | **Right Mouse Button** | **B** |
| Duplicate selected editor piece | **Ctrl + C** | — |
| Undo / redo editor change | **Ctrl + Z** / **Ctrl + Y** | — |
| Close WheelSettings | **Esc** or the on-screen close button | **Y** or the on-screen close button |

The radial lock, scroll lock, and camera-movement lock are controlled by their
on-screen buttons and are saved with the current game.
	
## Building

This project uses **CommonLibSSE-NG** and **Dear ImGui** as Git submodules.

Clone the repository including its submodules:

```bash
git clone --recursive https://github.com/Ipsutnu/3radials.git
```

If you already cloned the repository without `--recursive`, initialize the submodules with:

```bash
git submodule update --init --recursive
```

### Dependencies

The project currently uses:

- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [NanoSVG](https://github.com/memononen/nanosvg)

Additional dependencies may be managed through `vcpkg`.

## Contributing

Contributions are welcome.

If you would like to fix a bug, improve compatibility, optimize existing code, or add a feature, please fork the repository and submit a Pull Request.

Pull Requests are reviewed before being merged into the official repository.

Submitting a Pull Request does not guarantee that the contribution will be accepted. Changes may be discussed, modified, or rejected depending on the direction and requirements of the project.

Please keep contributions focused and provide a short explanation of:

- What was changed
- Why the change is useful or necessary
- How the change was tested

Contributions must be compatible with the licensing requirements of this project.

## Source Code

The source code available in this repository corresponds to the open-source SKSE plugin project.

Official releases are distributed through the project's official release channels:
	
	https://www.nexusmods.com/skyrimspecialedition/mods/193173

Forks and modified versions are not official releases of this project unless explicitly stated otherwise.

## License

Copyright © 2026 [Preguissoso]

This project is free software licensed under the **GNU General Public License, version 3 or any later version (GPL-3.0-or-later)**.

You are free to use, study, modify, and redistribute this software under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

See the [LICENSE] file for the complete license terms.

### Third-Party Software

This project uses third-party libraries that remain subject to their respective licenses.

In particular:

- **CommonLibSSE-NG** is licensed under GPL-3.0-or-later with its applicable Modding and Linking Exceptions.
- **Dear ImGui** is licensed under the MIT License.
- **NanoSVG** by Mikko Mononen is used to rasterize SVG icons in memory and is
  licensed under its permissive zlib-style license.
- **SkyUI Icons** — Some icons used by this project were originally created
  by **Psychosteve** for SkyUI. All credit for these original assets belongs
  to Psychosteve and the respective SkyUI contributors. These assets remain
  subject to their original permissions and are not covered by this
  project's GPL-3.0-or-later license.

The inclusion or use of third-party software does not transfer ownership of those projects to this project.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[licenses/NanoSVG.txt](licenses/NanoSVG.txt) for the applicable notices.

## Credits

- **CommonLibSSE-NG contributors** — Skyrim runtime bindings and plugin support.
- **Omar Cornut and Dear ImGui contributors** — user-interface framework.
- **Mikko Mononen** — NanoSVG and NanoSVGRast, used for SVG icon parsing and rasterization.
- **Psychosteve and the SkyUI contributors** — original SkyUI icon assets used by this project.

## Disclaimer

This project is an independent community-made modification and is not affiliated with, endorsed by, or associated with Bethesda Softworks, Bethesda Game Studios, or their parent companies.

The Elder Scrolls, Skyrim, and related names and trademarks belong to their respective owners.
