# host-arduino-core（日本語）

日本語 | English: [README.md](README.md)

https://tanakamasayuki.github.io/lang-ship-arduino-core/package_lang-ship_index.json

Arduino スケッチをホスト PC 上の実行ファイルとしてビルドし、`pytest-embedded-arduino-cli` などの Arduino CLI 対応ホスト側テストから制御するための最小構成 Arduino Core + Boards Manager パッケージです。

これは実用的な Arduino 互換ボードではありません。実機、フラッシュ書き込み、シリアルポートに依存しない、純粋なスケッチ / ライブラリのロジック部分をテストするためのターゲットです。

## ハイライト

- アーキテクチャは `host`、FQBN は `lang-ship:host:host`。
- `cores/host` に最小限の `Arduino.h`、時刻 API、`Serial`、weak な Arduino スタイル `main()` を同梱。
- `arduino-cli compile` は、ホストにある `gcc` / `g++` 互換ツールチェーンで通常の実行ファイルを生成します。
- `arduino-cli upload` は、実機への書き込みではなく、生成した実行ファイルをバックグラウンドで起動します。
- `arduino-cli monitor` には対応しません。
- `Serial` は localhost の TCP 接続経由でテスト側と通信します。
- 起動時の接続情報は標準出力に出し、実行ファイル横の `<executable>.host-arduino.json` にも保存します。
- リリース ZIP と `package_index.json` は GitHub Pages で公開します:
  `https://tanakamasayuki.github.io/host-arduino-core/package_index.json`

## API サポート状況

ベースは ESP32 系ボードを想定していますが、可能な限り Arduino Core API に
準拠することを目標としており、どちらの API 面で書かれたスケッチもホスト上
で動かせることを目指します。「出自」列は、その API が Arduino Core 標準か
ESP32 拡張かを示します。

凡例:
- ✅ 実装済み・`tests/` でテスト済み
- 🟡 スタブのみ（コンパイル・リンクは通るが、中身は何もしない）
- 🔲 未実装・コントリビューション歓迎
- ⛔ 対象外（このコアでは実装しない）

### ランタイム / 言語

| API | 状況 | 出自 | 備考 |
|-----|------|------|------|
| `setup()` / `loop()` / weak `main` | ✅ | Arduino | `cores/host/main.cpp` |
| `millis` / `micros` | ✅ | Arduino | `std::chrono::steady_clock` |
| `delay` / `delayMicroseconds` | ✅ | Arduino | `std::this_thread::sleep_for` |
| `yield` | ✅ | Arduino | no-op |
| `min` / `max` / `constrain` / `map` / `abs` | ✅ | Arduino | ヘッダオンリー |
| `random` / `randomSeed` | ✅ | Arduino | `std::rand` のラッパ |
| `bit*` / `lowByte` / `highByte` / `_BV` | ✅ | Arduino | マクロ |
| `String`（`WString.h`） | ✅ | Arduino | |

### Print / Stream / Serial

| API | 状況 | 出自 | 備考 |
|-----|------|------|------|
| `Print`（int / hex / bin / float / String / bool） | ✅ | Arduino | Arduino のフォーマットに準拠 |
| `Printable` | ✅ | Arduino | |
| `Stream`（`timedRead` / `readBytes` / `setTimeout` / `find` / `parseInt`） | ✅ | Arduino | |
| `HardwareSerial` / `Serial` | ✅ | Arduino | localhost の TCP ソケット越しに公開 |
| `Serial1` / `Serial2` | 🔲 | ESP32 | 複数 UART を同時に使うスケッチが現れたら |

### ファイルシステム

| API | 状況 | 出自 | 備考 |
|-----|------|------|------|
| `FS` / `File`（read / write / seek / size / openNextFile） | ✅ | Arduino | `<cstdio>` ラッパ |
| `LittleFS` / `SPIFFS` / `FFat` / `SD` | ✅ | ESP32 | すべて実行ファイル隣のディレクトリにマップ。フラッシュ容量制限やフォーマット意味論は再現しない |
| `Preferences`（NVS） | ✅ | ESP32 | オンメモリのみ（詳細は ESP-IDF 節を参照） |
| `EEPROM` | 🔲 | Arduino | `Preferences` と同じ方針 |

