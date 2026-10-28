# FrameColor

[English](README.md) | [日本語](README.ja.md)

## 이 포크에 대해

[beko-kerbecotton/framecolor](https://github.com/beko-kerbecotton/framecolor)의 포크입니다. 다음 기능을 추가했습니다.

- **선명도**: AMD FidelityFX CAS를 기존 색 보정 처리 안에서 실행합니다. 화면 전체를 한 번 더 처리하는 단계가 늘지 않습니다. 독립형 앱은 패널보다 낮은 해상도로 렌더링하는 경우가 많은데, 이렇게 확대된 화면을 더 또렷하게 보이게 합니다.
- **길게 눌러 비교**: 버튼을 누르고 있는 동안 보정하지 않은 화면을 보여 줍니다.
- **자동 프리셋**: 모드(독립형 또는 PC 스트리밍)나 지금 실행 중인 SteamVR 앱에 따라 프리셋을 적용합니다.
- **`framecolorctl bench`**: FrameColor가 동작하는 동안 프레임 타이밍과 메모리를 측정합니다.
- **한국어 화면**과 Steam 클라이언트 언어를 따르는 **언어 자동 선택**
- Vulkan 레이어 릴리스 빌드. 원본의 `install.sh`와 `package.sh`는 최적화 없이 빌드했습니다.

[brugr/framecolor](https://github.com/brugr/framecolor)의 변경 두 가지도 들어 있습니다. 영어·일본어 번역, 그리고 FrameColor가 다른 Vulkan 레이어와 함께 로드될 때 `vrcompositor`가 죽는 문제의 수정입니다.

이 포크에는 릴리스 압축 파일이 없습니다. [설치](#설치)에 적힌 대로 Steam Frame에서 빌드해서 설치합니다.

## 개요

FrameColor로 헤드셋 안의 SteamVR 대시보드에서 Steam Frame 디스플레이의 색을 조정할 수 있습니다. 패널 밝기와 함께 밝기, 채도, 대비, 감마를 전체 또는 빨강·초록·파랑 채널별로 조정하고, 화면을 선명하게 할 수 있습니다. 표준, 선명, 야간 프리셋과 직접 저장하는 프리셋 슬롯 3개가 있습니다.

FrameColor는 Steam Frame 전용입니다. 색 보정은 SteamVR의 `vrcompositor`에 로드되는 Vulkan 레이어가 합니다. 헤드셋에서 실행하는 앱과 스트리밍 화면뿐 아니라 대시보드 등 화면에 표시되는 모든 것에 적용됩니다.

## 요구 사항

- `/opt/steamvr`에 설치된 기본 SteamVR을 쓰는 Steam Frame(SteamOS, ARM64)
- Steam Frame에서 명령을 실행할 수 있도록 개발자 모드와 SSH 접속
- 설치할 때 LUT를 만드는 데 쓰는 Python 3.10 이상(Steam Frame에 설치되어 있어야 함)
- [개발](#개발)에 적힌 빌드 도구. 2026-10-28 기준으로 Steam Frame의 SteamOS에 모두 들어 있었습니다.

## 설치

아래 명령은 Steam Frame에서 실행합니다. 예를 들어 SSH로 접속해서 실행합니다(`ssh steamos@FRAME_IP`). 이 저장소를 클론한 뒤 빌드하고 설치합니다.

```sh
git clone <이 저장소의 주소> framecolor
cd framecolor
sh contrib/install.sh
~/.local/bin/framecolorctl enable
```

설치 스크립트는 `sudo` 없이 일반 사용자로 실행합니다. 대시보드와 Vulkan 레이어를 소스에서 빌드해서 `~/.local`과 `~/.config` 폴더에만 설치합니다. 처음 설치한 직후에는 Vulkan 보정이 꺼져 있습니다. 마지막 `enable` 명령이 보정과 대시보드를 함께 켜고 SteamVR을 다시 시작합니다.

원본 FrameColor를 이미 쓰고 있다면 같은 방법으로 덮어서 설치합니다. 저장한 조정값과 프리셋은 그대로 남고, 예전 형식의 파일도 읽습니다. 보정이 이미 켜져 있으면 설치 스크립트가 SteamVR을 다시 시작합니다.

업데이트하려면 `git pull`을 실행하고 `sh contrib/install.sh`를 다시 실행합니다.

FrameColor를 완전히 지우려면 저장소 폴더에서 다음을 실행합니다.

```sh
sh contrib/uninstall.sh
```

제거하면 저장한 조정값과 프리셋도 지워집니다. SteamVR이 저장한 패널 밝기는 남을 수 있습니다. 필요하면 제거하기 전에 대시보드의 초기화로 표준 설정으로 되돌립니다.

## 설정

SteamVR 대시보드에서 “디스플레이 색 조정”(Display Color Adjustment)을 엽니다.
화면 언어는 영어, 일본어, 한국어입니다. 기본값은 Steam 클라이언트 언어를 따르고, 그 언어를 지원하지 않으면 시스템 로캘을 보고, 둘 다 아니면 영어로 표시합니다. 패널 오른쪽 위의 언어 메뉴에서 언어를 고르거나, “자동”을 골라 다시 Steam 언어를 따르게 할 수 있습니다. `framecolorctl language en|ja|ko|auto`로도 바꿀 수 있습니다.

| 항목 | 범위 | 적용 대상 |
| --- | --- | --- |
| 패널 밝기 | 40–125% | SteamVR의 패널 밝기 설정. 모든 채널 공통 |
| 밝기 | 50–150% | Vulkan 보정. 전체 또는 RGB 채널별 |
| 채도 | 0–200% | Vulkan 보정. 전체 또는 RGB 채널별. 100%가 표준 |
| 대비 | 0–100% | Vulkan 보정. 전체 또는 RGB 채널별. 0%가 표준 |
| 감마 | 0.50–2.50 | Vulkan 보정. 전체 또는 RGB 채널별. 1.00이 표준 |
| 선명도 | 0–100% | Vulkan 보정(AMD CAS). 모든 채널 공통. 0%는 끔 |

조정 채널에서 전체, 빨강, 초록, 파랑 중 하나를 고릅니다. 전체를 고른 상태에서 항목을 움직이면 세 채널의 현재 값이 함께 바뀝니다. 패널 밝기와 선명도는 채널 선택과 관계없습니다. 지금 설정을 저장하려면 저장을 누른 뒤 P1, P2, P3 중 하나를 고릅니다. 초기화는 표준 프리셋을 적용합니다.

**길게 눌러 비교**를 누르고 있으면 FrameColor의 보정이 없는 화면이 보입니다. 버튼을 놓으면 원래 설정으로 돌아갑니다.

**자동 프리셋**은 표시 모드나 앱이 바뀔 때 프리셋을 적용합니다. 버튼을 누를 때마다 끔, 표준, 선명, 야간, 저장한 P1–P3 순서로 바뀝니다.

- **독립형**: PC 스트리밍을 하지 않는 동안 씁니다.
- **PC 스트리밍**: `vrlink`가 실행 중일 때(PC 스트리밍 중일 때) 씁니다.
- **이 앱**: 지금의 SteamVR 앱이 실행 중일 때 씁니다. 모드 규칙보다 우선합니다. 앱에 SteamVR 앱 키가 없으면 쓸 수 없습니다.

FrameColor는 1초마다 모드와 앱을 확인하고, 같은 변화를 두 번 연속 보면 프리셋을 바꿉니다. 새 규칙은 다음 전환 때 적용됩니다. 직접 바꾼 값은 모드나 앱이 다시 바뀔 때까지 유지됩니다. 모든 규칙에서 벗어나면(예를 들어 앱을 종료했는데 모드 규칙이 끔이면) 첫 자동 프리셋을 적용하기 전의 설정으로 되돌립니다. 규칙은 `~/.config/framecolor/auto.txt`에 저장됩니다.

SSH에서 다음 명령도 쓸 수 있습니다.

```sh
~/.local/bin/framecolorctl status
~/.local/bin/framecolorctl doctor
~/.local/bin/framecolorctl bench [초]
~/.local/bin/framecolorctl disable
~/.local/bin/framecolorctl enable
~/.local/bin/framecolorctl language
```

`disable`은 Vulkan 보정과 대시보드를 함께 끄고, `enable`은 둘 다 다시 켭니다. 두 명령 모두 SteamVR을 다시 시작합니다. `disable`로는 저장한 조정값과 프리셋이 지워지지 않습니다. 화면이 잘 보이지 않게 되면 SSH에서 `disable`을 실행합니다.

SteamVR이 시작 후 10초 안에 실패해서 꺼지면, 시작 보호 장치(guard)가 기본적으로 한 번 실패한 뒤 Vulkan 레이어를 끕니다. 실패 원인이 FrameColor와 관계없을 수도 있습니다. 대신 연속 세 번까지 실패를 허용할 수 있습니다.

```sh
~/.local/bin/framecolorctl guard 3
~/.local/bin/framecolorctl guard 1
```

보호 장치가 모든 실패에서 자동 복구를 보장하지는 않습니다. SSH로 직접 복구할 방법을 확보해 두세요.

## 성능

`framecolorctl bench [초]`는 FrameColor가 동작하는 동안의 프레임 타이밍(프레임 수, 컴포지터 CPU·GPU 시간, 드롭·미스프레젠트·리프로젝션된 프레임)과 메모리를 보여 줍니다. 백그라운드 앱으로 연결하기 때문에 SteamVR을 시작하지 않고, 명령을 쓰지 않으면 아무것도 실행되지 않습니다. 대시보드를 닫고 같은 장면에서 측정하고, 드롭과 리프로젝션된 프레임부터 봅니다. 헤드셋에서 실제로 느껴지는 것이 이 값입니다.

SteamVR이 알려 주는 컴포지터 GPU 시간에는 FrameColor 레이어가 추가하는 처리가 **들어 있지 않습니다**. 그래서 `bench`로는 선명도의 비용을 알 수 없습니다. `vrcompositor`의 프로세스별 GPU 시간에는 들어 있습니다. Steam Frame의 `msm` 드라이버에서는 `/proc/<pid>/fdinfo/*`의 `drm-engine-gpu` 값입니다. 일정 시간의 앞뒤에서 이 값을 읽고 프레임 수로 나눕니다.

2026-10-28 측정 결과입니다(SteamOS 0.4.5, BUILD_ID 20261022.6150929, SteamVR 1791331621, 레이어 릴리스 빌드). 각 행은 0%와 100%를 번갈아 10초씩 두 번 잰 값입니다.

| 장면 | 선명도 | 프레임당 vrcompositor GPU 시간 | GPU 사용률 | 드롭된 프레임 |
| --- | --- | --- | --- | --- |
| 데스크톱 창(Chrome), 90 Hz | 0% | 3.94–4.02 ms | 35–36% | 0 |
| 데스크톱 창(Chrome), 90 Hz | 100% | 4.76–4.78 ms | 43% | 0 |
| Beat Saber 메뉴, 120 Hz | 0% | 2.27–2.48 ms | 27–30% | 0–1 |
| Beat Saber 메뉴, 120 Hz | 100% | 3.06–3.07 ms | 37% | 0 |

선명도는 프레임당 GPU 시간을 약 0.7–0.8 ms 더 쓰고, 이 장면들에서는 드롭된 프레임을 늘리지 않았습니다. 셰이더는 0%일 때만 CAS를 건너뛰고, 그보다 크면 최대 강도로 계산한 뒤 섞습니다. 그래서 낮은 설정도 비용이 같을 것으로 봅니다(미측정). FrameColor를 끈 기준값은 아직 재지 않았습니다. 대시보드는 메모리(RSS)를 약 47 MiB 썼습니다.

## 면책

FrameColor는 비공식 베타 도구입니다. SteamVR이나 SteamOS 업데이트로 동작하지 않게 될 수 있습니다.

보정은 최종 화면에 적용되므로 대시보드와 안전 경계의 색도 바뀔 수 있습니다. 화면이 크게 안 보이게 되는 설정은 피하고, 쓰기 전에 주변이 안전한지 확인하세요. 이 도구는 안전 경계를 끄거나 바꾸지 않습니다.

이 버전은 `/opt/steamvr`에 설치된 기본 SteamVR만 지원합니다. 문제가 생기면 먼저 SSH에서 `~/.local/bin/framecolorctl disable`을 실행하세요. 패널 밝기처럼 SteamVR 자체가 저장한 설정은 보정을 끈 뒤에도 남을 수 있습니다. 소프트웨어의 보증 부인은 [LICENSE](LICENSE)를 보세요.

원본 도구는 Codex의 도움을 받아, 이 포크의 변경은 Claude Code의 도움을 받아 만들었습니다. Steam Frame에서 시험했지만, 사용에 따른 책임은 사용자에게 있습니다.

## 개발

Steam Frame에서 소스로 빌드합니다. CMake, Meson, Ninja, pkg-config, `glslangValidator`, Cairo·FreeType·Vulkan 개발 파일, 그리고 SteamVR에 들어 있는 `libopenvr_api.so`가 필요합니다. 릴리스 압축 파일을 만들 때는 `strip`, `tar`, `sha256sum`도 씁니다.

```sh
sh contrib/install.sh                 # 소스로 빌드해서 사용자 폴더에 설치
sh tests/test_guard.sh
sh tests/test_framecolorctl.sh
python3 -m unittest discover -s tests -v
sh contrib/package.sh VERSION        # dist/에 압축 파일과 .sha256 파일 생성
```

소스 폴더에 `bin/framecolor`와 `lib/libvkbasalt.so`가 있으면 `contrib/install.sh`는 빌드하지 않고 그 파일을 설치합니다. `contrib/package.sh`는 Steam Frame(ARM64)에서 실행합니다. 생성되는 `build/`와 `dist/` 폴더는 Git에서 제외됩니다.

`tests/offline/harness.cpp`는 설정 파일, 자동 프리셋, 언어 감지, `bench` 통계를 검사하고, 대시보드를 영어·일본어·한국어로 그려 `panel_*.rgba`(1200×920 RGBA)로 저장합니다. PC에서는 `tests/offline/docker_check.sh`가 헤드셋이 필요 없는 검사를 Docker에서 모두 빌드하고 실행합니다. Steam Frame에서 하네스를 실행하려면 실제 OpenVR 라이브러리와 링크합니다(정의되지 않은 심볼을 무시하는 방식은 ARM64에서 실패합니다).

```sh
g++ -std=c++17 -Isrc -Ithird_party/openvr $(pkg-config --cflags cairo freetype2 vulkan) \
    tests/offline/harness.cpp src/draw.cpp src/i18n.cpp src/auto_profile.cpp src/bench.cpp \
    src/vr_overlay.cpp src/vk_texture.cpp -o /tmp/harness $(pkg-config --libs cairo freetype2 vulkan) -pthread \
    /opt/steamvr/bin/linuxarm64/libopenvr_api.so -Wl,-rpath,/opt/steamvr/bin/linuxarm64
XDG_CONFIG_HOME=/tmp/framecolor-harness /tmp/harness /tmp
```

## 감사의 말

대시보드 코드와 디자인은 [frameeyeosc](https://github.com/sasaken1102r/frameeyeosc)를 바탕으로 했습니다. 오버레이는 [OpenVR](https://github.com/ValveSoftware/openvr) 헤더를 쓰고, 디스플레이 색 보정은 [vkBasalt](https://github.com/DadSchoorse/vkBasalt)를 수정해서 씁니다. 만든 분들과 기여자분들께 감사드립니다.

FrameColor의 원본 코드는 [MIT 라이선스](LICENSE)로 제공됩니다. 서드파티 코드와 원본 라이선스 전문은 [서드파티 고지](THIRD_PARTY_LICENSES.md)를 보세요.
