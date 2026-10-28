#!/bin/sh
# Off-device check on any PC with Docker (x86 is fine). It does not need a
# Steam Frame, SteamVR or libopenvr_api.so. Run from the repository root:
#
#   docker run --rm -v "$PWD:/src:ro" -v "$PWD/build/offline:/out" ubuntu:24.04 sh /src/tests/offline/docker_check.sh
#
# It validates the shader, builds the Vulkan layer, compiles the dashboard
# with -Wall -Wextra, runs tests/offline/harness.cpp, runs the shell tests,
# and writes dashboard screenshots to build/offline/panel_en.png, panel_ja.png and panel_ko.png.
set -eu
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null
apt-get install -y -qq --no-install-recommends glslang-tools meson ninja-build g++ pkg-config \
    libvulkan-dev libcairo2-dev libfreetype-dev fonts-noto-cjk spirv-tools python3-pil >/dev/null

rm -rf /work && cp -r /src /work && cd /work
mkdir -p /out

echo "== shader compile + validate"
glslangValidator -V third_party/vkbasalt/src/shader/lut.frag.glsl -o /tmp/lut.spv
spirv-val /tmp/lut.spv && echo "spirv-val OK"

echo "== layer build (meson)"
meson setup /tmp/layer third_party/vkbasalt >/dev/null
ninja -C /tmp/layer 2>&1 | grep -Ei "warning|error" || true
ninja -C /tmp/layer >/dev/null && echo "layer OK"

echo "== dashboard compile (-Wall -Wextra, no link: libopenvr_api.so is on the headset)"
for f in src/framecolor_main.cpp src/i18n.cpp src/auto_profile.cpp src/bench.cpp src/draw.cpp src/vk_texture.cpp src/vr_overlay.cpp; do
    g++ -std=c++17 -Wall -Wextra -c "$f" -Ithird_party/openvr $(pkg-config --cflags cairo freetype2 vulkan) -o /tmp/$(basename "$f").o
done
echo "dashboard objects OK"

echo "== harness"
mkdir -p /usr/share/fonts/noto-cjk
ln -sf /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc /usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc
ln -sf /usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc /usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc
# OpenVR symbols stay unresolved; the harness never calls them.
g++ -std=c++17 -Isrc -Ithird_party/openvr $(pkg-config --cflags cairo freetype2) tests/offline/harness.cpp \
    src/draw.cpp src/i18n.cpp src/auto_profile.cpp src/bench.cpp \
    -o /tmp/harness $(pkg-config --libs cairo freetype2) -Wl,--unresolved-symbols=ignore-all
XDG_CONFIG_HOME=/tmp/cfg /tmp/harness /tmp
python3 -c "
from PIL import Image
for n in ('en', 'ja', 'ko'):
    Image.frombytes('RGBA', (1200, 920), open('/tmp/panel_%s.rgba' % n, 'rb').read()).save('/out/panel_%s.png' % n)
"
echo "screenshots: build/offline/panel_en.png, panel_ja.png, panel_ko.png"

echo "== shell tests"
sh tests/test_guard.sh >/dev/null
sh tests/test_framecolorctl.sh >/dev/null
echo "shell tests OK"