### ネットワーク

| API | 状況 | 出自 | 備考 |
|-----|------|------|------|
| `IPAddress` | ✅ | Arduino | Arduino 互換の API 一式 |
| `UDP`（抽象） | ✅ | Arduino | `cores/host/Udp.h` |
| `WiFiUDP`（ユニキャスト + ブロードキャスト） | ✅ | ESP32 | POSIX / Winsock 実装。`SO_BROADCAST` を `begin()` で有効化。`lastError()` で errno 取得可。受信バッファ 65535 B。パケット操作前に `begin(0)` 必須（ESP32 より厳格）。誤用時は `Serial` に `[HostCore]` ヒントを出力（`cores/host/HostDiag.h` 参照） |
| `WiFiUDP::beginMulticast` | 🔲 | ESP32 | 優先度低。基底クラスのまま 0 を返す（参加していない）。host 上でのマルチキャストテストは Windows / WSL2 で安定させにくいため積極対応はしない |
| `WiFi` ファサード（`begin` / `disconnect` / `status` / `localIP` / `SSID` / `RSSI` / `mode`） | 🟡 | ESP32 | 状態追跡型のスタブ（`begin` → `WL_CONNECTED`、`disconnect` → `WL_DISCONNECTED`）。実アソシエーション・スキャンは無し |
| `WiFi.scanNetworks` / `scanComplete` | 🔲 | ESP32 | スタブで 0 返却が妥当 |
| `WiFi.softAP*`（実 AP 化） | 🟡 | ESP32 | `softAPIP()` のみ。実 AP 化は不可 |
| `Client` / `Server`（抽象） | ✅ | Arduino | `cores/host/Client.h`, `cores/host/Server.h` |
| `WiFiClient`（TCP） | ✅ | Arduino / ESP32 | POSIX / Winsock 実装。ノンブロッキング。`shared_ptr` で内部状態を共有するので `WiFiClient c = server.available();` が安全。`lastError()` で errno 取得可。誤用時は `HostDiag` 経由で `[HostCore]` ヒントを出力 |
| `WiFiServer`（TCP） | ✅ | Arduino / ESP32 | POSIX / Winsock 実装。`begin(port)` の `port=0` で OS にエフェメラルポートを割り当てさせ、`port()` で確認可能。`available()` / `accept()` はノンブロッキング |
| `WiFiClientSecure`（TLS） | ✅ | ESP32 | `cores/host/WiFiClientSecure.h` は `WiFiClient` を継承し、ボードメニュー `tls=openssl` 選択時に OpenSSL を使用（Linux `libssl-dev` 3.0.x と Windows MSYS2 UCRT64 `openssl` 3.5.2 で動作確認済）。`tls=disabled`（デフォルト）でもクラス自体はコンパイル可能で、`connect()` が 0 を返し `[HostCore]` ヒントを出力（ビルドは常に通る）。証明書検証は host では常時スキップ（実機テストの責務）。`setCACert` / `setCertificate` / `setPrivateKey` / `setInsecure` / `loadCACert(Stream&,size_t)` 等は no-op で受理。macOS は OpenSSL バックエンドの対象外（追加の `-I` / `-L` 解決が必要）。mbedTLS をソース同梱した別リポジトリ版バックエンドは引き続き将来計画（OS パッケージ依存を回避できる代替手段として） |
| `HTTPClient` | ✅ | ESP32 | `cores/host/HTTPClient.h`。`begin(url)` で `http://` は `WiFiClient`、`https://` は `WiFiClientSecure` を内部で自動選択（後者はボードメニュー `tls=openssl` が必要。未設定時は `begin("https://…")` が false を返し `[HostCore]` ヒントを出力）。サポート: `GET` / `POST` / `PUT` / `PATCH` / `sendRequest`、`addHeader`、`getString`（`Content-Length` と `Transfer-Encoding: chunked` の両方を decode）、`getStream` / `getStreamPtr`、`getSize`、`getLocation`、`setTimeout`、`setUserAgent`、`setAuthorization`。リダイレクト自動追従は `setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS / HTTPC_STRICT_FOLLOW_REDIRECTS / HTTPC_FORCE_FOLLOW_REDIRECTS)` + `setRedirectLimit(N)`（デフォルト無効・上限 10）で 301/302/303/307/308 に対応、絶対 URL とルート相対の `Location` の両方を解決、内部 client モードでは `http` ⇄ `https` の scheme 切替も透過、FORCE モードでは 301/302/303 の POST→GET ダウングレードを実施。**`addHeader` で追加した user header はリダイレクトを跨いで破棄される**（ESP32 Arduino HTTPClient v3.x と同じ挙動）。永続したい場合は DISABLE モードで 3xx を検出して手動で再リクエストする。接続戦略は毎リクエスト `Connection: close`（keep-alive 無し）。**未実装**: multipart、gzip、cookie、Basic 認証ヘルパ（`addHeader("Authorization", ...)` を直接使用） |
| `WebServer` / `AsyncWebServer` | ⛔ | ESP32 | 同上（`Server` の上の別ライブラリ） |
| `ESPmDNS` / `DNSServer` | 🔲 | ESP32 | 優先度低 |
| `Ping` / `NetworkInterface` | 🔲 | ESP32 | 優先度低 |
| Ethernet（`ETH`） | 🔲 | ESP32 | host では `WiFi` と区別する意味が薄い |

