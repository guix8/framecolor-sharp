# Third-party notices

[日本語版](THIRD_PARTY_LICENSES.ja.md)

## [frameeyeosc](https://github.com/sasaken1102r/frameeyeosc) (MIT)

The dashboard drawing, Vulkan texture upload, and OpenVR overlay code in
`src/draw.*`, `src/vk_texture.*`, and `src/vr_overlay.*` is adapted from
frameeyeosc. Its dashboard design was also a reference. The original work is
by konsti219, with changes in the linked fork by sasaken1102r. The copyright
and permission notice is preserved in
[`third_party/frameeyeosc-LICENSE`](third_party/frameeyeosc-LICENSE).

## [OpenVR](https://github.com/ValveSoftware/openvr) (BSD-3-Clause)

`third_party/openvr/openvr.h` is an OpenVR SDK header. Its license is preserved
in [`third_party/openvr/LICENSE`](third_party/openvr/LICENSE).

## [vkBasalt](https://github.com/DadSchoorse/vkBasalt) (zlib)

The Vulkan layer in `third_party/vkbasalt/` is a modified, LUT-only derivative
of vkBasalt, based on upstream
[commit `4f97f09`](https://github.com/DadSchoorse/vkBasalt/commit/4f97f09ffe91900e6ca136cc26cf7966f8f6970d). The zlib license is preserved in
[`third_party/vkbasalt/LICENSE`](third_party/vkbasalt/LICENSE). FrameColor
limits the layer to `vrcompositor`, updates the LUT from user settings, and
removes unused effects, input hooks, depth capture, image loaders, and
ReShade compiler support.

The source and binary release contain no NVIDIA FXAA, SMAA, DLS, ReShade
compiler, stb image loader, or SPIRV-Headers files. The two retained GLSL
shaders are from vkBasalt. `lut.frag.glsl` also includes the sharpening code
from vkBasalt's `cas.frag.glsl`, an implementation of AMD FidelityFX
Contrast Adaptive Sharpening (MIT, Copyright (c) 2017-2019 Advanced Micro
Devices, Inc.). Its license notice is preserved in that shader.

## System-provided dependencies

[Cairo](https://www.cairographics.org/), [FreeType](https://freetype.org/),
[Vulkan](https://www.vulkan.org/), and the C/C++ runtime are used from the
system.
