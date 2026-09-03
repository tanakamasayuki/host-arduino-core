# host-arduino-core（日本語）

日本語 | English: [README.md](README.md)

https://tanakamasayuki.github.io/lang-ship-arduino-core/package_lang-ship_index.json

Arduino スケッチをホスト PC 上の実行ファイルとしてビルドし、`pytest-embedded-arduino-cli` などの Arduino CLI 対応ホスト側テストから制御するための最小構成 Arduino Core + Boards Manager パッケージです。

これは実用的な Arduino 互換ボードではありません。実機、フラッシュ書き込み、シリアルポートに依存しない、純粋なスケッチ / ライブラリのロジック部分をテストするためのターゲットです。

## ハイライト

- アーキテクチャは `host`、自動テスト用 FQBN は `lang-ship:host:host`。
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
| `setup()` / `loop()` / weak `main` | ✅ | Arduino | `cores/host/main.cpp`。thunk の前後 4 点が任意のフックへ届く（[ライフサイクル口](#ライフサイクル口)） |
| `millis` / `micros` | ✅ | Arduino | 既定は `std::chrono::steady_clock`。[時計口](#時計口)経由なのでテストが仮想時計に差し替えられる。32bit なので実機と同じく wrap する |
| `delay` / `delayMicroseconds` | ✅ | Arduino | 既定は `std::this_thread::sleep_for`。[時計口](#時計口)経由で、上書きすれば `delay(5000)` が実時間を消費しない。`delay` のループ・`runtimePoll()`・停止チェックはコア側に残る |
| `yield` | ✅ | Arduino | `runtimePoll()` + [時計口](#時計口)への長さ 0 の待ち。ビジーウェイト中のスケッチがテスト側に実行機会を渡す唯一の場所 |
| `min` / `max` / `constrain` / `map` / `abs` | ✅ | Arduino | ヘッダオンリー |
| `random` / `randomSeed` | ✅ | Arduino | `std::rand` のラッパ |
| `bit*` / `lowByte` / `highByte` / `_BV` | ✅ | Arduino | マクロ |
| `String`（`WString.h`） | ✅ | Arduino | |

### Print / Stream / Serial

| API | 状況 | 出自 | 備考 |
|-----|------|------|------|
| `Print`（int / hex / bin / float / String / bool） | ✅ | Arduino | Arduino のフォーマットに準拠 |
| `Printable` | ✅ | Arduino | |
| `Stream`（`timedRead` / `readBytes` / `setTimeout` / `find` / `parseInt`） | ✅ | Arduino | タイムアウトが[時計口](#時計口)経由。仮想時計に追従し、ブロッキング読み出しの内側からテスト側が応答を積める |
| `HardwareSerial` / `Serial` | ✅ | Arduino | localhost の TCP ソケット越しに公開 |
| `Serial1` / `Serial2` | ✅ | ESP32 | `cores/host/HostUart.h`。コンソールと分離したデバイス向け UART。送受信ともプログラムが駆動するオンメモリのキュー（[デバイス UART](#デバイス-uart)）。`HardwareSerial` 互換は取らない（コンソール側クラスの別名のまま） |

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

### ハードウェア I/O

ハードウェアに依存する API の多くは「実機向けスケッチが少なくともリンクできる」
ことを目的にスタブ化しています。ただしデジタルピン、アナログ出力（PWM / DAC）、
`SPI`、`Wire` はスタブ以上のもので、[バス観測口](#バス観測口)を構成します。
ライブラリ側が自前のデバイス模型を持ち、スケッチがバスに流したものからそれを
駆動できます。

| API | 状況 | 出自 | 備考 |
|-----|------|------|------|
| `pinMode` / `digitalWrite` / `digitalRead` | ✅ | Arduino | ピンの値を保持する。`digitalRead` は直前に書いた値を返し、`INPUT_PULLUP` は HIGH を読む。書き込みは任意のフックへ通知される（[バス観測口](#バス観測口)）。電気的挙動とタイミングは再現しない |
| `analogRead` / `analogReadMilliVolts` / `analogReadResolution` / `analogSetWidth` | ✅ | Arduino / ESP32 | `HostArduino::setAnalogValue` / `setAnalogMilliVolts` で差し込んだ値、またはアナログ読み出しフックが計算した値を返す（アナログ側の応答方向）。分解能は記録するだけで適用しない — 差し込んだ値を勝手にスケールするのは基準電圧を捏造することになる |
| `analogWrite` / `analogWriteFrequency` / `analogWriteResolution` | ✅ | Arduino / ESP32 | LEDC 経由。未 attach のピンは arduino-esp32 と同じくグローバル既定値で初回に attach される。呼び出しはすべてピン・チャンネル・周波数・分解能・デューティ付きで `HostArduino::setAnalogWriteHook` へ通知される |
| `ledcAttach` / `ledcAttachChannel` / `ledcWrite` / `ledcWriteChannel` / `ledcRead` / `ledcReadFreq` / `ledcChangeFrequency` / `ledcDetach` / `ledcWriteTone` / `ledcWriteNote` / `ledcOutputInvert` / `ledcFade*` | ✅ | ESP32 | ピンごとの状態と同じフック。実機が拒否する呼び出し（未 attach ピンへのデューティ書き込み、二重 attach、20 bit を超える分解能、周波数 0）は同じく拒否し、イベントも出さない。チャンネルは 16 本（classic ESP32 の番号付け）。fade は即座に目標値へ到達し（`max_fade_time_ms` は無視、両端点を通知）。2.x の `ledcSetup` / `ledcAttachPin` は**提供しない** — arduino-esp32 3.x が削除済みで、チャンネル指定の書き込みは `ledcWriteChannel` が担う |
| `dacWrite` / `dacDisable` | ✅ | ESP32 | チャンネルと周波数を持たない形で同じピンごとの枠に記録されるので、PWM と DAC が 1 本のトレースに乗る。ピンの制限は掛けない（どのピンが DAC かはバリアント固有の情報で、コアは持たない） |
| `tone` / `noTone` | ✅ | Arduino | LEDC の上に実装。arduino-esp32 と同じく同時に 1 音のみ。`duration` を指定してもブロックしない — 発音と消音を連続して通知するので、実機と共有するスケッチが実時間の待ちなしで同じ呼び出し順を保つ |
| `analogSetAttenuation` / `analogSetPinAttenuation` | 🟡 | ESP32 | 受理するだけ。設定すべき減衰器が無いので、欲しい読み値を直接差し込む |
| `analogContinuous*` | 🔲 | ESP32 | 未提供（必要とする具体的なスケッチがまだ無い） |
| `touchRead` / `touchAttachInterrupt` | 🟡 | ESP32 | `0` を返す |
| `pulseIn` / `pulseInLong` | 🟡 | Arduino | no-op |
| `attachInterrupt` / `detachInterrupt` | 🟡 | Arduino | no-op |
| `Wire`（I²C） | ✅ | Arduino | 同梱 `Wire` ライブラリ。初期化は成功し、ライブラリ側の模型がトランザクションフックを登録するまでデバイスは応答しない（`endTransmission()` → 2、`requestFrom()` → 0）。`begin()` / `end()` / `setPins` / `setClock` / `setTimeOut` はライフサイクルフックに届くので、バスの初期化が通信と同じ順序付きトレースに乗る。`Wire1` も提供 |
| `SPI` | ✅ | Arduino | 同梱 `SPI` ライブラリ。転送された 1 バイトごとにフックが呼ばれ、その戻り値が MISO としてスケッチに返る。`SPISettings` の中身はトランザクションフックで、`begin()` / `end()` / 設定系セッターはライフサイクルフックで取れる。タイミングは再現しない |
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
| M5GFX / LovyanGFX ヘッドレスバックエンド | ✅ | FQBN に `mode=lgfx` を指定（例: `lang-ship:host:host:mode=lgfx`）すると有効。`main()` で `SDL_VIDEODRIVER=dummy` を立てて `Panel_sdl::main` に普通の `setup()`/`loop()` をワーカースレッドから駆動させる構造。`ARDUINO` は未定義のままなので M5GFX/LovyanGFX が `device.hpp` で SDL バックエンドを選ぶ。スケッチは通常通り `Serial.print()` を TCP runtime に流せ、`gfx.createPng()` でフレームを PNG として保存して assert 可能（`tests/graphics/lgfx_smoke` 参照）。リンクは `-lSDL2` 自動付加。Linux は `libsdl2-dev` を導入。M5GFX もしくは LovyanGFX ライブラリがスケッチに含まれている必要あり（コア側は `lgfx::v1::Panel_sdl::main` を forward-declare してリンク時に解決） |
| SDL2 手動表示ボード | ✅ | `lang-ship:host:display` で有効。`upload` で SDL2 画面を開いて手動確認できる。Core は `host` と同じ `cores/host` を使い、API 対応差を作らない。M5Stack / Core2 / CoreS3 などの個別ボード ID は増やさず、`display` ボードのメニューと `sketch.yaml` の profile で対象デバイス、倍率、回転を選択する。TCP runtime は使わず、`Serial` と M5Unified の `M5_LOG*` は標準出力へ出る |
| TFT_eSPI | 🔲 | 計画なし |

### 「Planned」の意味

🔲 の項目には予定日はありません。具体的なスケッチが必要として声を上げた
時点でピックアップします。該当するスケッチがある場合、必要な API 表面を
記述した issue が最も有用なコントリビューションです。

### テストされている範囲

上記マトリックスは「API の存在」、`tests/` 配下は「振る舞いのカバレッジ」
を表します:

```
tests/
  runtime/  smoke, timing, print_api, esp_log, lifecycle_hook,
            clock_hook, uart_buffer, gpio_hook, analog_hook, spi_hook,
            wire_hook, bus_trace, accept_emu_tick, esp_random,
            esp_timer, freertos_mutex, freertos_notify, freertos_queue,
            freertos_task
  storage/  fs, preferences
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

## バス観測口

このコアは周辺機器を意図的に模型化しません。デバイスのプロトコルを知っているのは
そのデバイスを扱うライブラリ側です（ST7789 の初期化列は表示ライブラリ、SD の
コマンドセットは SD ライブラリの持ち物）。コアがデバイス模型を抱え始めると際限が
ありません。コアが用意するのは「バスを覗き、応答を差し込む口」だけで、デバイスの
模型はライブラリ側に置きます。

```text
スケッチ ──SPI.transfer()──▶ コアの SPI ─────┐
スケッチ ──ledcWrite()─────▶ コアの LEDC ────┤──▶ 観測・応答フック
スケッチ ──digitalWrite()──▶ コアの GPIO ────┘         │
                             （書き込み通知）           ▼  ライブラリ側の模型
                                              例: ST7789 のコマンド解釈
                                                        │
                                                        ▼
                                                  仮想 GRAM → PPM / PNG
```

| バス | 宣言場所 | フック |
|------|----------|--------|
| GPIO | `cores/host/HostBus.h`（常に利用可能） | `HostArduino::setPinWriteHook` / `setPinReadHook` / `setPinModeHook` |
| アナログ / PWM（`analogWrite`、`ledc*`、`dacWrite`、`tone`） | `cores/host/HostBus.h`（常に利用可能） | `HostArduino::setAnalogWriteHook` / `setAnalogReadHook` |
| SPI | 同梱 `SPI` ライブラリ | `SPI.setTransferHook` / `SPI.setTransactionHook` / `SPI.setLifecycleHook` |
| I²C | 同梱 `Wire` ライブラリ | `Wire.setWriteHook` / `Wire.setReadHook` / `Wire.setLifecycleHook` |

ライブラリ側の分岐は `HOST_ARDUINO` で行えます。`platform.txt` が両ボードで常に
定義しています。

**フックは種類ごとに 1 スロットです。** 登録は前のフックを置き換えます。チェインに
すると 1 フレームで数百万回通る経路にメモリ確保が入るためです。したがって 1 つの
スケッチでバスを持てるのは同時に 1 ライブラリだけです。1 つの模型が同じバスを
使い回すならピン番号やアドレスで振り分けられますが、**2 つのライブラリが同じバスを
同時に観測することはできません**。`nullptr`（または `clearPinHooks()` /
`clearHooks()`）でスロットを解放できるので、テスト内で模型を差し替えるのはその形で
行います。

### GPIO — ビットバン経路を丸ごと拾える口

いちばん効くのはこちらです。ビットバンの転送はバスクラスを一切経由しないためで、
ソフト SPI、ソフト I²C、WS2812、IR のパルスはすべて `digitalWrite` を直接叩きます。

- `digitalWrite` はすべてフックへ通知されます（同じ値の書き込みも含む。エッジの
  判定は模型側の仕事）。
- 各ピンは直前に書かれた値を保持し、`digitalRead` がそれを返します。
- `pinMode(pin, INPUT_PULLUP)` は保持値を HIGH に（`INPUT_PULLDOWN` は LOW に）
  します。open-drain を release した線が実機どおりに読めるのはこのためです。
  `INPUT` は保持値を変更しません（フローティングのピンに定義された電位は無い）。
- `HostArduino::setPinValue(pin, level)` で入力値を差し込めます。これが GPIO 側の
  応答方向です。読み出し時に値を計算したい模型向けに `setPinReadHook` もあります。
- 追跡するのはピン 0〜255 です。範囲外への書き込みはフックを呼ばずに捨て、読み出し
  は 0 を返します。

```cpp
#include <Arduino.h>

// 表示ライブラリが持つべきものの代役: SCK の立ち上がりでビットを組み立て、
// DC 線を読んでコマンドとデータを区別する。
void onPinWrite(uint8_t pin, uint8_t value, void *user)
{
    auto *model = static_cast<PanelModel *>(user);
    if (pin != PIN_SCK || !value) return;
    model->shiftIn(digitalRead(PIN_MOSI));
    if (model->byteComplete()) {
        model->feedByte(digitalRead(PIN_DC) == LOW);   // LOW ならコマンド
    }
}

HostArduino::setPinWriteHook(onPinWrite, &model);
```

コスト（実測）: 240x240 16bpp の 1 フレームをビットバンで押し出す場合
（`digitalWrite` 276 万回）、フック無しで 1.1 ms、フック有りで 7.2 ms。書き込み経路は
inline のままで、フックは `std::function` ではなく素の関数ポインタです。

### アナログ / PWM — バックライトの口

`analogWrite`、`ledc*` 一族、`dacWrite`、`tone` はいずれも最終的に 1 本のピンの
アナログ出力を駆動するもので、すべて同じフックへ通知されます。no-op スタブでも
表示ドライバはリンクできますが、それだとバックライトの初期化が見えません。パネルの
立ち上げの中で**順序**が最も重要なのがまさにそこです（暗いまま設定 → 初期化
シーケンス → 点灯）。

呼び出しごとにスロットを分けず 1 本にしているのは、検証したいものが順序付きの
シーケンスだからです。1 本のストリームならゴールデンと直接比較できますが、4 本
だとテスト側が先に時系列へ合成し直す必要があります。イベントは API よりも粗い
粒度で、「どの綴りで呼ばれたか」ではなく「ピンに何が起きたか」を記録します:

| イベント | 通知元 |
|----------|--------|
| `kAnalogAttach` | `ledcAttach`、`ledcAttachChannel`、`analogWrite` / `tone` の暗黙 attach |
| `kAnalogWrite` | `ledcWrite`、`ledcWriteChannel`、`analogWrite`、`ledcFade*` の両端点 |
| `kAnalogConfig` | `ledcChangeFrequency`、`ledcOutputInvert`、`analogWriteFrequency`、`analogWriteResolution` |
| `kAnalogTone` | `ledcWriteTone`、`ledcWriteNote`、`tone` |
| `kAnalogDetach` | `ledcDetach`、`noTone`、`dacDisable` |
| `kAnalogDac` | `dacWrite` |

```cpp
#include <Arduino.h>

void onBacklight(HostArduino::AnalogWriteEvent event, const HostArduino::AnalogOut &out, void *user)
{
    // out.pin / out.channel / out.frequency / out.resolution / out.duty
    static_cast<Trace *>(user)->add(event, out);
}

HostArduino::setAnalogWriteHook(onBacklight, &trace);
ledcAttach(PIN_BL, 5000, 8);   // kAnalogAttach, チャンネル 0, 5000 Hz, 8 bit
ledcWrite(PIN_BL, 0);          // kAnalogWrite, duty 0 — 初期化中は暗いまま
ledcWrite(PIN_BL, 200);        // kAnalogWrite, duty 200 — ここで点灯
```

`HostArduino::analogOut(pin)` はフック無しで同じ状態を返します。最終状態だけを
見たいときはこちらを検証してください。`ledcReadFreq(pin)` は arduino-esp32 に
忠実で duty が 0 の間は 0 Hz を返すため、設定値が隠れます。

- 実機が拒否する呼び出しは同じく拒否します（未 attach ピンへの duty 書き込み、
  二重 attach、`HostArduino::kLedcMaxResolution`（20）を超える分解能、周波数 0）。
  そのときイベントも出さないので、実機がやらなかった作業がトレースに現れません。
- `ledcAttach` は空いている最小のチャンネルを割り当てます。arduino-esp32 と同じ
  選び方なので、トレースに出るチャンネルは実機で割り当てられるものと一致します。
- 応答方向: `HostArduino::setAnalogValue(pin, raw)` と
  `setAnalogMilliVolts(pin, mv)` が `analogRead` / `analogReadMilliVolts` の
  返す値になります。読み出し時に計算したい模型向けに `setAnalogReadHook` も
  あります。2 つの値を別々に差し込む形にしているのは意図的で、一方から他方を
  導くには減衰器と Vref の模型が必要になり、コアはそれを持ちません。
- 再現しないもの: 波形、タイミング、タイマの共有。ピンには何も出力されず、
  `digitalRead` が PWM 波形を見ることはなく、duty 128/255 で何かが半分明るく
  なることもありません。fade は即座に目標値へ到達します。

### SPI

`SPI.transfer()` は転送する 1 バイトごとにフックを呼び、その戻り値をそのまま返します。
つまり 1 本のフックで「会話の観測」と「MISO での応答」の両方が足ります。フック未登録
の転送は `0xFF` を返します（何も駆動していないアイドルのバスの読み値）。

```cpp
#include <SPI.h>

uint8_t onByte(uint8_t out, void *user)
{
    static_cast<MyPanelModel *>(user)->feedByte(out);
    return 0xFF;                       // 書き込み専用デバイス
}

void onTransaction(bool active, const SPISettings &s, void *user)
{
    if (active) Serial.printf("%u Hz, order %u, mode %u\n", s.clock(), s.bitOrder(), s.dataMode());
}

SPI.setTransferHook(onByte, &model);
SPI.setTransactionHook(onTransaction);
```

`SPISettings` は読み取り用に `clock()` / `bitOrder()` / `dataMode()` を持ちます。
アンダースコア付きの `_clock` / `_bitOrder` / `_dataMode` も public のまま残して
あります（arduino-esp32 のスケッチがその綴りで書くため）。`SPI.settings()` /
`inTransaction()` / `transferCount()` / `sck()` / `miso()` / `mosi()` / `ss()` が
あるので、フックを一切使わずに配線とトラフィックを検証することもできます。フックはバイト単位です。ビット順は `SPISettings` として報告するだけで
バイトには適用しません（並べるべき実際の線が無いため）。クロックは記録するだけで、
待ちには反映しません。

`SPI.setLifecycleHook` はバス自体を見ます。`SPIClass::kBegin`、`kEnd`、そして
`setFrequency` / `setBitOrder` / `setDataMode` / `setClockDivider` / `setHwCs`
（バスを専有するドライバが transaction を使わずに設定する綴り）の `kConfig` です。
ピンは元から後から読めましたが、リセットパルスやバックライトに対して**いつ**バスが
立ち上がったのかを言えるのはフックだけです。

### I²C

`Wire` は初期化に成功し、バス上には何も居ません（空きアドレスに対してスキャン
ループが期待する形）。フックはバイト単位ではなくトランザクション単位です。I²C の
デバイス模型が実際に扱う粒度が、アドレス + ペイロード + ストップ条件だからです。

```cpp
#include <Wire.h>

uint8_t onWrite(uint8_t addr, const uint8_t *data, size_t len, bool stop, void *user)
{
    if (addr != 0x68) return 2;        // アドレス NACK — 誰も居ない
    static_cast<MySensor *>(user)->command(data, len);
    return 0;                          // ACK
}

size_t onRead(uint8_t addr, uint8_t *data, size_t len, bool stop, void *user)
{
    return addr == 0x68 ? static_cast<MySensor *>(user)->fill(data, len) : 0;
}

Wire.setWriteHook(onWrite, &sensor);
Wire.setReadHook(onRead, &sensor);
```

`Wire.setLifecycleHook` はバス自体を見ます。`TwoWire::kBegin`、`kEnd`、
`kSetPins`、`kSetClock`、`kSetTimeout` です。`sda()` / `scl()` / `getClock()` は
元から後から読めましたが、ドライバが行った他のすべてに対して**いつ**バスを
立ち上げたのかを言えるのはフックだけです。フックは `TwoWire` ごと受け取るので、
1 つの模型が両方を見る場合は `busNum()` で `Wire` と `Wire1` を区別できます。

再現しないもの: クロックストレッチ、アービトレーション、バスのタイミング、
スレーブ動作（`onReceive` / `onRequest` は受け付けるが呼ばれない）。

### ゴールデントレース

すべてのフックを登録して 1 本のバッファへ 1 イベント 1 行で追記すると、ドライバの
立ち上げ全体（I²C 起動 → リセットパルス → SPI 起動 → バックライトを暗く設定 →
コマンド送出 → バックライト点灯 → タッチ探索）が順序付きの記録になり、ゴールデン
リストと 1 行ずつ比較できます:

```text
i2c.begin sda=21 scl=22 clock=100000
gpio.mode pin=33 mode=1
gpio.write pin=33 value=0
gpio.write pin=33 value=1
spi.begin sck=18 mosi=23 cs=5
pwm.attach pin=38 ch=0 f=5000 r=8
pwm.write pin=38 duty=0
spi.txn active=1 clock=40000000 mode=0
spi.byte 01 cs=0
...
pwm.write pin=38 duty=200
```

1 つの手順が前後の手順に対してずれた場合、最終状態の検証はすべて通ってもこの比較は
失敗します。表示ドライバの不具合として実際にありがちで、最終状態のテストでは
取り逃がす種類のものです。フックの中から直接 print せず、記録して最後に一度出力
してください。そうすればスケッチが `Serial` へ書く他の出力からトレースが独立します。
実例は `tests/runtime/bus_trace` です。

### スレッドと、意図的に用意しないもの

フックはバスを呼んだスレッド上で同期的に実行され、状態はロック無しの素の配列
です。`mode=lgfx`（および `display` ボード）では `setup()` / `loop()` がワーカー
スレッドで動き、FreeRTOS タスクは `std::thread` なので、フックの登録はタスク起動前に
行い、1 つのバスは 1 スレッドから使ってください。

- **コアにデバイス模型は入れません。** SD カードも LCD も入れません。プロトコルを
  知っているライブラリの持ち物です。
- **フックの引数にタイムスタンプは持たせません。** `micros()` が既に利用可能で
  monotonic なので、必要なフックが自分で呼べば足り、不要なフックは何も払いません。
- **フレームバッファを渡して表示する口はありません。** `display` ボードの SDL2
  ウィンドウはコアの持ち物ではなく LovyanGFX の `Panel_sdl` のものです
  （`cores/host/main.cpp` はそのエントリポイントを前方宣言しているだけ）。目で見たい
  模型は実行ファイル隣に PPM / PNG を書くか、画素を `LGFX_Sprite` へ流し込んでください。

実例: `libraries/Host/examples/Plane/BusObserve`、および
`tests/runtime/gpio_hook`、`tests/runtime/analog_hook`、`tests/runtime/spi_hook`、
`tests/runtime/wire_hook`、`tests/runtime/bus_trace`。

## 拡張口

上のバス観測口は「スケッチがバスに流したもの」をライブラリ側から見る口です。
テストの土台がスケッチの**横**ではなく**下**に入るために必要な残りを、3 つの
小さな口が受け持ちます。実行する場所、いまが何時か、話し相手です。いずれも
1 スロット・1 利用者で、多重化は上の層の仕事です。だからこそ既存のものを
何も置き換えていません（既存のバスフックは無変更です）。

### ライフサイクル口

`cores/host/HostLifecycle.h`。Arduino thunk の前後 4 点です。

```text
runtimeStart()
  kPreSetup          1 回 — 試験の支度。Serial はもう使える
  setup()
  kPostSetup         1 回 — setup() が残した状態のダンプ
  loop {
    kPreLoop         毎周 — スケッチがこれから観測するものを走らせる
    loop()
    kPostLoop        毎周 — その周を締める
    runtimePoll()
  }
```

```cpp
void onPhase(HostArduino::LifecyclePhase phase, void *user)
{
    if (phase == HostArduino::kPostLoop) advanceMyClock();
}

HostArduino::setLifecycleHook(onPhase, &harness);
```

**`kPostLoop` は `runtimePoll()` の後ではなく前です。** 外部入力の取り込みは
つねに「その周を締めた後」になり、**次の周のもの**としてスタンプされます。逆順
だと `runtimePoll()` が拾ったバイトが「アプリがすでに走り終わった周」に落ちて、
スケッチが見られなかったものがその周のトレースに写ります。

4 スロットではなくフック 1 本 + phase 引数にしているのは、バス観測口のアナログ
半分と同じ理由です。4 点は順序付きのシーケンスであり、1 本のストリームとして
欲しい利用者に 4 本の合成を強いるべきではありません。

`HostArduino::loopCount()` が周回数です。`kPostLoop` でフックが走る**前**に
加算されるので、`kPostLoop` から読むと「いま締めている周」の番号が得られます。
`kPreSetup` を捕まえたければグローバルコンストラクタから登録してください。
`setup()` からでは既に取り逃がしています。

thunk は 2 つとも同じ 4 点を通知します（通常の host ランタイムと、`mode=lgfx`
および `display` ボードが使う SDL 版）。SDL 版はワーカースレッドで走るため、
`main` から登録したフックはそちらでは別スレッドで呼ばれます。

### 時計口

`cores/host/HostClock.h`。関数 2 つ、「いま何時か」と「1 スライス待つ」です。

```cpp
uint64_t virtualNow(void *user)                 { return c(user)->micros; }
void     virtualWait(uint32_t us, void *user)   { c(user)->micros += us; }

HostArduino::setClockHooks(virtualNow, virtualWait, &clock);
```

スケッチから見える時間処理はすべてここを通ります。`millis`、`micros`、`delay`、
`delayMicroseconds`、`yield`、そして `readBytes` / `find` / `parseInt` が乗って
いる `Stream` のタイムアウトです。

**なぜ「いま」だけでなく「待ち」も渡すのか。** 「いま何時か」だけ差し替えても、
待つ処理は別の場所にあるので `delay(1000)` は実時間 1 秒眠り続けます。待ちも
渡すことで、役に立つ 3 状態が 1 つの機構になります。

| 入れたもの | 結果 |
|---|---|
| 何も入れない | 実時間の monotonic クロック。`delay` は本当に眠る（この口ができる前と同じ） |
| 待ちだけ | 実時間のまま、すべての待ちの 1ms スライスごとに自分のコードが走る（onTick 方式） |
| 両方 | 仮想時間。`delay(5000)` がホスト速度で戻り、時計は 5000ms 進む |

**「いま」だけ**を入れるのが唯一避けるべき組み合わせです。期限は仮想で待ちは
実時間になり、`delay` のループが期限に到達せず回り続けます。

**コア側に残るもの。** `delay` のループ・`runtimePoll()`・停止チェックは上書き
できません。上書き側が忘れられないようにするためです。

```cpp
deadline = clockNowMicros() + ms * 1000;
while (!runtimeShouldStop() && clockNowMicros() < deadline) {
    runtimePoll();
    clockWaitMicros(1000);
}
```

`clockRealNowMicros()` / `clockRealWaitMicros()` は何が入っていても実時計に
届きます。onTick 方式のフックは後者を呼んで実際に眠り、テストは前者で自分の
実時間コストを測ります。

`yield()` は時限待ちではありませんが、長さ 0 の待ちとして口を通ります。
ビジーウェイト中のスケッチがホスト側に実行機会を渡す唯一の場所だからです。
`delay` も `yield` も無い `while (millis() - t0 < 1000);` は、他の場所でしか
時計が進まない仮想時計の下では終わりません。読み取りごとに微小に進めるのが
通例の答えで、それは上書き側の仕事です。

### 実時間のまま残るタイムアウト

時計口はコア内のすべての待ちを覆っているわけではありません。仮想時計で書く
テストは「どの読み値が仮想時計と一致しないか」を知る必要があるので、全一覧を
ここに置きます。

**意図的に口の外にあるもの** — いずれも実期限 + 実待ちで自己完結しており、必ず
満了します。実時間を消費し、仮想の `millis()` とは一致しません。

| 何 | 場所 | 備考 |
|---|---|---|
| `vTaskDelay` / `vTaskDelayUntil` / `xTaskGetTickCount` | `cores/host/freertos/FreeRTOS.h` | FreeRTOS タスク自身の時計。仮想時計の下で `xTaskGetTickCount()` は `millis()` と一致しない |
| `xQueueSend` / `xQueueReceive` / `xSemaphoreTake` / `ulTaskNotifyTake` のタイムアウト | `cores/host/freertos/FreeRTOS.h` | `condition_variable::wait_for` なので通知で早く起きる。「1 スライス待つ」1 関数では表現できない二条件待ち |
| `esp_timer` の発火と `esp_timer_get_time()` | `cores/host/esp_timer.h` | タイマごとに 1 スレッド、condition variable で待つ |
| `WiFiClientSecure` のハンドシェイク予算（5 秒） | `cores/host/WiFiClientSecure.h` | `select()` で `SSL_connect` を駆動 |

これらはマルチスレッドの事例で、「誰が時計を進めるか」に答えが必要なため仮想化
していません。コントリビューション歓迎です。それまでは、仮想時計の下で
`millis()` と `xTaskGetTickCount()` を混ぜると両者が乖離します。

**恒久的に実時間のもの** — プロセス起動とソケット、`cores/host/HostRuntime.cpp`
内の TCP accept 待ち、`waitForClient`、`HOST_ARDUINO_START_DELAY_MS`、ランチャの
port ファイル待ち、send のリトライです。ここを仮想化すると、テスト側が実ソケット
で接続する前にランタイムが止まり、何も起動しません。

`HTTPClient` の読み取りタイムアウトはこの一覧に**入りません**。期限を `millis()`
から取り `delay(1)` で待つので、仮想時計に正しく追従します。

### デバイス UART

`cores/host/HostUart.h`。`Serial` はコンソールです。TCP ランタイム（`display`
ボードでは stdout）へ出て行き、スケッチが print したものをテストが読む口です。
`Serial1` と `Serial2` はもう一方の UART、デバイスがぶら下がる側なので、
プロセスの外には一切繋がっていません。

```text
スケッチ --write()--> tx キュー --readTx()--> テスト側
スケッチ <--read()--- rx キュー <--pushRx()-- テスト側
```

```cpp
Serial1.begin(9600, SERIAL_8N1, 16, 17);

// テスト側:
const String sent = Serial1.readTxString();
if (sent.startsWith("AT")) Serial1.pushRx("OK\r\n");
```

どちらのキューも他に消費者がいないので、会話全体をテストが所有します。
`begun()` / `baudRate()` / `config()` / `rxPin()` / `txPin()` が `begin()` に
渡された値を返し、`txTotal()` / `rxTotal()` が中身を解釈せずに流量を数えます。
`txOverflowed()` / `rxOverflowed()` はキューがバイトを捨てたときに立つ sticky な
フラグで、push ごとではなく最後に 1 回確認すれば足ります。キューは既定で各 1KB、
`setRxBufferSize` / `setTxBufferSize` で変更できます。

**1 周の中で応答する。** コマンドを書いて `loop()` を抜ける前に応答を読む
スケッチ（AT コマンド系ドライバはすべてこれ）は `kPreLoop` からは応答できません。
返答が 1 周遅れて届くためです。代わりに時計口で応答できます。
`Stream::readBytes` が `clockWaitMicros` で待つので、待ちを上書きしたテスト側が
スケッチ自身のブロッキング読み出しの内側から tx を抜き rx へ積めます。
`tests/runtime/uart_buffer` が両方の形を実演しています。

**`HardwareSerial` 互換は取りません。** 実機では `Serial`・`Serial1`・USB CDC が
同一クラスですが、そのクラスはこのコアに存在しない周辺機器を記述するためのもので、
そもそもライブラリが「話す先」を要求する方法として `HardwareSerial&` は筋が良く
ありません（`Stream&` で足ります）。したがって `HostUart` は `Stream` のみを継承し、
`HardwareSerial` は従来どおりコンソール側クラスの別名のままです。
`HardwareSerial&` を要求するライブラリは `Serial1` を受け取れません。

再現しないもの: ボーレートのタイミング（書いた瞬間にバイトが現れます）、
フレーミング、パリティ、ブレーク検出、フロー制御。

実例: 1 つずつは `tests/runtime/lifecycle_hook`、`tests/runtime/clock_hook`、
`tests/runtime/uart_buffer`。全部を同時に使う例が
`tests/runtime/accept_emu_tick` で、仮想時計の tick 模型が「そのために書かれて
いない」普通のデバウンス + AT コマンドのスケッチを下から駆動します。

## ボード

標準ボードは自動テスト用の `Host` と、手動表示確認用の `Host Display` です。

```text
lang-ship:host:host
```

- パッケージ: `lang-ship`
- アーキテクチャ: `host`
- ボード ID: `host`
- ボード名: `Host`
- Core ディレクトリ: `cores/host`

`Host` は CI / pytest / headless 実行を安定させるためのボードです。`upload`
は実行ファイルをバックグラウンドで起動し、`Serial` は TCP runtime 経由で
テスト側から制御します。TCP 接続が一定時間確立されない場合や、一度確立し
た TCP 接続が切断された場合は、テスト実行後にプロセスが残らないよう自動
終了します。

グラフィックスの自動テスト向けには、既存の `mode=lgfx` メニューを使いま
す。

```text
lang-ship:host:host:mode=lgfx
```

このモードは SDL2 の dummy video driver を使う headless 実行で、PNG キャ
プチャなどをテストから検証する用途です。

手動表示確認用には `Host Display` ボードを使います。

```text
lang-ship:host:display
```

`Host Display` は `Host` と同じ `cores/host` を使い、Core API の対応差を
作らないまま、`upload` で SDL2 画面を開く手動テスト用ボードです。
M5Stack / Core2 / CoreS3 などの個別ボード ID は増やさず、
`display` ボードのメニューと example ごとの `sketch.yaml` profile で対象
デバイス、倍率、回転を選ぶ構成です。

LovyanGFX で画面サイズを明示したい場合は、`Display Board` メニューで対象
を選びます。メニュー選択は `M5GFX_BOARD` に加えて
`HOST_DISPLAY_WIDTH` / `HOST_DISPLAY_HEIGHT` も定義するため、
LovyanGFX スケッチからも同じ設定を参照できます。

```bash
arduino-cli upload -p NONE \
  --fqbn 'lang-ship:host:display:m5gfx_board=board_M5Stack,m5gfx_scale=x2,m5gfx_rotation=r0' \
  libraries/Host/examples/SDL2/HostDisplayLovyanGFX
```

この例は `M5Stack (320x240)` を 2 倍表示、回転 0 で起動します。
M5Unified を使う場合は
`libraries/Host/examples/SDL2/HostDisplayM5Unified` も同じ FQBN で起動
できます。

`Host Display` では TCP runtime を使いません。接続待ち、接続直後の settle
待ち、接続情報ファイル、TCP 切断による自動終了は `Host` 専用の仕様にし、
手動表示確認では起動後すぐに `setup()` / `loop()` を開始します。`Serial`
出力は標準出力へ流し、終了は SDL2 画面を閉じる、スケッチ自身が終了する、
または利用者がプロセスを終了する操作を基本にします。

M5Unified の `M5_LOGE()` / `M5_LOGW()` / `M5_LOGI()` / `M5_LOGD()` /
`M5_LOGV()` は ESP32 の `ESP_LOG*` ではなく `M5.Log` に展開されます。PC /
SDL2 ビルドでは M5Unified が標準出力へ `printf()` するため、手動確認時は
SDL2 画面とは別にログコンソールへ表示されます。

## リポジトリ構成

- `platform.txt` / `boards.txt`: Arduino platform のメタデータ。リリース ZIP に含めます。
- `cores/host/Arduino.h`: Arduino スケッチ側に見せる最小 API。
- `cores/host/HostRuntime.{h,cpp}`: ホスト実行ランタイム、TCP 経由の `Serial`、プロセス起動、接続情報ファイル処理。
- `cores/host/HostBus.{h,cpp}`: GPIO のピン状態、アナログ出力（PWM / DAC）の状態、バス観測フック（[バス観測口](#バス観測口)）。
- `cores/host/HostLifecycle.{h,cpp}`: Arduino thunk の前後 4 点（[ライフサイクル口](#ライフサイクル口)）。
- `cores/host/HostClock.{h,cpp}`: コアが時計を読む唯一の場所と、待つ唯一の場所（[時計口](#時計口)）。
- `cores/host/HostUart.{h,cpp}`: `Serial1` / `Serial2`、デバイス向け UART（[デバイス UART](#デバイス-uart)）。
- `libraries/SPI/`、`libraries/Wire/`: 同梱の `SPI` / `Wire`。同じ観測口の SPI 半分と I²C 半分。
- `cores/host/main.cpp`: `setup()` を 1 回呼び、その後ランタイムが終了要求を出すまで `loop()` を呼ぶ weak `main()`。
- `scripts/bump_version.py`: `platform.txt` の `version=` と `libraries/Host/examples/*/*/sketch.yaml` の host platform バージョンを更新します。
- `scripts/build_package.py`: `package/host-arduino-core/` を作成し、ZIP 作成、SHA-256 算出、`package_index.json` 更新を行います。
- `scripts/prepare_release.py`: `CHANGELOG.md` の unreleased entries をリリースバージョンの節へ移動します。
- `.github/workflows/release.yml`: タグ push または手動実行でパッケージを作成し、`package/` を `gh-pages` に公開し、GitHub Releases に成果物を添付します。
- `CHANGELOG.md`: 英語・日本語併記のリリースノート。release workflow は対象バージョンの節を GitHub Release の本文に使います。
- `docs/requirements.ja.md`: 要件定義書。
- `libraries/Host/examples/`: リリース ZIP に同梱する手元実行用スケッチ（例: `tls=openssl` ボードメニューを各 OS で検証する `TLSProbe`）。
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

### オプション: SDL2（LovyanGFX / M5GFX ヘッドレス描画）

ボードメニュー `mode=lgfx` を選ぶ場合のみ必要（M5GFX / LovyanGFX /
M5Unified スケッチを host PC 上でオフスクリーン描画する用途、CI で
レイアウト確認するなどに使う）。スケッチがグラフィックを触らない場合は
スキップして構いません。

- Linux（Debian / Ubuntu）:
  ```bash
  sudo apt update
  sudo apt install libsdl2-dev
  ```

- Linux（Fedora / RHEL / Rocky）:
  ```bash
  sudo dnf install SDL2-devel
  ```

- Windows（MSYS2 UCRT64）:
  ```bash
  C:\msys64\usr\bin\pacman -S mingw-w64-ucrt-x86_64-SDL2
  ```

- macOS（Homebrew）:
  ```bash
  brew install sdl2
  ```
  リンクレシピが Homebrew 固有の `-I` / `-L` パスを追加していないため、
  macOS の `mode=lgfx` ビルドは現状未検証（`tls=openssl` と同じ注意）。

dev パッケージをインストール後の追加設定は不要 — `-lSDL2` は
`mode=lgfx` メニュー側で注入され、標準探索パスでヘッダも見つかります。
Arduino IDE の **Tools → Mode → LovyanGFX / M5GFX headless** を選ぶ
（もしくは FQBN に `mode=lgfx` を付与、例: `lang-ship:host:host:mode=lgfx`）
で有効化されます。このモードでは SDL2 が常に `SDL_VIDEODRIVER=dummy` で
動作するためウィンドウは出ず、出力は `gfx.createPng()` でディスクに
書き出して確認します。

リンクが正しく通っているかは `tests/graphics/lovyangfx_smoke`
（もしくは `m5gfx_smoke` / `m5unified_smoke`）で確認可能 — 各テストが
複数サイズの PNG を `output/` に生成します。

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

子プロセスは localhost の TCP サーバを開いて接続情報を公開し、最初の TCP
クライアントが接続してから Arduino スケッチの `setup()` に入ります。これに
より、pytest などの制御側が TCP 経由の `Serial` ストリームへ接続し終わる前に
初期の `Serial.print()` / `Serial.println()` 出力が発生することを避けます。
`HOST_ARDUINO_CONNECT_TIMEOUT_MS` までにクライアントが接続しない場合、子プロセス
は終了します。

`Serial.print()`、`Serial.println()`、`Serial.write()` の出力は上限付きバッファに
保存され、接続済みの TCP クライアントへ送信されます。

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
- `HOST_ARDUINO_START_DELAY_MS`: 最初の TCP クライアント接続後、`setup()` を開始する前の短い settle 待ち。既定値は `250`。
- `HOST_ARDUINO_PARENT_WAIT_MS`: ランチャーが子プロセスの接続情報公開を待つ時間。既定値は `5000`。
- `HOST_ARDUINO_SERIAL_BUFFER_SIZE`: `Serial` 出力バッファの最大バイト数。既定値は `65536`。
- `HOST_ARDUINO_LOG`: ランタイムログの出力先。既定値は `<executable>.host-arduino.log`。`0`、`false`、`off` を指定するとログを無効化し、ファイルパスを指定すると出力先を変更できます。
- `HOST_ARDUINO_LOG_LEVEL`: ランタイムログのレベル。既定値は `info`。`debug` を指定すると `Serial` の送受信バイト数も記録します。

クライアント接続後にスケッチが開始され、以後 TCP ソケットが切断されると
スケッチプロセスも終了します。

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
2. ローカルでリリース前の差分を確認したい場合は、`python3 scripts/bump_version.py 0.1.0` で `platform.txt` と example `sketch.yaml` のバージョンを更新します。release workflow も同じ処理を実行します。
3. 変更を `main` にコミットします。
4. `v0.1.0` のようなタグを push するか、GitHub Actions から `Build and Release Host Arduino Core` を手動実行します。
5. workflow が `platform.txt` と example `sketch.yaml` のバージョンを更新し、`## Unreleased` の内容を `## <version>` へ移動し、ZIP を作成し、`package_index.json` を更新し、`package/` を `gh-pages` に公開し、GitHub Release に ZIP と index を添付します。
6. GitHub Release の本文には、`## 0.1.0` のような `CHANGELOG.md` の対象バージョン節が入ります。

## 制限事項

- 実機ハードウェアへの書き込みは行いません。
- `arduino-cli upload` は「ホスト実行ファイルを起動する」という意味です。
- `arduino-cli monitor` は非対応です。
- 実シリアルポートや仮想シリアルポートは提供しません。
- TCP ランタイムは localhost でのテスト自動化を想定しています。
- GPIO と analog API は最小スタブであり、実機の正確な再現ではありません。
- 現時点では純粋なロジックテストを対象とし、SDL2 やグラフィカルなホストシミュレーションは含みません。

不具合報告・要望は Issues までお願いします。