### ハードウェア I/O（意図的にスタブ化）

ハードウェアに依存する API は「実機向けスケッチが少なくともリンクできる」
ことを目的にスタブ化しています。ピンの状態が結果に影響するテストは、
スケッチ層でモックする想定です。

| API | 状況 | 出自 | 備考 |
|-----|------|------|------|
| `pinMode` / `digitalWrite` / `digitalRead` | 🟡 | Arduino | no-op、`digitalRead` は `0` を返す |
| `analogRead` / `analogWrite` / `analogReadResolution` / `analogSetAttenuation` | 🟡 | Arduino / ESP32 | no-op |
| `touchRead` / `touchAttachInterrupt` | 🟡 | ESP32 | `0` を返す |
| `tone` / `noTone` / `pulseIn` / `pulseInLong` | 🟡 | Arduino | no-op |
| `attachInterrupt` / `detachInterrupt` | 🟡 | Arduino | no-op |
| `dacWrite` / `ledcWrite` / `ledcAttach` / `ledcSetup` | 🔲 | ESP32 | no-op スタブで十分 |
| `Wire`（I²C） | 🔲 | Arduino | 「初期化成功・デバイス無し」スタブの方針 |
| `SPI` | 🔲 | Arduino | `Wire` と同じ方針 |
| `Servo` | 🔲 | Arduino | no-op スタブ |

### ESP-IDF / ベンダー拡張

| API | 状況 | 出自 | 備考 |
|-----|------|------|------|
| FreeRTOS（`xTaskCreate` / `vTaskDelay` / キュー / セマフォ / ミューテックス / Notify） | ✅ | ESP-IDF | `std::thread` / `std::mutex` / `std::condition_variable` でバック。優先度・コア固定・スタックサイズは引数として受けるが無視。`portENTER_CRITICAL` はグローバル recursive mutex に縮退。`vTaskDelete(NULL)` は終了フラグを立てて return するだけ — タスク関数側がフラグを観測して return する想定（別スレッドを移植性のある方法で強制終了できないため）。非ゴール: 優先度ベーススケジューリング、コア固定、`vTaskSuspend` の即時性、スタックオーバーフロー検出、`portENTER_CRITICAL` の割り込み禁止意味論。これらに依存するスケッチは host テスト向きではない |
| `esp_log` / `ESP_LOG*` / `log_e` / `log_i` / `log_w` / `log_d` / `log_v` マクロ | ✅ | ESP-IDF | host スタブのみ — `CORE_DEBUG_LEVEL` は `ARDUHAL_LOG_LEVEL_NONE` (0) 固定、全マクロは `(void)sizeof(...)` で引数を捨てる（出力なし・未使用変数警告なし）。`esp_log_level_set` は no-op、`esp_log_level_get` は常に `ESP_LOG_NONE`。理由: ログ出力が `dut.expect` ストリームに混ざるとテストの読みづらさにつながる + Arduino 流儀では診断用には `Serial.print` を使う |
| `esp_timer` | ✅ | ESP-IDF | `esp_timer_get_time()` は最初の呼び出しからの µs を返す。`create` / `start_once` / `start_periodic` / `stop` / `delete` / `is_active` の高レベル API は timer ごとに `std::thread` 1 本 + `condition_variable` でスケジューリング。リアルタイム保証なし |
| `esp_random` / `esp_fill_random` | ✅ | ESP-IDF | `std::random_device` で seed した `std::mt19937` でバック。暗号用途ではないが、Arduino スケッチで silicon の HW RNG が使われる用途（jitter / seed / バリエーション）には十分 |
| `Preferences`（NVS） | ✅ | ESP32 | オンメモリのみ。scalar / string / bytes 各種 put/get を完備。キー不在（または別型で保存済み）の場合はデフォルト値を返す。値はスケッチ終了で消える — boot 跨ぎ動作は実機との interop で検証する必要あり |
| 生の `nvs_flash_*` API | ⛔ | ESP-IDF | `Preferences` 経由で十分 |
| `Update` / OTA | ⛔ | ESP32 | host では意味が無い |
| BLE / Classic Bluetooth | ⛔ | ESP32 | 予定無し |
| ESP-NOW | 🔲 | ESP32 | UDP ブロードキャストで擬似可能だが優先度低 |
| Camera（`esp_camera`） | ⛔ | ESP32 | 対象外 |

