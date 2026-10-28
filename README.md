# FrameColor

[日本語](README.ja.md)

## About this fork

This is a fork of [beko-kerbecotton/framecolor](https://github.com/beko-kerbecotton/framecolor). It adds:

- **Sharpness**: AMD FidelityFX CAS, run inside the existing color correction pass, so no extra full-screen pass is added. Standalone apps often render below the panel resolution; this makes the upscaled image look sharper.
- **Hold to Compare**: hold the button to see the image without correction.
- **Auto presets**: apply a preset by mode (standalone or PC streaming) or for the current SteamVR app.
- **`framecolorctl bench`**: measure frame timing and memory while FrameColor runs.
- **Korean interface** and **automatic language selection** that follows the Steam client language.
- A release build of the Vulkan layer. Upstream's `install.sh` and `package.sh` built it without optimization.

It also includes two changes from [brugr/framecolor](https://github.com/brugr/framecolor): English and Japanese translations, and a fix for a `vrcompositor` crash when FrameColor loads alongside other Vulkan layers.

This fork has no release archives. Build and install it on the Steam Frame as described under [Installation](#installation).

## Overview

FrameColor lets you adjust the Steam Frame's display colors from the SteamVR dashboard inside the headset. Alongside panel brightness, you can adjust brightness, saturation, contrast, and gamma globally or separately for the red, green, and blue channels, and sharpen the image. It includes Standard, Vivid, and Night presets, plus three slots for your own presets.

FrameColor is for Steam Frame only. Color correction is applied by a Vulkan layer loaded into SteamVR's `vrcompositor`. It affects locally run and streamed content, as well as the dashboard and other displayed elements.

## Requirements

- A Steam Frame (SteamOS, ARM64) using the stock SteamVR installation at `/opt/steamvr`
- Developer Mode and SSH access, so you can transfer the archive to the Steam Frame and run commands on it
- Python 3.10 or later on the Steam Frame, used to generate the LUT during installation
- The build tools listed under [Development](#development). SteamOS on the Steam Frame shipped with all of them on 2026-10-28.

## Installation

Run these commands on the Steam Frame, for example over SSH (`ssh steamos@FRAME_IP`). Clone this repository, then build and install:

```sh
git clone <this repository's URL> framecolor
cd framecolor
sh contrib/install.sh
~/.local/bin/framecolorctl enable
```

Run the installer as your regular user, without `sudo`. It builds the dashboard and the Vulkan layer from source and installs them in your `~/.local` and `~/.config` directories only. Vulkan correction is disabled immediately after a fresh installation. The final `enable` command enables both the correction and the dashboard, and restarts SteamVR.

If you already use upstream FrameColor, install over it the same way. Saved adjustments and presets are kept, and their older file formats are read. If correction is already enabled, the installer restarts SteamVR.

To update, run `git pull` and `sh contrib/install.sh` again.

To remove FrameColor completely, run this from the repository directory:

```sh
sh contrib/uninstall.sh
```

Uninstalling also deletes saved adjustments and presets. Panel brightness stored by SteamVR may remain. If necessary, use Reset in the dashboard to return to the standard setting before uninstalling.

## Settings

Open “Display Color Adjustment” in the SteamVR dashboard.
The interface is available in English, Japanese and Korean. By default it follows the Steam client language, then the system locale, and falls back to English. Choose a language, or Auto to follow Steam again, from the language drop-down menu in the top-right corner of the overlay panel, or run `framecolorctl language en|ja|ko|auto`.

| Control | Range | Applies to |
| --- | --- | --- |
| Panel brightness | 40–125% | SteamVR panel brightness setting; shared across channels |
| Brightness | 50–150% | Vulkan correction; global or per RGB channel |
| Saturation | 0–200% | Vulkan correction; global or per RGB channel; 100% is standard |
| Contrast | 0–100% | Vulkan correction; global or per RGB channel; 0% is standard |
| Gamma | 0.50–2.50 | Vulkan correction; global or per RGB channel; 1.00 is standard |
| Sharpness | 0–100% | Vulkan correction (AMD CAS); shared across channels; 0% is off |

Under Adjustment Channel, choose All, Red, Green, or Blue. Moving a control with All selected changes the current values of all three channels together. Panel brightness and sharpness are unaffected by the channel selection. To save the current settings, select Save and then P1, P2, or P3. Reset applies the Standard preset.

Hold **Hold to Compare** to see the image without FrameColor's correction. Release it to return to your settings.

**Auto Preset** applies a preset when the display mode or the app changes. Press a button to step through Off, Standard, Vivid, Night, and your saved P1–P3:

- **Standalone**: used while no PC stream runs.
- **PC Streaming**: used while `vrlink` runs (a PC stream is active).
- **This App**: used while the current SteamVR scene app runs. It wins over the mode rules. It is unavailable when the app has no SteamVR app key.

FrameColor checks the mode and the app once per second and switches after it sees the same change twice. A new rule applies at the next switch. Your manual changes stay until the mode or the app changes again. When you leave all rules (for example, you quit an app and the mode rule is Off), FrameColor restores the settings from before the first auto preset. Rules are saved in `~/.config/framecolor/auto.txt`.

You can also use these commands over SSH:

```sh
~/.local/bin/framecolorctl status
~/.local/bin/framecolorctl doctor
~/.local/bin/framecolorctl bench [seconds]
~/.local/bin/framecolorctl disable
~/.local/bin/framecolorctl enable
~/.local/bin/framecolorctl language
```

`disable` turns off both Vulkan correction and the dashboard; `enable` turns them back on. Both commands restart SteamVR. `disable` does not delete saved adjustments or presets. If the display becomes hard to see, run `disable` over SSH.

If SteamVR fails and exits within 10 seconds of starting, the startup guard disables the Vulkan layer after one failure by default. The failure may have a cause unrelated to FrameColor. You can instead allow up to three consecutive failures:

```sh
~/.local/bin/framecolorctl guard 3
~/.local/bin/framecolorctl guard 1
```

The guard cannot guarantee automatic recovery from every failure. Keep a way to recover manually over SSH.

## Performance

`framecolorctl bench [seconds]` reports frame timing (frame count, compositor CPU and GPU time, dropped, mispresented and reprojected frames) and memory while FrameColor runs. It connects as a background app, so it never starts SteamVR, and nothing runs unless you use the command. Measure in the same scene with the dashboard closed, and watch the dropped and reprojected frames first; they are what you notice in the headset.

The compositor GPU time that SteamVR reports does **not** include the pass that the FrameColor layer adds, so `bench` cannot show the cost of sharpening. The per-process GPU time of `vrcompositor` does include it. On the Steam Frame's `msm` driver it is the `drm-engine-gpu` value in `/proc/<pid>/fdinfo/*`. Read it before and after a fixed interval and divide by the number of frames.

Results on 2026-10-28 (SteamOS 0.4.5, BUILD_ID 20261022.6150929; SteamVR 1791331621; layer built in release mode). Each row is two 10 s runs, alternating between 0% and 100%:

| Scene | Sharpness | vrcompositor GPU time per frame | GPU busy | Dropped frames |
| --- | --- | --- | --- | --- |
| Desktop window (Chrome), 90 Hz | 0% | 3.94–4.02 ms | 35–36% | 0 |
| Desktop window (Chrome), 90 Hz | 100% | 4.76–4.78 ms | 43% | 0 |
| Beat Saber menu, 120 Hz | 0% | 2.27–2.48 ms | 27–30% | 0–1 |
| Beat Saber menu, 120 Hz | 100% | 3.06–3.07 ms | 37% | 0 |

Sharpening costs about 0.7–0.8 ms of GPU time per frame and added no dropped frames in these scenes. The shader skips CAS only at 0%; any higher value runs it at full strength and blends, so lower settings are expected to cost the same (not measured). The baseline with FrameColor disabled has not been measured yet. The dashboard used about 47 MiB of RAM (RSS).

## Disclaimer

FrameColor is an unofficial beta tool. SteamVR or SteamOS updates may break it.

Because correction is applied to the final display, it may also change the colors of the dashboard and safety boundaries. Avoid settings that significantly reduce visibility, and check that your surroundings are safe before use. This tool does not disable or modify safety boundaries.

This version supports only the stock SteamVR installation at `/opt/steamvr`. If something goes wrong, first run `~/.local/bin/framecolorctl disable` over SSH. Settings retained by SteamVR itself, such as panel brightness, may remain after correction is disabled. See [LICENSE](LICENSE) for the software's warranty disclaimer.

The original tool was created with the assistance of Codex, and this fork's changes with the assistance of Claude Code. It has been tested on a Steam Frame, but you use it at your own risk.

## Development

Build from source on the Steam Frame. You need CMake, Meson, Ninja, pkg-config, `glslangValidator`, development files for Cairo, FreeType, and Vulkan, and the `libopenvr_api.so` included with SteamVR. Creating a release archive also uses `strip`, `tar`, and `sha256sum`.

```sh
sh contrib/install.sh                 # Build from source and install to user directories
sh tests/test_guard.sh
sh tests/test_framecolorctl.sh
python3 -m unittest discover -s tests -v
sh contrib/package.sh VERSION        # Generate the archive and .sha256 file in dist/
```

If `bin/framecolor` and `lib/libvkbasalt.so` exist in the source directory, `contrib/install.sh` installs them instead of building. Run `contrib/package.sh` on a Steam Frame (ARM64). Generated `build/` and `dist/` directories are excluded from Git.

`tests/offline/harness.cpp` checks settings files, auto presets, language detection and `bench` statistics, and renders the dashboard in English, Japanese and Korean to `panel_*.rgba` (1200×920 RGBA). On a PC, `tests/offline/docker_check.sh` builds and runs everything that does not need a headset in Docker. To run the harness on the Steam Frame, link it with the real OpenVR library (ignoring unresolved symbols fails on ARM64):

```sh
g++ -std=c++17 -Isrc -Ithird_party/openvr $(pkg-config --cflags cairo freetype2 vulkan) \
    tests/offline/harness.cpp src/draw.cpp src/i18n.cpp src/auto_profile.cpp src/bench.cpp \
    src/vr_overlay.cpp src/vk_texture.cpp -o /tmp/harness $(pkg-config --libs cairo freetype2 vulkan) -pthread \
    /opt/steamvr/bin/linuxarm64/libopenvr_api.so -Wl,-rpath,/opt/steamvr/bin/linuxarm64
XDG_CONFIG_HOME=/tmp/framecolor-harness /tmp/harness /tmp
```

## Acknowledgments

The dashboard code and design are adapted from [frameeyeosc](https://github.com/sasaken1102r/frameeyeosc). The overlay uses headers from [OpenVR](https://github.com/ValveSoftware/openvr), and display color correction uses a modified version of [vkBasalt](https://github.com/DadSchoorse/vkBasalt). Thanks to their authors and contributors.

FrameColor's original code is available under the [MIT license](LICENSE). See [Third-party notices](THIRD_PARTY_LICENSES.md) for details about third-party code and the original license texts.
