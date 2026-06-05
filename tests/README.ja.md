# tests

English: [README.md](README.md)

`host-arduino-core` のローカルテスト一式です。
[`pytest-embedded-arduino-cli`](https://pypi.org/project/pytest-embedded-arduino-cli/)
の上に作られています。テストは分野ごとにグルーピングされています：

```
tests/
  runtime/   # host 専用 — Arduino ランタイム基本機能 (smoke, timing, print_api)
  storage/   # host 専用 — LittleFS / SPIFFS / FFat / SD ファサード (fs)
  network/   # host 専用 — IPAddress / UDP / TCP / TLS / HTTPClient
  interop/   # host + ESP32 実機 — 実装間互換性の検証 (smoke, wifi_connect, https_get, ...)
```

各リーフディレクトリが 1 つのテスト対象（スケッチ + `sketch.yaml` +
`test_*.py`）です。新しい分野を増やすときはトップレベルに新しいディレク
トリを作ってください。

## テスト方針

### host テスト (`runtime/` / `storage/` / `network/`)

- **検証目的**: host ランタイムそのものの正しさ。socket バックエンド、Stream
  挙動、`HostDiag` ヒント、`HTTPClient` のパース／状態遷移など、host で完結
  する挙動を全部詰め込む
- **実行頻度**: コアを編集したら**毎回**回す。コードレビュー前 / コミット前 /
  CI に上げる前のチェックリストに入る
- **実機依存なし**: ローカル PC 上で完結。外部ネットワーク／実機 USB 接続不要。
  数分で全 suite が走る
- **網羅性優先**: edge case や hint メッセージ、エラーパスまで含めて細かく検証
  していい。host で書きやすいテストは host に書く

### interop テスト (`interop/`)

- **検証目的**: host と実 ESP32 silicon の**挙動同値性**。host で動いたコード
  が実機で同じ振る舞いをすることを保証する。気になる領域は TLS ハンドシェイ
  ク、HTTPClient のリダイレクト追従、chunked decode 等、両ランタイム間で乖
  離が起きやすい高レベル機能
- **実行頻度**: 該当機能を**触ったときだけ手動で**実行。日常開発で常時回すコ
  ストは見合わない（ESP32 実機 + WiFi 接続 + 外部サービス到達が必要）
- **手動実行 trigger の目安**:
  - `WiFiClient` / `WiFiClientSecure` / `HTTPClient` / `WiFiUDP` 系のコードを変更
  - 新しい `tls` メニュー option やバックエンド分岐を追加
  - HTTPClient のリダイレクト・chunked・header 処理ロジックを変更
  - 外部 API 連携を新しく追加するスケッチを書く前後
- **`#ifdef ARDUINO_ARCH_HOST` 等の分岐は禁止**: sketch は両プロファイルで**同一
  ソース**から動くこと。同値性の検証はソース自体が裏付け
- **観測可能な挙動だけ assert する**: 内部実装は host (OpenSSL) と ESP32 (mbedTLS)
  で全く違うので、cipher 名、内部バッファサイズ、メモリレイアウトなどは検証
  対象外。Arduino API の表面上の動作（status code、body bytes、Serial への
  出力順序）だけが検証範囲

### 二段構えの原則（host 先 → interop 追加）

新機能やバグ修正をテストする順序は **host 専用カテゴリ (`runtime/` /
`storage/` / `network/`) でまずカバーする → 実機での検証が必要そうなら
`interop/` にも追加する** の 2 段階。

- **interop テストは host テストを置き換えない**。`interop/` に書いた
  からといって host 側を省略してはいけない
- **内容が重複してよい**。host 側はローカル fixture を相手に edge case や
  内部挙動まで網羅、interop 側は同じ機能を実機 + 外部サービスで表面同値性
  だけ確認、という役割分担。同じコードパスを 2 経路で叩くこと自体が価値
- **host 側で再現できる検証は host 側だけで足りる**。実機を相手にしないと
  意味がない（TLS handshake、リダイレクト追従、chunked decode 等の host vs
  ESP32 差が出やすい挙動）場合だけ interop を追加

理由: host 側に対応テストがないと、毎コミット自動回帰で何も守られない
領域ができる。interop だけで気付ける問題は基本的に「host で動くのに実機で
壊れる」差分だけで、それ以外の通常バグは host 側で検出するのが効率的。

## interop テストを追加する基準

新しい interop テストを書くべきか迷ったら以下に照らしてください：

1. **host と実機で差が出やすい領域か?** — TLS（OpenSSL vs mbedTLS）、HTTP
   redirect / chunked、WiFi 接続フロー、`Client::connected()` の semantics
   など、ABI／プロトコル境界に近いほど候補
2. **「host で動くのに実機で壊れる」が現実的に起こりうるか?** — 単純な
   `String::concat` の動作などは Arduino spec で決まっていて差が出にくいので
   interop 不要。逆に「host が ESP32 より高機能」な状態（例: 以前 host だけ
   redirect 跨ぎヘッダ保持していた件）は interop が拾う
3. **テストの形が host と実機で同一に書けるか?** — `dut.write` / `dut.expect`
   で完結し、外部サービス（httpbin.org 等）か Serial 経由の往復だけで結果が出
   せる構造であること。実機側で複雑なセットアップを要求する場合は別の方法
   （手動の動作確認等）を検討
4. **新規 / 変更したコアコードと観点が結びつくか?** — 「念のため」より、具体
   的な「この変更が壊しうる挙動」を 1 観点に絞ったテストを書く
5. **外部依存の脆弱性が許容範囲か?** — httpbin.org / badssl.com 等への到達は
   flaky になりうる。代替できる場合（ローカルサーバで足りる等）は host
   `network/` 側に書く。実機を相手にしないと意味がない場合のみ interop

## セットアップ

テスト依存をインストール：

```bash
cd tests
uv sync          # もしくは: pip install -e .
```

`conftest.py` の session-scoped fixture が、セッション開始時にこのリポジ
トリを `<sketchbook>/hardware/lang-ship/host` へ自動で symlink し、終了時
に外します。これによりテスト実行中だけ `lang-ship:host:host` がローカル
ワーキングツリーに解決されます。

すでに `<sketchbook>/hardware/lang-ship/host` が存在し、別の場所を指して
いる場合（例: 手動インストール）は fixture が即座に fail します。誤って
上書きしないための保護なので、事前に削除するか張り直してから実行してくだ
さい。

> **対応プラットフォーム**: テストスイートは **Linux / macOS のみ**。
> conftest が symlink を貼るが、Windows では Developer Mode か
> 管理者権限が無いと作成できず、通常 Windows 環境では session 開始時に
> `OSError` で fixture が落ちる。host-arduino-core 本体は Windows でも
> ビルド・実行可能だが、**pytest ハーネスは Windows 非対応**。

### `.env` の用意（interop のみ）

`interop/` の WiFi 接続を伴うテストは、`.env` から SSID / パスワードを
build-time 注入します（[.env.example](.env.example) を参考に
`tests/.env` を作成）：

```
TEST_SERIAL_PORT_ESP32=/dev/ttyUSB0
TEST_WIFI_SSID=YourSSID
TEST_WIFI_PASSWORD=YourPassword
```

`build_config.toml` が各 interop テストディレクトリにあり、
`TEST_WIFI_SSID` → `-DWIFI_SSID="..."` のマッピングを宣言しています。

## 実行

### host 専用テスト（日常）

```bash
cd tests
uv run pytest -v
```

すべて host で完結。WiFi 接続も外部サービス到達も不要。`interop/` は
`pyproject.toml` の `addopts = "--ignore=interop"` で default 探索か
ら除外しています（blacklist 方式）。`pytest interop/` のような明示
指定は引き続き動作します — `--ignore` は再帰探索を抑制するだけで、
コマンドラインに渡されたパスは尊重されます。将来トップレベルカテゴリ
が増えても自動で収集対象に入り、手動 / ハードウェア依存のものだけ
ここに追記すればよい構造です。

### interop テスト（host プロファイル）

```bash
cd tests
uv run --env-file .env pytest -v interop/
```

`.env` をロードしないと WiFi クレデンシャル不在で fail します。host
プロファイル上で interop sketch を回し、最低限「両プロファイルでビルド
が通る + Arduino API の同じソースが host で動く」ことを保証します。

### interop テスト（ESP32 実機）

```bash
cd tests
uv run --env-file .env pytest --profile esp32 interop/
```

`--profile esp32` を渡すと `sketch.yaml` の `esp32` プロファイルが選択さ
れ、`esp32:esp32:esp32` (3.3.8) でビルドして USB 経由で書き込み、Serial
で結果を受け取ります。**該当機能を触ったときだけ手動で**実行する想定で
す。

`runtime/`、`storage/`、`network/` は `esp32` プロファイルを持っていな
いので `--profile esp32` 指定では収集対象外（pytest-embedded が
profile-mismatch を skip 扱いにします）。

## ローカル版を使う仕組み

host profile の `sketch.yaml` では、意図的に `platforms:` ブロックを
書きません：

```yaml
profiles:
  host:
    fqbn: lang-ship:host:host
    port: socket://localhost
```

`tests/conftest.py` の session fixture が、テスト開始前にこの working tree
を `<sketchbook>/hardware/lang-ship/host` として登録します。FQBN だけなら、
arduino-cli はそのローカル hardware entry から `lang-ship:host` を解決し、
公開済みパッケージをインストール・更新しに行きません。コアを編集して
`pytest` を再実行すれば反映されます。リリース不要です。

逆に *リリース済み* のバージョンを検証したい場合は、`platforms:` ブロッ
クにバージョンと index URL を書きます：

```yaml
platforms:
  - platform: lang-ship:host (1.0.5)
    platform_index_url: https://tanakamasayuki.github.io/host-arduino-core/package_index.json
```

`esp32` プロファイルは逆で、Boards Manager 経由でインストールされる
パッケージを直接使う想定なのでバージョン pin と `platform_index_url` を
明示しています：

```yaml
esp32:
  fqbn: esp32:esp32:esp32
  platforms:
    - platform: esp32:esp32 (3.3.8)
      platform_index_url: https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

## ファイル I/O の慣習

## host ランタイムの起動順

host プロファイルのテストでは、ランタイムは TCP 経由の `Serial` 接続情報を
先に公開し、pytest の `dut` クライアントが接続してから sketch の `setup()` に
入る。したがって `TEST start ...` のような最初の行は、接続前 `Serial`
バッファに依存せずに期待できる。

host テストで最初の期待行より前に timeout した場合は、artifact の
`*.host-arduino.log` を確認する。`runtime_client_ready` があれば pytest は接続済みで
sketch の開始も許可されている。これが無い場合は launcher または TCP 接続段階の
問題として見る。

スケッチが fixture ファイルを読んだり生成物を書き出したりする場合の
推奨ルール：

- **実行時のカレントディレクトリは sketch ディレクトリ**（`.ino` が
  ある場所）。pytest-embedded がそこからバイナリを起動するので、
  `fopen("foo", ...)` のような相対パスはそのまま sketch ディレクトリ
  基準で解決される。
- **入力は `<sketch_dir>/input/` 配下**に置く。fixture ファイルは
  ここに git 管理で置き、スケッチからは `fopen("input/foo", "rb")` で
  読む。
- **出力は `<sketch_dir>/output/` 配下**に書く。スケッチ側は書き出し
  前に `mkdir("output", 0755)`（EEXIST は無視）で作ってから
  `fopen("output/<name>", "wb")` で書く。`output/` はプロジェクトの
  `.gitignore` に入れて生成物が commit に混入しないようにする。
- **各テスト実行前に `output/` を削除する**。前回の残骸ファイルが
  残っていると、スケッチが失敗していてもテストが偶然パスしてしまう。
  このリポジトリでは `tests/conftest.py` の `pytest_runtest_setup`
  フックがテスト前に `<sketch_dir>/output/` を丸ごと削除する形で
  実装している。下流プロジェクトに同等の conftest がない場合は、
  この実装を参考にコピーするか、使っている fixture / セットアップ
  機構で同じ事前クリーンアップを行う。
  **注意**: `tests/conftest.py` には **session-scoped な symlink
  fixture** も同居していて、これは host-arduino-core 固有の
  「ワーキングツリーに対して開発したい」事情用の仕掛け。コピーする
  なら **`pytest_runtest_setup` の output 削除部分だけ** にして、
  symlink fixture をそのまま持っていかないこと。symlink fixture は
  sketchbook 配下にディレクトリを作る上に、セッションが異常終了すると
  シンボリックリンクが残留する危険がある。
- テストはこの `<sketch_dir>/output/<name>` を assert する。

これで「ソース（`.ino` / `sketch.yaml` / `test_*.py`）」と「生成物」が
見た目で分離され、毎回クリーンな状態からテストが走る。

`tests/graphics/lgfx_smoke` がリファレンス実装で、`gfx.createPng()` の
出力を `output/lgfx_smoke_capture.png` に書き、テスト側でその存在と
サイズを assert している。

## `tests/common_libs/` で sketch 間の C++ コードを共有する

複数 sketch から共有したい C++ コード（描画ヘルパ、モック等）は
`tests/common_libs/<libname>/` に Arduino library 形式（`library.properties`
+ `src/`）で置く。sketch 側は `sketch.yaml` から参照する:

```yaml
profiles:
  host:
    libraries:
      - dir: ../../common_libs/<libname>
```

`tests/common_libs/gfx_demo/` が参考実装。`drawHome` 等の関数を 1 箇所で
書き、3 つの graphics smoke（`lovyangfx_smoke` / `m5gfx_smoke` /
`m5unified_smoke`）から同じ関数を呼ぶ。各 sketch は `LGFX_Sprite` を
複数解像度 / 回転 / 色深度で生成し同じ関数に通すので、`drawHome` の
回帰は 3 entry point 全てから即検出できる。

### 複数 upstream gfx ライブラリに対応するライブラリの書き方

`gfx_demo.cpp` は `M5Unified.h` / `M5GFX.h` / `LovyanGFX.hpp` のどれが
利用可能かを `__has_include` で検出して include する。**この 3 分岐は
テスト都合のパターン**で、本番コードは利用 lib を 1 つに固定して直に
`#include <M5GFX.h>` 等と書くのが普通。`sketch.yaml` 経由なら宣言済み
lib のみが include path に通るので `__has_include` は決定的に動くが、
Arduino IDE のように複数 lib がグローバル install されてる環境では順序
依存で誤検出しうる。

`gfx_demo.h` は「中立」に保ち、自前で `__has_include` せず宣言だけ
（`void drawHome(LovyanGFX &)`）置く。呼び出し側が gfx ライブラリを
先に include して `LovyanGFX` 型をスコープに入れる前提。これで 3 分岐
を `.cpp` 1 箇所に閉じ込めている。

## 新しいテストを追加する

### host 専用テスト

```
tests/<category>/<name>/
  <name>.ino
  sketch.yaml      # 既存の host 専用テストの sketch.yaml をコピー
  test_<name>.py   # `dut` fixture を利用
```

`.ino` のファイル名はディレクトリ名と一致させる必要があります（arduino-cli
の規約）。

### interop テスト

```
tests/interop/<name>/
  <name>.ino           # 両プロファイルで動く Arduino スケッチ
  sketch.yaml          # host + esp32 両プロファイル
  build_config.toml    # 必要なら env → -D 注入（WIFI_SSID 等）
  test_interop_<name>.py
```

- `test_*.py` の basename は他のテストと衝突させない (`test_smoke.py` が複数
  ディレクトリにあると pytest がモジュール名重複でエラーになるので
  `test_interop_<name>.py` の形にする)
- sketch は `Serial.begin(115200); delay(5000);` で必ず settle を入れる
  （ESP32 の USB-Serial bridge 対応 + host では `sleep_for` の薄いラッパなので
  無害）
- `#ifdef ARDUINO_ARCH_HOST` 等で host/ESP32 のコードパスを分けない（方針）
- 同値性の assertion は「Arduino API の表面挙動」だけに留め、内部実装の
  cipher 名やメモリレイアウトには依存しない