### グラフィックス / ディスプレイ

| API | 状況 | 備考 |
|-----|------|------|
| M5GFX フレームバッファモード + 画面キャプチャ | 🔲 | フレームバッファに描画し、画像ファイルとして保存 |
| LovyanGFX / TFT_eSPI | 🔲 | M5GFX と同じ方針 |

### 「Planned」の意味

🔲 の項目には予定日はありません。具体的なスケッチが必要として声を上げた
時点でピックアップします。該当するスケッチがある場合、必要な API 表面を
記述した issue が最も有用なコントリビューションです。

### テストされている範囲

上記マトリックスは「API の存在」、`tests/` 配下は「振る舞いのカバレッジ」
を表します:

```
tests/
  runtime/  smoke, timing, print_api
  storage/  fs
  network/  udp_recv, udp_echo, udp_broadcast, udp_no_begin, wifi,
            tcp_echo, tcp_client, tls_openssl, tls_secure_connect,
            http_get, https_get, http_redirect
  interop/  smoke, wifi_connect, https_get, http_redirect, http_chunked
```

`runtime/`、`storage/`、`network/` は host 専用。`interop/` のスケッチ
は host ランタイムと実 ESP32 シリコンの**両方で動く** — スケッチソース
は両プロファイル間で完全に同一（`#ifdef` 不使用）。ESP32 で実行する
場合:

```bash
cd tests
uv run --env-file .env pytest --profile esp32 interop/
```

`.env` ファイルには `TEST_WIFI_SSID`、`TEST_WIFI_PASSWORD`、ESP32 用シ
リアルポートを記載。ビルド時マクロ注入が必要なテスト（例: `wifi_connect`）
は自身の `build_config.toml` でマッピングを宣言（`TEST_WIFI_SSID = "WIFI_SSID"`
が `-DWIFI_SSID="..."` として渡る）。スケッチは `Serial.begin(115200); delay(5000);`
で実機側の起動 settle 時間を確保 — host ランタイムでは `delay()` が
`std::this_thread::sleep_for` の薄いラッパなので、同じソースが両ターゲット
でクリーンに動作。

各リーフは `.ino` + `sketch.yaml` + `test_*.py` の 3 点セットで、新しい
テストを追加するとマトリックスの 1 マスが緑になっていきます。

## ボード

初期版で提供するボードは 1 つだけです。

```text
lang-ship:host:host
```

- パッケージ: `lang-ship`
- アーキテクチャ: `host`
- ボード ID: `host`
- ボード名: `Host`
- Core ディレクトリ: `cores/host`

初期版ではボードメニューは定義していません。SDL2 やグラフィカルなホストターゲットは、現時点では対象外です。

## リポジトリ構成

