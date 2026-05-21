# host-arduino-core ロードマップ

English: [roadmap.md](roadmap.md) | 日本語

host 側で Arduino スケッチをテストするにあたって、本コアで「できること」
「できないこと」を整理したドキュメントです。スケッチの *ロジック* 部分
（パース、状態機械、プロトコルのフレーミング）を、実機書き込み無しで開発
マシン上で検証できるようにするのが目的です。

物理 I/O、ベンダー SDK、特殊無線などに直結する API は対象外で、今後もそ
の方針は変えない予定です。意味のあるエミュレーションをしようとすると別の
プロジェクトになってしまいます。

## 凡例

- ✅ 実装済み・`tests/` でテスト済み
- 🟡 スタブのみ（コンパイル・リンクは通るが、中身は何もしない）
- 🔲 未実装・コントリビューション歓迎
- ⛔ 対象外（このコアでは実装しない）

## ステータス

### ランタイム / 言語

| API | 状況 | 備考 |
|-----|------|------|
| `setup()` / `loop()` | ✅ | `cores/host/main.cpp` の weak `main` |
| `millis` / `micros` | ✅ | `std::chrono::steady_clock` |
| `delay` / `delayMicroseconds` | ✅ | `std::this_thread::sleep_for` |
| `yield` | ✅ | no-op |
| `min` / `max` / `constrain` / `map` | ✅ | ヘッダオンリー |
| `random` / `randomSeed` | ✅ | `std::rand` のラッパ |
| `bit*` / `lowByte` / `highByte` / `_BV` | ✅ | マクロ |

### Serial / Print

| API | 状況 | 備考 |
|-----|------|------|
| `Serial`（UART ファサード） | ✅ | localhost の TCP ソケット越しに公開（README 参照） |
| `Print`（int / hex / bin / float / String / bool） | ✅ | Arduino のフォーマットに準拠 |
| `Stream`（`timedRead` / `readBytes` / `setTimeout`） | ✅ | |

### ファイルシステム

| API | 状況 | 備考 |
|-----|------|------|
| `LittleFS` / `SPIFFS` / `FFat` / `SD` | ✅ | すべて実行ファイル隣のディレクトリにマップ。フラッシュ容量制限やフォーマット意味論は再現しない |
| `File`（read / write / seek / size / openNextFile） | ✅ | `<cstdio>` ラッパ |

### ネットワーク

| API | 状況 | 備考 |
|-----|------|------|
| `IPAddress` | ✅ | Arduino 互換の API 一式 |
| `UDP`（抽象） | ✅ | `cores/host/Udp.h` |
| `WiFiUDP` | ✅ | POSIX / Winsock 実装。`SO_BROADCAST` を `begin()` で有効化。`lastError()` で errno 取得可。受信バッファ 65535 B。パケット操作前に `begin(0)` 必須（ESP32 より厳格）。誤用時は `Serial` に `[HostCore]` ヒントを出力（`cores/host/HostDiag.h` 参照） |
| `WiFiUDP::beginMulticast` | 🔲 | 基底クラスのまま 0 を返す（参加していない）。VBAN 等で必要 |
| `WiFi` ファサード | 🟡 | 状態追跡型のスタブ（`begin` → `WL_CONNECTED`、`disconnect` → `WL_DISCONNECTED`）。実アソシエーション・スキャンは無し |
| `Client` / `Server`（抽象） | 🔲 | TCP 実装の前提 |
| `WiFiClient`（TCP） | 🔲 | POSIX socket で実装可能、概算 250 行 |
| `WiFiServer`（TCP） | 🔲 | POSIX socket で実装可能、概算 120 行 |
| `WiFiClientSecure`（TLS） | ⛔ | mbedTLS / OpenSSL 依存が必要、本コアでは対象外 |
| `HTTPClient` / `WebServer` | ⛔ | `Client` / `Server` の上に別ライブラリとして作るのが適切 |
| mDNS / DNSServer | 🔲 | 優先度低 |

### ハードウェア I/O（意図的にスタブ化）

| API | 状況 | 備考 |
|-----|------|------|
| `pinMode` / `digitalWrite` / `digitalRead` | 🟡 | no-op、`digitalRead` は `0` を返す |
| `analogRead` / `analogWrite` / attenuation / resolution | 🟡 | no-op |
| `touchRead` | 🟡 | `0` を返す |
| `tone` / `noTone` / `pulseIn` | 🟡 | no-op |
| `attachInterrupt` / `detachInterrupt` | 🟡 | no-op |
| `Wire`（I²C） | 🔲 | どうせスタブにしかならないので、必要なスケッチが現れたら追加 |
| `SPI` | 🔲 | 同上 |

ハードウェアに依存する API は「実機向けスケッチが少なくともリンクできる」
ことを目的にスタブ化しています。ピンの状態が結果に影響するテストは、ス
ケッチ層でモックする想定です。

### ESP-IDF / ベンダー拡張

| API | 状況 | 備考 |
|-----|------|------|
| FreeRTOS（`xTaskCreate`、キュー、セマフォ） | ⛔ | 対象外。RTOS のスケジューリングに依存するスケッチは host テスト向きではない |
| `esp_log` / `log_*` マクロ | 🔲 | `Serial` にルーティングは可能 |
| `Preferences`（NVS） | 🔲 | 実行ファイル隣のファイルでバックアップ可能 |
| `EEPROM` | 🔲 | 同上 |
| `Update` / OTA | ⛔ | host では意味が無い |
| BLE / Classic Bluetooth | ⛔ | 予定無し |

## 「Planned」の意味

🔲 の項目には予定日はありません。具体的なスケッチが必要として声を上げた
時点でピックアップします。該当するスケッチがある場合、必要な API 表面を
記述した issue が最も有用なコントリビューションです。

## テストされている範囲

上記マトリックスは「API の存在」、`tests/` 配下は「振る舞いのカバレッジ」
を表します：

```
tests/
  runtime/  smoke, timing, print_api
  storage/  fs
  network/  udp_recv, udp_echo, udp_broadcast, udp_no_begin, wifi
```

各リーフは `.ino` + `sketch.yaml` + `test_*.py` の 3 点セットで、新しい
テストを追加するとマトリックスの 1 マスが緑になっていきます。
