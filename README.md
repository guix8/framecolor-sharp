# FrameColor

[日本語](README.ja.md) | [한국어](README.ko.md)

## About this fork

This is a fork of [beko-kerbecotton/framecolor](https://github.com/beko-kerbecotton/framecolor). It adds:

- **Sharpness**: AMD FidelityFX CAS, run inside the existing color correction pass, so no extra full-screen pass is added. Standalone apps often render below the panel resolution; this makes the upscaled image look sharper. By default it is applied around the center of each eye and fades out towards the lens edges, which are blurred anyway; this halves its cost (about 0.4 ms instead of 0.8 ms per frame on the Steam Frame). You can switch it to the whole image.
- **Hold to Compare** and **Auto Compare**: hold the button to see the image without correction, or let it switch every 3 seconds so you can compare inside a game.
- **Auto presets**: apply a preset by mode (standalone or PC streaming) or for the current SteamVR app.
- **`framecolorctl bench`**: measure GPU time, frame timing and memory while FrameColor runs.
- **Korean interface** and **automatic language selection** that follows the Steam client language.
- A release build of the Vulkan layer. Upstream's `install.sh` and `package.sh` built it without optimization (proposed upstream in beko-kerbecotton/framecolor#3).

It also includes two changes from [brugr/framecolor](https://github.com/brugr/framecolor): English and Japanese translations, and a fix for a `vrcompositor` crash when FrameColor loads alongside other Vulkan layers.

Prebuilt archives are on this repository's Releases page. See [Installation](#installation).

## Overview

FrameColor lets you adjust the Steam Frame's display colors from the SteamVR dashboard inside the headset. Alongside panel brightness, you can adjust brightness, saturation, contrast, and gamma globally or separately for the red, green, and blue channels, and sharpen the image. It includes Standard, Vivid, and Night presets, plus three slots for your own presets.

FrameColor is for Steam Frame only. Color correction is applied by a Vulkan layer loaded into SteamVR's `vrcompositor`. It affects locally run and streamed content, as well as the dashboard and other displayed elements.

## Requirements

- A Steam Frame (SteamOS, ARM64) using the stock SteamVR installation at `/opt/steamvr`
- Developer Mode and SSH access, so you can transfer the archive to the Steam Frame and run commands on it
- Python 3.10 or later on the Steam Frame, used to generate the LUT during installation
- To build from source only: the build tools listed under [Development](#development). SteamOS on the Steam Frame shipped with all of them on 2026-10-28.

## Installation

### From a release archive

Download `framecolor-VERSION-steamframe-aarch64.tar.gz` and the matching `.sha256` file from this repository's Releases page. Replace `VERSION` and `FRAME_IP` below with the release version (for example `0.1.0-sharp.2`) and your Steam Frame's IP address.

From your PC, copy both files to the Steam Frame's home directory and connect over SSH:

```sh
scp framecolor-VERSION-steamframe-aarch64.tar.gz framecolor-VERSION-steamframe-aarch64.tar.gz.sha256 steamos@FRAME_IP:
ssh steamos@FRAME_IP
```

Run the remaining commands on the Steam Frame. Verify the archive, then extract and install it:

```sh
sha256sum -c framecolor-VERSION-steamframe-aarch64.tar.gz.sha256
tar -xzf framecolor-VERSION-steamframe-aarch64.tar.gz
cd framecolor
sh contrib/install.sh
~/.local/bin/framecolorctl enable
```

### From source

Run these commands on the Steam Frame. Clone this repository, then build and install:

```sh
git clone <this repository's URL> framecolor
cd framecolor
sh contrib/install.sh
~/.local/bin/framecolorctl enable
```

### Notes

Run the installer as your regular user, without `sudo`. It installs the prebuilt binaries from a release archive, or builds the dashboard and the Vulkan layer from source when there are none, and uses only your `~/.local` and `~/.config` directories. After a fresh installation the dashboard runs, but Vulkan correction is disabled and the dashboard's color controls stay disabled until you run `enable`, which enables the correction and restarts SteamVR.

If you already use upstream FrameColor, install over it the same way. Saved adjustments and presets are kept, and their older file formats are read. If correction is already enabled, the installer restarts SteamVR.

To update, install a newer release archive the same way, or run `git pull` and `sh contrib/install.sh` again.

To remove FrameColor completely, run this from the extracted archive or repository directory:

```sh
sh contrib/uninstall.sh
```

Uninstalling also deletes saved adjustments and presets. Panel brightness stored by SteamVR may remain. If necessary, use Reset in the dashboard to return to the standard setting before uninstalling.

## Settings

Open **Display** (the RGB icon) in the SteamVR dashboard. The panel is titled “Display Color Adjustment”.
The interface is available in English, Japanese and Korean. By default it follows the Steam client language, then the system locale, and falls back to English. Choose a language, or Auto to follow Steam again, from the language drop-down menu in the top-right corner of the overlay panel, or run `framecolorctl language en|ja|ko|auto`. A change made with `framecolorctl language` takes effect the next time the dashboard starts (after SteamVR restarts, or after `systemctl --user restart framecolor.service`).

| Control | Range | Applies to |
| --- | --- | --- |
| Panel brightness | 40–125% | SteamVR panel brightness setting; shared across channels |
| Brightness | 50–150% | Vulkan correction; global or per RGB channel |
| Saturation | 0–200% | Vulkan correction; global or per RGB channel; 100% is standard |
| Contrast | 0–100% | Vulkan correction; global or per RGB channel; 0% is standard |
| Gamma | 0.50–2.50 | Vulkan correction; global or per RGB channel; 1.00 is standard |
| Sharpness | 0–100% | Vulkan correction (AMD CAS); around the center of each eye by default, or the whole image; shared across channels; 0% is off |

Under Adjustment Channel, choose All, Red, Green, or Blue. Moving a control with All selected changes the current values of all three channels together. Panel brightness and sharpness are unaffected by the channel selection. To save the current settings, select Save and then P1, P2, or P3. Reset applies the Standard preset.

The button next to Sharpness chooses where sharpening applies. **Center only** (default) sharpens around the center of each eye and fades out towards the lens edges, which are blurred anyway. **Whole image** sharpens the whole image; the sharpening then costs about twice as much (about 0.8 ms instead of 0.4 ms per frame; see [Performance](#performance)). This setting is not part of presets.

Press and hold **Hold to Compare** to see the image without FrameColor's Vulkan correction (color and sharpening). Release it to return to your settings. Panel brightness stays as set.

To compare inside a game, where you cannot hold the button, press **Auto Compare** and hide the dashboard (not with the panel's X, which closes the panel and stops auto compare). The image switches between the uncorrected image and your settings every 3 seconds, starting with the uncorrected one; the button shows which one is on screen. Auto compare turns itself off after 60 seconds. Press it again to stop earlier. Auto presets wait while it runs. If the dashboard stops responding while the uncorrected image is shown, FrameColor returns to your settings within 5 seconds.

**Auto Preset** applies a preset when the display mode or the app changes. Press a button to step through Off, Standard, Vivid, Night, and your saved P1–P3:

- **Standalone**: used while no PC stream runs.
- **PC Streaming**: used while `vrlink` runs (a PC stream is active).
- **This App**: used while the current SteamVR scene app runs. It wins over the mode rules. It is unavailable when the app has no SteamVR app key.

FrameColor checks the mode and the app once per second and switches after it sees the same change twice. A new rule applies at the next switch. Your manual changes stay until the mode or the app changes again. When you leave all rules (for example, you quit an app and the mode rule is Off), FrameColor restores the settings from before the first auto preset. Rules are saved in `~/.config/framecolor/auto.txt`.

You can also use these commands over SSH:

```sh
~/.local/bin/framecolorctl status
~/.local/bin/framecolorctl doctor
~/.local/bin/framecolorctl bench [seconds]   # default 8, 1–60; see Performance
~/.local/bin/framecolorctl disable
~/.local/bin/framecolorctl enable
~/.local/bin/framecolorctl language          # shows the setting; add en, ja, ko or auto to change it
```

`disable` turns off both Vulkan correction and the dashboard; `enable` turns them back on. Both commands restart SteamVR. `disable` does not delete saved adjustments or presets. If the display becomes hard to see, run `disable` over SSH.

If SteamVR fails and exits within 10 seconds of starting, the startup guard disables the Vulkan layer after one failure by default. The failure may have a cause unrelated to FrameColor. To disable the layer only after three consecutive failures, run `framecolorctl guard 3`; `framecolorctl guard 1` restores the default.

The guard cannot guarantee automatic recovery from every failure. Keep a way to recover manually over SSH.

## Performance

`framecolorctl bench [seconds]` reports, while FrameColor runs:

- the GPU time of `vrcompositor` per frame and how busy the GPU is with it. This comes from the `drm-engine-gpu` value in `/proc/<pid>/fdinfo/*` (the Steam Frame's `msm` driver) and includes FrameColor's pass.
- SteamVR's frame timing: frame count, compositor CPU and GPU time, and dropped, mispresented and reprojected frames. SteamVR's compositor GPU time does **not** include FrameColor's pass.
- memory: RSS of `vrcompositor` and the dashboard, and the GPU memory of `vrcompositor`.

It connects as a background app, so it never starts SteamVR, and nothing runs unless you use the command. Measure in the same scene with the dashboard closed, and watch the dropped and reprojected frames first; they are what you notice in the headset.

Other VR overlays change the result a lot. A browser extension that showed a 3D overlay through SteamVR raised `vrcompositor`'s GPU time by roughly 0.5–1 ms per frame, so close such overlays first. Scenes also drift. To compare two settings, switch between them every few seconds many times and compare neighboring intervals, rather than running one long measurement for each. The repository does not include a script for this yet.

FrameColor works on the final image that `vrcompositor` presents after lens distortion: 4320×2160, the two 2160×2160 eyes side by side. That is why sharpening can be limited to the center of each eye.

Results on 2026-10-28 (SteamOS 0.4.5, BUILD_ID 20261022.6150929; SteamVR 1791331621; layer built in release mode). Scene: Beat Saber menu at 120 Hz, no other overlays. Sharpening costs were measured by switching sharpness every 2 s, 12 pairs per run (± is two standard errors):

| What | vrcompositor GPU time per frame |
| --- | --- |
| FrameColor disabled | 1.59–1.61 ms (19% busy) |
| FrameColor enabled, sharpness 0% | 1.7–2.1 ms, roughly 0.1–0.5 ms more (estimate: measured before and after a SteamVR restart, not in pairs) |
| Sharpening, Center, 0% → 60% | +0.42 ms (±0.10) |
| Sharpening, Center, 0% → 100% | +0.38 / +0.43 ms in two runs (±0.06 / ±0.07) |
| Sharpening, Whole, 0% → 60% | +0.82 ms (±0.18) |
| Sharpening, Whole, 0% → 100% | +0.78 ms (±0.05) |

The Whole rows were measured with 0.1.0-sharp.1, which always sharpened the whole image with the same shader code. No extra dropped frames were seen with sharpening. Any sharpness above 0% runs CAS at full strength and blends, so the cost hardly depends on the value. With FrameColor enabled, `vrcompositor` used about 85 MiB more GPU memory (885 MiB instead of 800 MiB), roughly the size of two to three copies of the 4320×2160 image (about 36 MiB each). The dashboard used about 50 MiB of RAM (RSS).

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

`tests/offline/harness.cpp` checks settings files, the sharpen area and Auto Compare buttons, auto presets, language detection, `bench` statistics and DRM fdinfo parsing, and renders the dashboard in English, Japanese and Korean to `panel_*.rgba` (1200×920 RGBA). On a PC, `tests/offline/docker_check.sh` builds and runs everything that does not need a headset in Docker. To run the harness on the Steam Frame, link it with the real OpenVR library (ignoring unresolved symbols fails on ARM64):

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