- `platform.txt` / `boards.txt`: Arduino platform のメタデータ。リリース ZIP に含めます。
- `cores/host/Arduino.h`: Arduino スケッチ側に見せる最小 API。
- `cores/host/HostRuntime.{h,cpp}`: ホスト実行ランタイム、TCP 経由の `Serial`、プロセス起動、接続情報ファイル処理。
- `cores/host/main.cpp`: `setup()` を 1 回呼び、その後ランタイムが終了要求を出すまで `loop()` を呼ぶ weak `main()`。
- `scripts/build_package.py`: `package/host-arduino-core/` を作成し、ZIP 作成、SHA-256 算出、`package_index.json` 更新を行います。
- `scripts/prepare_release.py`: `CHANGELOG.md` の unreleased entries をリリースバージョンの節へ移動します。
- `.github/workflows/release.yml`: タグ push または手動実行でパッケージを作成し、`package/` を `gh-pages` に公開し、GitHub Releases に成果物を添付します。
- `CHANGELOG.md`: 英語・日本語併記のリリースノート。release workflow は対象バージョンの節を GitHub Release の本文に使います。
- `docs/requirements.ja.md`: 要件定義書。
- `examples/`: リリース ZIP に同梱する手元実行用スケッチ（例: `tls=openssl` ボードメニューを各 OS で検証する `TLSProbe`）。
- `package_index.json`: Boards Manager 用 index。リリース workflow で更新されます。

## 事前準備

このパッケージは Boards Manager 経由でコンパイラ、リンカ、その他ビルドツールをインストールしません。事前に `gcc` / `g++` 互換のホストツールチェーンを用意してください。

- Windows: MSYS2 を導入し、MinGW/UCRT のツールチェーンディレクトリを `PATH` に追加します。
  ```powershell
  winget install MSYS2.MSYS2
  ```
  例として UCRT64 の GCC を入れる場合:
  ```bash
  C:\msys64\usr\bin\pacman -S mingw-w64-ucrt-x86_64-gcc
  ```
  `C:\msys64\ucrt64\bin` を `PATH` に追加し、新しいターミナルで `g++ --version` を確認してください。

- Linux:
  ```bash
  sudo apt update
  sudo apt install build-essential
  ```

- macOS:
  ```bash
  xcode-select --install
  ```

macOS の `g++` は実体が Apple clang のことがありますが、GCC 互換コマンドとして利用できれば問題ありません。

### オプション: OpenSSL（TLS / HTTPS 対応）

ボードメニュー `tls=openssl` を選ぶ場合のみ必要（`WiFiClientSecure` と
`https://` URL を扱う `HTTPClient` で使用）。スケッチが plain TCP / UDP /
HTTP のみであればスキップして構いません。

- Linux（Debian / Ubuntu）:
  ```bash
  sudo apt update
  sudo apt install libssl-dev
  ```

- Linux（Fedora / RHEL / Rocky）:
  ```bash
  sudo dnf install openssl-devel
  ```

- Windows（MSYS2 UCRT64）:
  ```bash
  C:\msys64\usr\bin\pacman -S mingw-w64-ucrt-x86_64-openssl
  ```

- macOS: `tls=openssl` ボードオプションは**現状サポート外**です。リンクレ
  シピが Homebrew 固有の `-I` / `-L` パスを未対応なため。将来対応予定。
  当面 macOS では `tls=disabled` のスケッチをビルドしてください。

dev パッケージをインストールした後の追加設定は不要 — ヘッダ・ライブラリ
が toolchain のデフォルト探索パスに入ります。Arduino IDE の **Tools →
TLS → OpenSSL** を選ぶ（もしくは FQBN に `tls=openssl` を付与、例:
`lang-ship:host:host:tls=openssl`）で有効化されます。

リンクが正しく通っているかは同梱の `TLSProbe` example スケッチで確認可能。
OpenSSL に到達できれば `PROBE_RESULT=PASS`、できなければ `PROBE_RESULT=FAIL`
（ヒント付き）を Serial 出力します。

## Arduino CLI での使い方

