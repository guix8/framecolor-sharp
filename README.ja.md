# FrameColor

[English](README.md) | [한국어](README.ko.md)

> このリポジトリは [beko-kerbecotton/framecolor](https://github.com/beko-kerbecotton/framecolor) のフォークです．フォークで追加した機能（シャープネス（範囲は中央のみ・全体），比較ボタンと自動比較，自動プリセット，`framecolorctl bench`，韓国語と言語の自動選択）とソースからのビルド方法は [英語のREADME](README.md#about-this-fork) を参照してください．このフォークのリリースアーカイブはこのリポジトリのReleasesページにあり，下の「インストール」と同じ手順でインストールできます（`VERSION` は `0.1.0-sharp.2` などです）．

## 概要

Steam Frameの表示色を，ヘッドセット内のSteamVRダッシュボードから調整するツールです．パネル輝度に加え，明るさ・彩度・コントラスト・ガンマを全体または赤・緑・青のチャンネルごとに調整でき，シャープネスも加えられます．標準・鮮やか・夜間のプリセットと，保存できる3つのプリセット枠があります．

Steam Frame専用です．色補正はSteamVRの `vrcompositor` に読み込ませるVulkanレイヤーで行います．ローカル実行とストリーミングの映像に加え，ダッシュボードなどの表示にも影響します．

## 必要要件

- Steam Frame（SteamOS，ARM64）と，標準の `/opt/steamvr` にあるSteamVR
- 開発者モードとSSH接続．アーカイブをSteam Frameに転送してコマンドを実行できること
- Steam Frame側のPython 3.10以降（インストール時のLUT生成に使用）

配布アーカイブにはSteam Frame用のビルド済みバイナリが含まれます．ソースからビルドする場合に必要なツールは[開発](#開発)を参照してください．

## インストール

GitHub Releasesから `framecolor-VERSION-steamframe-aarch64.tar.gz` を入手してください．チェックサムを確認する場合は，同名の `.sha256` ファイルも入手します．以下の `VERSION` と `FRAME_IP` は，ダウンロードした版とSteam FrameのIPアドレスに置き換えます．

PCからSteam Frameのホームディレクトリに転送します．

```sh
scp framecolor-VERSION-steamframe-aarch64.tar.gz steamos@FRAME_IP:
```

チェックサムを確認する場合のみ，`.sha256` ファイルも転送します．

```sh
scp framecolor-VERSION-steamframe-aarch64.tar.gz.sha256 steamos@FRAME_IP:
```

SSHで接続します．

```sh
ssh steamos@FRAME_IP
```

以降はSteam Frame上で実行します．`.sha256` ファイルを転送した場合は，展開前に次の確認を行います．

```sh
sha256sum -c framecolor-VERSION-steamframe-aarch64.tar.gz.sha256
```

その後，展開してインストールします．

```sh
tar -xzf framecolor-VERSION-steamframe-aarch64.tar.gz
cd framecolor
sh contrib/install.sh
~/.local/bin/framecolorctl enable
```

インストールは通常ユーザーで実行し，`sudo` は付けないでください．新規インストール直後はダッシュボードは動きますが，Vulkan補正が無効のため色の調整項目は使えません．最後の `enable` で補正を有効にし，SteamVRを再起動します．インストール先はユーザー領域の `~/.local` と `~/.config` のみです．

更新するときは，新しいアーカイブを取得して同じ手順で `sh contrib/install.sh` を実行します．保存済みの調整値とプリセットは保持されます．補正がすでに有効なら，インストーラーがSteamVRを再起動します．

完全に削除するには，展開済みのアーカイブのディレクトリから次を実行します．

```sh
sh contrib/uninstall.sh
```

アンインストールでは保存した調整値・プリセットも削除します．SteamVR側に保存されたパネル輝度は残る場合があります．必要なら，削除前にダッシュボードの「リセット」で標準値に戻してください．

## 設定

SteamVRダッシュボードで「ディスプレイ」（RGBアイコン）を開きます．パネルの見出しは「ディスプレイ色調整」です．
表示言語は英語，日本語，韓国語です．初期設定ではSteamクライアントの言語に合わせ，次にシステムのロケールを見て，どちらも対応していなければ英語になります．パネル右上の言語ドロップダウンメニューで言語を選ぶか，「自動」でSteamの言語に戻せます．`framecolorctl language en|ja|ko|auto` でも変更できます．このコマンドでの変更は，次にダッシュボードが起動したとき（SteamVRの再起動後，または `systemctl --user restart framecolor.service` の後）に反映されます．

| 項目 | 範囲 | 適用先 |
| --- | --- | --- |
| パネル輝度 | 40〜125% | SteamVRのパネル輝度設定．チャンネル共通 |
| 明るさ | 50〜150% | Vulkan補正．全体またはRGB別 |
| 彩度 | 0〜200% | Vulkan補正．全体またはRGB別．100%が標準 |
| コントラスト | 0〜100% | Vulkan補正．全体またはRGB別．0%が標準 |
| ガンマ | 0.50〜2.50 | Vulkan補正．全体またはRGB別．1.00が標準 |
| シャープネス | 0〜100% | Vulkan補正（AMD CAS）．初期設定は各目の中央（全体にも変更可）．チャンネル共通．0%で無効 |

「調整チャンネル」で全体・赤・緑・青を選びます．全体を動かすと，各チャンネルの現在値からまとめて変化します．パネル輝度とシャープネスはチャンネル選択に関係しません．「保存」を押してからP1〜P3を選ぶと現在の設定を保存できます．「リセット」は標準プリセットを適用します．

シャープネスの横のボタンで，シャープ化をかける範囲を選びます．「中央のみ」（初期設定）は各目の中央にかけ，もともとぼやけているレンズの端に向かって弱めます．「画面全体」は画面全体にかけ，シャープ化の負荷が約2倍になります（1フレームあたり約0.4 msではなく約0.8 ms．[性能](#性能)参照）．この設定はプリセットに含まれません．

「長押しで比較」を押している間は，FrameColorのVulkan補正（色とシャープネス）をかけない画面を表示します．離すと設定に戻ります．パネル輝度は設定したままです．

ボタンを押し続けられないゲームの中で比べるときは，「自動比較」を押してダッシュボードを隠します（パネルの×は使わないでください．パネルが閉じて自動比較も止まります）．3秒ごとに補正なしの画面と設定した画面が切り替わり（補正なしから始まり，ボタンに今の表示が出ます），60秒たつと自動比較は自動でオフになります．早く止めるときはもう一度押します．自動比較の間，自動プリセットは待機します．補正なしの画面を表示している間にダッシュボードが応答しなくなっても，5秒以内に設定した画面に戻ります．

「自動プリセット」は，表示モードやアプリが変わったときにプリセットを適用します．ボタンを押すたびに，オフ・標準・鮮やか・夜間・保存済みのP1〜P3を順に切り替えます．

- 単体：PCストリーミングをしていないときに使います．
- PCストリーミング：`vrlink` が動いている（PCから配信している）ときに使います．
- このアプリ：現在のSteamVRシーンアプリの実行中に使います．モードの規則より優先します．SteamVRのアプリキーがないアプリでは使えません．

モードとアプリは1秒ごとに確認し，同じ変化を2回続けて確認すると切り替えます．新しい規則は次の切り替えから適用します．手動で変えた値は，次にモードかアプリが変わるまで保たれます．どの規則にも当てはまらなくなると（例：アプリを終了し，モードの規則がオフ），最初の自動プリセット前の設定に戻します．規則は `~/.config/framecolor/auto.txt` に保存します．

SSHからは次のコマンドを使えます．

```sh
~/.local/bin/framecolorctl status
~/.local/bin/framecolorctl doctor
~/.local/bin/framecolorctl bench [秒数]   # 初期値8，1〜60．「性能」参照
~/.local/bin/framecolorctl disable
~/.local/bin/framecolorctl enable
~/.local/bin/framecolorctl language      # 現在の設定を表示．en，ja，ko，autoを付けると変更
```

`disable` はVulkan補正とダッシュボードを無効にし，`enable` は両方を有効にします．どちらもSteamVRを再起動します．`disable` では保存済みの調整値とプリセットは削除されません．画面が見づらくなった場合は，SSHから `disable` を実行してください．

SteamVRが起動開始から10秒未満で異常終了した場合，起動ガードは標準で1回の失敗後にVulkanレイヤーを無効化します．原因がFrameColorとは限りません．3回連続で失敗したときだけ無効化するには `framecolorctl guard 3`，初期設定に戻すには `framecolorctl guard 1` を実行します．

ガードはすべての障害からの自動復旧を保証するものではありません．SSHからの手動復旧手段も確保してください．

## 性能

`framecolorctl bench [秒数]` は，FrameColorの動作中に次を表示します．

- `vrcompositor` の1フレームあたりGPU時間とGPU使用率．`/proc/<pid>/fdinfo/*` の `drm-engine-gpu`（Steam Frameの `msm` ドライバー）から読み，FrameColorの処理を含みます．
- SteamVRのフレームタイミング：フレーム数，コンポジターのCPU・GPU時間，ドロップ，ミスプレゼント，リプロジェクション．SteamVRのコンポジターGPU時間にはFrameColorの処理が**含まれません**．
- メモリ：`vrcompositor` とダッシュボードのRSS，`vrcompositor` のGPUメモリ．

バックグラウンドアプリとして接続するため，SteamVRを起動しません．コマンドを使わない限り何も動きません．ダッシュボードを閉じ，同じ場面で測定し，まずドロップとリプロジェクションの数を見ます．

ほかのVRオーバーレイがあると結果が大きく変わります．SteamVRで3D表示を出すブラウザー拡張が動いているとき，`vrcompositor` のGPU時間は1フレームあたり約0.5〜1 ms増えました．このようなオーバーレイは先に閉じてください．場面も少しずつ変わるため，2つの設定を比べるときは，それぞれを長く1回ずつ測るのではなく，数秒ごとに何度も切り替えて隣り合う区間どうしを比べます．このように測るスクリプトは，まだリポジトリにありません．

FrameColorは，`vrcompositor` がレンズの歪み補正まで終えた最終画面にかかります．大きさは4320×2160で，2160×2160の両目を横に並べた画面です．そのため，シャープ化を各目の中央だけにかけられます．

2026年10月28日の結果（SteamOS 0.4.5，BUILD_ID 20261022.6150929，SteamVR 1791331621，レイヤーはリリースビルド）．場面はほかのオーバーレイのないBeat Saberのメニュー（120 Hz）です．シャープ化の負荷は2秒ごとにシャープネスを切り替え，1回に12組ずつ測りました（±は標準誤差の2倍）．

| 項目 | vrcompositorの1フレームあたりGPU時間 |
| --- | --- |
| FrameColor無効 | 1.59〜1.61 ms（使用率19%） |
| FrameColor有効，シャープネス0% | 1.7〜2.1 ms，約0.1〜0.5 ms増（推定．SteamVR再起動の前後で別々に測った値） |
| シャープネス，中央のみ，0% → 60% | +0.42 ms（±0.10） |
| シャープネス，中央のみ，0% → 100% | 2回測って +0.38 / +0.43 ms（±0.06 / ±0.07） |
| シャープネス，全体，0% → 60% | +0.82 ms（±0.18） |
| シャープネス，全体，0% → 100% | +0.78 ms（±0.05） |

全体の行は0.1.0-sharp.1で測りました．この版は同じシェーダーコードで常に画面全体にシャープ化をかけていました．シャープ化でドロップは増えませんでした．0%より大きいとCASを最大強度で計算して混ぜるため，負荷は値にほとんどよりません．FrameColorを有効にすると `vrcompositor` のGPUメモリが約85 MiB増えました（800 MiB → 885 MiB）．4320×2160の画面イメージ（1枚約36 MiB）2〜3枚分の大きさです．ダッシュボードのメモリ（RSS）は約50 MiBでした．

## 免責

FrameColorは非公式ツールです．また，ベータ版です．SteamVRやSteamOSの更新後に動作しなくなる可能性があります．

色補正は最終表示にかかるため，ダッシュボードや安全境界の色も変わる可能性があります．強い補正で視認性を損なう設定は避け，周囲の安全を確認して使用してください．このツールは安全境界を無効化・変更するものではありません．

現在の構成は標準の `/opt/steamvr` のみを対象とします．問題が起きた場合は，まずSSHから `~/.local/bin/framecolorctl disable` を実行してください．パネル輝度などSteamVR自身が保持する設定は，補正を無効化しても残る場合があります．ソフトウェアの無保証条項は [LICENSE](LICENSE) を参照してください．

元のツールはCodexを，このフォークの変更はClaude Codeを利用して作成しました．Steam Frameで動作確認していますが，自己責任でお使いください．

## 開発

ソースからのビルドはSteam Frame上で行います．CMake，Meson，Ninja，pkg-config，`glslangValidator`，Cairo・FreeType・Vulkanの開発用ファイル，およびSteamVRに付属する `libopenvr_api.so` が必要です．配布アーカイブの作成には `strip`，`tar`，`sha256sum` も使います．

```sh
sh contrib/install.sh                 # ソースからビルドしてユーザー領域へインストール
sh tests/test_guard.sh
sh tests/test_framecolorctl.sh
python3 -m unittest discover -s tests -v
sh contrib/package.sh VERSION        # dist/ にアーカイブと.sha256を生成
```

`contrib/install.sh` はビルド済みバイナリが同梱されていなければソースからビルドします．`contrib/package.sh` はSteam Frame（ARM64）上で実行してください．生成した `build/` と `dist/` はGit管理の対象外です．

## 謝辞

ダッシュボード関連のコードとデザインは [frameeyeosc](https://github.com/sasaken1102r/frameeyeosc) を参考・改変しています．オーバーレイには [OpenVR](https://github.com/ValveSoftware/openvr) のヘッダーを使用し，表示色の補正には [vkBasalt](https://github.com/DadSchoorse/vkBasalt) の改変版を使用しています．作者と貢献者の皆さまに感謝します．

FrameColor独自部分は [MITライセンス](LICENSE) です．第三者由来コードの詳細とライセンス原文は [サードパーティーに関する表示](THIRD_PARTY_LICENSES.ja.md) を参照してください．
