# FrameColor

[English](README.md) | [日本語](README.ja.md)

## 이 포크에 대해

[beko-kerbecotton/framecolor](https://github.com/beko-kerbecotton/framecolor)의 포크입니다. 다음 기능을 추가했습니다.

- **선명도**: AMD FidelityFX CAS를 기존 색 보정 처리 안에서 실행합니다. 화면 전체를 한 번 더 처리하는 단계가 늘지 않습니다. 독립형 앱은 패널보다 낮은 해상도로 렌더링하는 경우가 많은데, 이렇게 확대된 화면을 더 또렷하게 보이게 합니다. 기본값은 각 눈의 중앙에 적용하고, 원래 흐린 렌즈 가장자리로 갈수록 약해집니다. 덕분에 비용이 절반으로 줄었습니다(Steam Frame에서 프레임당 0.8 ms 대신 약 0.4 ms). 화면 전체에 적용하도록 바꿀 수도 있습니다.
- **길게 눌러 비교**와 **자동 비교**: 버튼을 누르고 있는 동안 보정하지 않은 화면을 보여 줍니다. 게임 안에서 비교할 수 있게 3초마다 저절로 바뀌게 할 수도 있습니다.
- **자동 프리셋**: 모드(독립형 또는 PC 스트리밍)나 지금 실행 중인 SteamVR 앱에 따라 프리셋을 적용합니다.
- **`framecolorctl bench`**: FrameColor가 동작하는 동안 GPU 시간, 프레임 타이밍, 메모리를 측정하거나 두 선명도 값을 비교합니다.
- **한국어 화면**과 Steam 클라이언트 언어를 따르는 **언어 자동 선택**
- Vulkan 레이어 릴리스 빌드. 원본의 `install.sh`와 `package.sh`는 최적화 없이 빌드했습니다(원본에 beko-kerbecotton/framecolor#3으로 제안).

[brugr/framecolor](https://github.com/brugr/framecolor)의 변경 두 가지도 들어 있습니다. 영어·일본어 번역, 그리고 FrameColor가 다른 Vulkan 레이어와 함께 로드될 때 `vrcompositor`가 죽는 문제의 수정입니다.

미리 빌드한 압축 파일은 이 저장소의 Releases 페이지에 있습니다. [설치](#설치)를 보세요.

## 개요

FrameColor로 헤드셋 안의 SteamVR 대시보드에서 Steam Frame 디스플레이의 색을 조정할 수 있습니다. 패널 밝기와 함께 밝기, 채도, 대비, 감마를 전체 또는 빨강·초록·파랑 채널별로 조정하고, 화면을 선명하게 할 수 있습니다. 표준, 비비드, 야간 프리셋과 직접 저장하는 프리셋 슬롯 3개가 있습니다.

FrameColor는 Steam Frame 전용입니다. 색 보정은 SteamVR의 `vrcompositor`에 로드되는 Vulkan 레이어가 합니다. 헤드셋에서 실행하는 앱과 스트리밍 화면뿐 아니라 대시보드 등 화면에 표시되는 모든 것에 적용됩니다.

## 요구 사항

- `/opt/steamvr`에 설치된 기본 SteamVR을 쓰는 Steam Frame(SteamOS, ARM64)
- Steam Frame에서 명령을 실행할 수 있도록 개발자 모드와 SSH 접속
- 설치할 때 LUT를 만드는 데 쓰는 Python 3.10 이상(Steam Frame에 설치되어 있어야 함)
- 소스로 빌드할 때만: [개발](#개발)에 적힌 빌드 도구. 2026-10-28 기준으로 Steam Frame의 SteamOS에 모두 들어 있었습니다.

## 설치

### 릴리스 압축 파일로 설치

이 저장소의 Releases 페이지에서 `framecolor-VERSION-steamframe-aarch64.tar.gz`와 같은 이름의 `.sha256` 파일을 받습니다. 아래의 `VERSION`과 `FRAME_IP`는 릴리스 버전(예: `0.1.0-sharp.2`)과 Steam Frame의 IP 주소로 바꿉니다.

PC에서 두 파일을 Steam Frame의 홈 폴더로 복사하고 SSH로 접속합니다.

```sh
scp framecolor-VERSION-steamframe-aarch64.tar.gz framecolor-VERSION-steamframe-aarch64.tar.gz.sha256 steamos@FRAME_IP:
ssh steamos@FRAME_IP
```

나머지 명령은 Steam Frame에서 실행합니다. 압축 파일을 확인한 뒤 풀어서 설치합니다.

```sh
sha256sum -c framecolor-VERSION-steamframe-aarch64.tar.gz.sha256
tar -xzf framecolor-VERSION-steamframe-aarch64.tar.gz
cd framecolor
sh contrib/install.sh
~/.local/bin/framecolorctl enable
```

### 소스로 빌드해서 설치

아래 명령은 Steam Frame에서 실행합니다. 이 저장소를 클론한 뒤 빌드하고 설치합니다.

```sh
git clone <이 저장소의 주소> framecolor
cd framecolor
sh contrib/install.sh
~/.local/bin/framecolorctl enable
```

### 참고

설치 스크립트는 `sudo` 없이 일반 사용자로 실행합니다. 릴리스 압축 파일에 든 실행 파일을 설치하고, 실행 파일이 없으면 대시보드와 Vulkan 레이어를 소스에서 빌드합니다. `~/.local`과 `~/.config` 폴더만 씁니다. 처음 설치한 직후에는 대시보드는 실행되지만 Vulkan 보정이 꺼져 있어서, 대시보드의 색 조정 항목도 쓸 수 없습니다. 마지막 `enable` 명령이 보정을 켜고 SteamVR을 다시 시작합니다.

원본 FrameColor를 이미 쓰고 있다면 같은 방법으로 덮어서 설치합니다. 저장한 조정값과 프리셋은 그대로 남고, 예전 형식의 파일도 읽습니다. 보정이 이미 켜져 있으면 설치 스크립트가 SteamVR을 다시 시작합니다.

업데이트하려면 새 릴리스 압축 파일을 같은 방법으로 설치하거나, `git pull`을 실행하고 `sh contrib/install.sh`를 다시 실행합니다.

FrameColor를 완전히 지우려면 압축을 푼 폴더나 저장소 폴더에서 다음을 실행합니다.

```sh
sh contrib/uninstall.sh
```

제거하면 저장한 조정값과 프리셋도 지워집니다. SteamVR이 저장한 패널 밝기는 남을 수 있습니다. 필요하면 제거하기 전에 대시보드의 초기화로 표준 설정으로 되돌립니다.

## 설정

SteamVR 대시보드에서 **디스플레이**(RGB 아이콘)를 엽니다. 패널 제목은 “디스플레이 색 조정”입니다.
화면 언어는 영어, 일본어, 한국어입니다. 기본값은 Steam 클라이언트 언어를 따르고, 그 언어를 지원하지 않으면 시스템 로캘을 보고, 둘 다 아니면 영어로 표시합니다. 패널 오른쪽 위의 언어 메뉴에서 언어를 고르거나, “자동”을 골라 다시 Steam 언어를 따르게 할 수 있습니다. `framecolorctl language en|ja|ko|auto`로도 바꿀 수 있습니다. 이 명령으로 바꾼 언어는 대시보드가 다음에 시작할 때 적용됩니다(SteamVR을 다시 시작하거나 `systemctl --user restart framecolor.service`를 실행한 뒤).

| 항목 | 범위 | 적용 대상 |
| --- | --- | --- |
| 패널 밝기 | 40–125% | SteamVR의 패널 밝기 설정. 모든 채널 공통 |
| 밝기 | 50–150% | Vulkan 보정. 전체 또는 RGB 채널별 |
| 채도 | 0–200% | Vulkan 보정. 전체 또는 RGB 채널별. 100%가 표준 |
| 대비 | 0–100% | Vulkan 보정. 전체 또는 RGB 채널별. 0%가 표준 |
| 감마 | 0.50–2.50 | Vulkan 보정. 전체 또는 RGB 채널별. 1.00이 표준 |
| 선명도 | 0–100% | Vulkan 보정(AMD CAS). 기본은 각 눈의 중앙, 전체로 바꿀 수 있음. 모든 채널 공통. 0%는 끔 |

조정 채널에서 전체, 빨강, 초록, 파랑 중 하나를 고릅니다. 전체를 고른 상태에서 항목을 움직이면 세 채널의 현재 값이 함께 바뀝니다. 패널 밝기와 선명도는 채널 선택과 관계없습니다. 지금 설정을 저장하려면 저장을 누른 뒤 P1, P2, P3 중 하나를 고릅니다. 초기화는 표준 프리셋을 적용합니다.

선명도 옆의 버튼으로 선명도 효과를 적용할 범위를 고릅니다. **중앙만**(기본값)은 각 눈의 중앙에 적용하고, 원래 흐린 렌즈 가장자리로 갈수록 약해집니다. **화면 전체**는 화면 전체에 적용하고, 선명도 비용이 약 2배가 됩니다(프레임당 0.4 ms 대신 약 0.8 ms, [성능](#성능) 참고). 이 설정은 프리셋에 포함되지 않습니다.

**길게 눌러 비교**를 누르고 있으면 FrameColor의 Vulkan 보정(색과 선명도)이 없는 화면이 보입니다. 버튼을 놓으면 원래 설정으로 돌아갑니다. 패널 밝기는 설정한 그대로입니다.

버튼을 누르고 있을 수 없는 게임 안에서 비교하려면 **자동 비교**를 누르고 대시보드를 숨깁니다(패널의 X는 누르지 마세요. 패널이 닫히면서 자동 비교도 꺼집니다). 3초마다 보정하지 않은 화면과 설정한 화면이 번갈아 보이고(보정하지 않은 화면부터 시작, 버튼에 지금 보이는 화면이 표시됨), 60초가 지나면 자동 비교가 저절로 꺼집니다. 더 일찍 끝내려면 한 번 더 누릅니다. 자동 비교 중에는 자동 프리셋이 기다립니다.

**자동 프리셋**은 표시 모드나 앱이 바뀔 때 프리셋을 적용합니다. 버튼을 누를 때마다 끔, 표준, 비비드, 야간, 저장한 P1–P3 순서로 바뀝니다.

- **독립형**: PC 스트리밍을 하지 않는 동안 씁니다.
- **PC 스트리밍**: `vrlink`가 실행 중일 때(PC 스트리밍 중일 때) 씁니다.
- **이 앱**: 지금의 SteamVR 앱이 실행 중일 때 씁니다. 모드 규칙보다 우선합니다. 앱에 SteamVR 앱 키가 없으면 쓸 수 없습니다.

FrameColor는 1초마다 모드와 앱을 확인하고, 같은 변화를 두 번 연속 보면 프리셋을 바꿉니다. 새 규칙은 다음 전환 때 적용됩니다. 직접 바꾼 값은 모드나 앱이 다시 바뀔 때까지 유지됩니다. 모든 규칙에서 벗어나면(예를 들어 앱을 종료했는데 모드 규칙이 끔이면) 첫 자동 프리셋을 적용하기 전의 설정으로 되돌립니다. 규칙은 `~/.config/framecolor/auto.txt`에 저장됩니다.

SSH에서 다음 명령도 쓸 수 있습니다.

```sh
~/.local/bin/framecolorctl status
~/.local/bin/framecolorctl doctor
~/.local/bin/framecolorctl bench [초]   # 기본 8초, 1–60초. 성능 참고
~/.local/bin/framecolorctl bench --compare A B [쌍 수]   # 선명도 A%와 B%(0–100 정수)를 번갈아 비교. 쌍 수 기본 12, 5–60. 성능 참고
~/.local/bin/framecolorctl disable
~/.local/bin/framecolorctl enable
~/.local/bin/framecolorctl language    # 지금 설정을 보여 줌. en, ja, ko, auto를 붙이면 변경
```

`disable`은 Vulkan 보정과 대시보드를 함께 끄고, `enable`은 둘 다 다시 켭니다. 두 명령 모두 SteamVR을 다시 시작합니다. `disable`로는 저장한 조정값과 프리셋이 지워지지 않습니다. 화면이 잘 보이지 않게 되면 SSH에서 `disable`을 실행합니다.

SteamVR이 시작 후 10초 안에 실패해서 꺼지면, 시작 보호 장치(guard)가 기본적으로 한 번 실패한 뒤 Vulkan 레이어를 끕니다. 실패 원인이 FrameColor와 관계없을 수도 있습니다. 연속 세 번 실패한 뒤에 끄게 하려면 `framecolorctl guard 3`, 기본값으로 되돌리려면 `framecolorctl guard 1`을 실행합니다.

보호 장치가 모든 실패에서 자동 복구를 보장하지는 않습니다. SSH로 직접 복구할 방법을 확보해 두세요.

## 성능

`framecolorctl bench [초]`는 FrameColor가 동작하는 동안 다음을 보여 줍니다.

- 프레임당 `vrcompositor`의 GPU 시간과 GPU 사용률. `/proc/<pid>/fdinfo/*`의 `drm-engine-gpu` 값(Steam Frame의 `msm` 드라이버)에서 읽고, FrameColor의 처리가 들어 있습니다.
- SteamVR의 프레임 타이밍: 프레임 수, 컴포지터 CPU·GPU 시간, 드롭·미스프레젠트·리프로젝션된 프레임. SteamVR의 컴포지터 GPU 시간에는 FrameColor의 처리가 **들어 있지 않습니다**.
- 메모리: `vrcompositor`와 대시보드의 RSS, `vrcompositor`의 GPU 메모리.

재는 동안 화면이 그려지지 않았으면(예를 들어 헤드셋을 벗었을 때) `vrcompositor`의 GPU 시간 대신 그렇다고 알려 줍니다.

백그라운드 앱으로 연결하기 때문에 SteamVR을 시작하지 않고, 명령을 쓰지 않으면 아무것도 실행되지 않습니다. 대시보드를 닫고 같은 장면에서 측정하고, 드롭과 리프로젝션된 프레임부터 봅니다. 헤드셋에서 실제로 느껴지는 것이 이 값입니다.

다른 VR 오버레이가 있으면 결과가 크게 달라집니다. SteamVR로 3D 화면을 띄우는 브라우저 확장이 켜져 있을 때 `vrcompositor`의 GPU 시간이 프레임당 약 0.5–1 ms 늘었습니다. 이런 오버레이는 먼저 닫으세요. 장면도 조금씩 바뀝니다. 두 설정을 비교할 때는 설정마다 길게 한 번씩 재지 말고, 몇 초마다 여러 번 번갈아 바꾸면서 붙어 있는 구간끼리 비교합니다. `framecolorctl bench --compare A B [쌍 수]`가 두 선명도 값 A와 B(0–100 정수 퍼센트)에 대해 이렇게 잽니다. 두 값을 번갈아 한 번에 2초씩 재며 정한 쌍 수만큼(한 쌍 = 두 값을 한 번씩, 기본 12쌍, 약 1분) 반복하고, 값마다 `vrcompositor`의 프레임당 GPU 시간과 차이(95% 구간)를 보여 줍니다. 그다음 설정을 원래대로 되돌립니다. Ctrl+C로 일찍 멈춰도 되돌립니다. 화면이 그려지지 않으면(예를 들어 헤드셋을 쓰지 않았을 때) 시작하지 않거나 도중에 멈추고, 길게 눌러 비교나 자동 비교가 켜져 있으면 시작하지 않습니다. 재는 동안 대시보드를 건드리지 마세요. 재는 중에 다른 프로그램이 설정을 바꾸면 멈추고 그 내용을 그대로 둡니다. 비정상적으로 끝나더라도 재기 전 설정은 `~/.config/framecolor/current.txt.bench-backup`에 남고, 다음 실행 때 되돌리는 방법을 알려 줍니다.

FrameColor는 `vrcompositor`가 렌즈 왜곡 보정까지 끝낸 최종 화면에 적용됩니다. 크기는 4320×2160으로, 2160×2160인 두 눈을 나란히 붙인 화면입니다. 그래서 선명도를 각 눈의 중앙에만 적용할 수 있습니다.

2026-10-28 측정 결과입니다(SteamOS 0.4.5, BUILD_ID 20261022.6150929, SteamVR 1791331621, 레이어 릴리스 빌드). 장면은 다른 오버레이 없는 Beat Saber 메뉴(120 Hz)입니다. 선명도 비용은 2초마다 선명도를 바꿔 가며 한 번에 12쌍씩 쟀습니다(±는 표준 오차의 2배).

| 항목 | 프레임당 vrcompositor GPU 시간 |
| --- | --- |
| FrameColor 끔 | 1.59–1.61 ms(사용률 19%) |
| FrameColor 켬, 선명도 0% | 1.7–2.1 ms, 약 0.1–0.5 ms 증가(추정. SteamVR 재시작 앞뒤로 따로 잰 값) |
| 선명도, 중앙만, 0% → 60% | +0.42 ms(±0.10) |
| 선명도, 중앙만, 0% → 100% | 두 번 재서 +0.38 / +0.43 ms(±0.06 / ±0.07) |
| 선명도, 전체, 0% → 60% | +0.82 ms(±0.18) |
| 선명도, 전체, 0% → 100% | +0.78 ms(±0.05) |

전체 행은 0.1.0-sharp.1에서 쟀습니다. 이 버전은 같은 셰이더 코드로 항상 화면 전체에 선명도를 적용했습니다. 선명도를 켜도 드롭된 프레임은 늘지 않았습니다. 0%보다 크면 CAS를 최대 강도로 계산한 뒤 섞기 때문에, 값과 관계없이 비용이 거의 같습니다. FrameColor를 켜면 `vrcompositor`의 GPU 메모리가 약 85 MiB 늘었습니다(800 MiB → 885 MiB). 4320×2160 화면 이미지(장당 약 36 MiB) 2~3장 크기입니다. 대시보드는 메모리(RSS)를 약 50 MiB 썼습니다.

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

`tests/offline/harness.cpp`는 설정 파일, 선명도 범위·자동 비교 버튼, 자동 프리셋, 언어 감지, `bench` 통계, DRM fdinfo 읽기를 검사하고, 대시보드를 영어·일본어·한국어로 그려 `panel_*.rgba`(1200×920 RGBA)로 저장합니다. PC에서는 `tests/offline/docker_check.sh`가 헤드셋이 필요 없는 검사를 Docker에서 모두 빌드하고 실행합니다. Steam Frame에서 하네스를 실행하려면 실제 OpenVR 라이브러리와 링크합니다(정의되지 않은 심볼을 무시하는 방식은 ARM64에서 실패합니다).

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