1. Boards Manager index を登録します。

   ```bash
   arduino-cli config add board_manager.additional_urls https://tanakamasayuki.github.io/host-arduino-core/package_index.json
   arduino-cli core update-index
   arduino-cli core install lang-ship:host
   ```

2. 動作確認用スケッチを作成します。

   ```bash
   mkdir -p /tmp/HostSmoke
   cat >/tmp/HostSmoke/HostSmoke.ino <<'EOF'
   #include <Arduino.h>

   void setup() {
     Serial.println("boot");
   }

   void loop() {
     if (Serial.available()) {
       int c = Serial.read();
       Serial.print("rx:");
       Serial.println((char)c);
     }
     delay(10);
   }
   EOF
   ```

3. ビルドします。

   ```bash
   cd /tmp/HostSmoke
   arduino-cli compile --fqbn lang-ship:host:host --build-path .build --clean
   ```

4. `upload` で実行ファイルをバックグラウンド起動します。

   ```bash
   arduino-cli upload --fqbn lang-ship:host:host --input-dir .build
   ```

   以下のような出力が出れば起動できています。

   ```text
   HOST_ARDUINO_PORT=xxxxx
   HOST_ARDUINO_INFO=/tmp/HostSmoke/.build/HostSmoke.ino.out.host-arduino.json
   ```

5. TCP 経由の `Serial` に接続します。

   ```bash
   python3 - <<'PY'
   import json, socket, time

   info_path = "/tmp/HostSmoke/.build/HostSmoke.ino.out.host-arduino.json"
   with open(info_path) as f:
       info = json.load(f)

   s = socket.create_connection(("127.0.0.1", info["port"]), timeout=2)
   s.settimeout(2)
   print(s.recv(1024))
   s.sendall(b"A")
   time.sleep(0.1)
   print(s.recv(1024))
   s.close()
   PY
   ```

   期待される出力:

   ```text
   b'boot\n'
   b'rx:A\n'
   ```

   raw TCP を喋れるツールなら何でも同じポートに繋げます。`HOST_ARDUINO_PORT=`
   の値をコピーして、好きなツールで接続してください（以下の `34567` は
   実際のポート番号に置き換える）:

   ```bash
   # nc — Linux / macOS は標準搭載、Windows は nmap か MSYS2 で導入
   nc 127.0.0.1 34567

   # socat — Linux / macOS（apt install socat / brew install socat）
   socat - TCP:127.0.0.1:34567

   # telnet — Windows は機能追加で有効化、macOS は brew、Linux は inetutils
   telnet 127.0.0.1 34567

   # PuTTY（Windows）— cmd / PowerShell からヘッドレスで起動可能
   putty.exe -raw 127.0.0.1 34567
   ```

   TeraTerm（Windows GUI）: File → New Connection → TCP/IP、Service: Other、
   TCP port#: 表示されたポート番号。

## TCP Serial ランタイム

実行ファイルを通常起動すると、まずランチャーとして動作します。

1. 子プロセスをバックグラウンド起動します。
2. 子プロセスが接続情報ファイルを作成するまで待ちます。
3. TCP ポート番号と接続情報ファイルのパスを標準出力に出します。
4. ランチャーは終了し、`arduino-cli upload` が戻ります。

子プロセスは Arduino スケッチ本体を実行し、localhost の TCP サーバを開きます。`Serial.print()`、`Serial.println()`、`Serial.write()` の出力は上限付きバッファに保存されます。テスト側が接続する前に出た出力も、最初の TCP クライアント接続後に送信されます。

接続情報ファイルは実行ファイルと同じ場所に作成されます。

```text
<executable>.host-arduino.json
```

例:

```json
{
  "pid": 12345,
  "port": 34567
}
```

ランタイムは 1 つの実行プロセスに対して 1 つのテストクライアント接続を想定しています。複数クライアントの同時接続は必須要件ではありません。

## ファイルパス

`SD.h`、`SPIFFS.h`、`LittleFS.h`、`FFat.h` はホスト上のフォルダにマップされます。各ファイルシステムのルートは実行ファイルと同じ場所の `SD/`、`SPIFFS/`、`LittleFS/`、`FFat/` です。例えば `SD.open("/data.txt", FILE_WRITE)` は、実行ファイル横の `SD/data.txt` を開きます。

これらのフォルダはビルド成果物の場所に作られるため、読み込み用の fixture が必要な場合は、テスト開始時に `SD/` などへコピーしてください。

スケッチから直接 `fopen()` を使った場合は、このマッピングを通らず、通常のホスト C ランタイムと同じくプロセスのカレントディレクトリ基準になります。pytest から起動する場合は、通常 `test.py` のあるディレクトリをカレントにして実行されるため、`input/` や `output/` のようなテスト用フォルダを置いて扱えます。実行ファイル横に揃えたい場合は、テスト側で起動前または起動時にカレントディレクトリを変更してください。

## ランタイム環境変数

- `HOST_ARDUINO_CONNECT_TIMEOUT_MS`: 子プロセスが最初の TCP クライアント接続を待つ時間。既定値は `10000`。
- `HOST_ARDUINO_PARENT_WAIT_MS`: ランチャーが子プロセスの接続情報公開を待つ時間。既定値は `5000`。
- `HOST_ARDUINO_SERIAL_BUFFER_SIZE`: `Serial` 出力バッファの最大バイト数。既定値は `65536`。
- `HOST_ARDUINO_LOG`: ランタイムログの出力先。既定値は `<executable>.host-arduino.log`。`0`、`false`、`off` を指定するとログを無効化し、ファイルパスを指定すると出力先を変更できます。
- `HOST_ARDUINO_LOG_LEVEL`: ランタイムログのレベル。既定値は `info`。`debug` を指定すると `Serial` の送受信バイト数も記録します。

接続タイムアウトまでに TCP クライアントが接続しない場合、子プロセスは終了します。接続後は、TCP ソケットが切断されるとスケッチプロセスも終了します。

ランタイムログには、ランチャー起動、子プロセス起動、TCP の待ち受け・接続・切断、debug 時の `Serial` バイト数、最終的な終了理由を記録します。`Serial` のデータ内容そのものはログに出力しません。

## Arduino CLI を使わない簡易ビルド

Core 自体を手早く確認したい場合は、直接 `g++` でビルドできます。

```bash
g++ -std=gnu++11 -Wall -I cores/host \
  -x c++ \
  /tmp/HostSmoke/HostSmoke.ino \
  cores/host/HostRuntime.cpp \
  cores/host/main.cpp \
  -pthread \
  -o /tmp/HostSmoke.out

/tmp/HostSmoke.out
```

Windows では Winsock のリンクが必要なため、`-pthread` の代わりに `-lws2_32` を指定します。

## パッケージ化とリリース

リリースは GitHub Actions で行います。

ローカルでパッケージ生成だけ確認する場合:

```bash
python3 scripts/build_package.py --version 0.1.0 --repo tanakamasayuki/host-arduino-core
```

生成されるもの:

- `package/host-arduino-core/`
- `host-arduino-core-0.1.0.zip`
- 更新された `package_index.json`
- GitHub Pages 公開用の `package/package_index.json`

リリース手順:

1. `CHANGELOG.md` を更新します。変更内容を `## Unreleased` に追記します。
2. 変更を `main` にコミットします。
3. `v0.1.0` のようなタグを push するか、GitHub Actions から `Build and Release Host Arduino Core` を手動実行します。
4. workflow が `## Unreleased` の内容を `## <version>` へ移動し、ZIP を作成し、`package_index.json` を更新し、`package/` を `gh-pages` に公開し、GitHub Release に ZIP と index を添付します。
5. GitHub Release の本文には、`## 0.1.0` のような `CHANGELOG.md` の対象バージョン節が入ります。

## 制限事項

- 実機ハードウェアへの書き込みは行いません。
- `arduino-cli upload` は「ホスト実行ファイルを起動する」という意味です。
- `arduino-cli monitor` は非対応です。
- 実シリアルポートや仮想シリアルポートは提供しません。
- TCP ランタイムは localhost でのテスト自動化を想定しています。
- GPIO と analog API は最小スタブであり、実機の正確な再現ではありません。
- 現時点では純粋なロジックテストを対象とし、SDL2 やグラフィカルなホストシミュレーションは含みません。

不具合報告・要望は Issues までお願いします。
