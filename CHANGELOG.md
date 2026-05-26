# Changelog / 変更履歴

## Unreleased
- (EN) Merged the API support matrix into `README.md` / `README.ja.md` and removed `docs/roadmap.md` / `docs/roadmap.ja.md`. The matrix now lives next to the rest of the user-facing docs, with each API tagged as Arduino-standard or ESP32-extension, and expanded coverage for Hardware I/O, ESP-IDF extensions, and graphics.
- (JA) API サポート状況マトリックスを `README.md` / `README.ja.md` に統合し、`docs/roadmap.md` / `docs/roadmap.ja.md` を削除。各 API を Arduino 標準 / ESP32 拡張で区別し、ハードウェア I/O・ESP-IDF 拡張・グラフィックス領域を拡充。
- (EN) Reclassified `WiFiClientSecure` and `HTTPClient` from ⛔ to 🔲 in the API support matrix. API headers will live in `cores/host`; TLS backends are planned along two parallel paths (priority TBD): (A) a board variant linking OpenSSL, (B) a separate repository providing an mbedTLS source-build library via `sketch.yaml`. Default board ships without TLS; HTTPS calls fail at runtime with a `[HostCore]` hint when no backend is enabled. Cert verification is always skipped on host.
- (JA) API サポートマトリックスで `WiFiClientSecure` と `HTTPClient` を ⛔ から 🔲 へ。API ヘッダは `cores/host` 同梱予定。TLS バックエンドは 2 方式を並行検討（優先度未定）: (A) ボード variant で OpenSSL を動的リンク、(B) 別リポジトリで mbedTLS をソース同梱したライブラリを `sketch.yaml` 経由で追加。デフォルトボードは TLS 無しで、未組込み時に HTTPS は実行時失敗＋`[HostCore]` ヒント。host では証明書検証を常時スキップ。
- (EN) Reclassified FreeRTOS scheduling primitives (`xTaskCreate`, `vTaskDelay`, queues, semaphores, mutexes, notifications) from ⛔ to 🔲. Planned host implementation is `std::thread` / `std::mutex` / `std::condition_variable` backed, ignoring priorities, core affinity, and stack size; useful for periodic-worker patterns but explicitly not a substitute for real RTOS scheduling tests.
- (JA) FreeRTOS のスケジューリング系 API（`xTaskCreate` / `vTaskDelay` / キュー / セマフォ / ミューテックス / Notify）を ⛔ から 🔲 へ。`std::thread` / `std::mutex` / `std::condition_variable` でバックする実装方針で、優先度・コア固定・スタックサイズは無視。周期ワーカー用途には十分だが、RTOS スケジューリングそのもののテスト代替にはならないことを非ゴールとして明記。
- (EN) Lowered `WiFiUDP::beginMulticast` to "low priority"; removed the VBAN-specific motivation since cross-platform multicast testing on host (especially Windows / WSL2) is fragile.
- (JA) `WiFiUDP::beginMulticast` を「優先度低」に変更。host 上でのマルチキャストテストは Windows / WSL2 で不安定なため、VBAN 等のユースケース記述は削除。

## 1.0.7
- (EN) Added `HostDiag.h` with `HOST_DIAG_ONCE` for once-per-call-site `[HostCore]` hints over `Serial`. WiFiUDP emits hints when packet methods are called before `begin()` or when sendto / recvfrom fail; FS emits a hint when `File::open()` fails for write/append.
- (JA) `HostDiag.h` を追加。`HOST_DIAG_ONCE` で呼び出しサイトごとに 1 回 `Serial` に `[HostCore]` ヒントを出力。WiFiUDP は `begin()` 前のパケット操作・sendto / recvfrom 失敗時に、FS は `File::open()` の書き込みモード失敗時にヒントを出力。

## 1.0.6
- (EN) Added `IPAddress`, abstract `UDP`, `WiFiUDP`, and a state-tracked `WiFi` stub so ESP32-style sketches can build and run on host.
- (JA) `IPAddress`、抽象 `UDP`、`WiFiUDP`、状態を追跡する `WiFi` スタブを追加し、ESP32 向けスケッチを host でビルド・実行できるように。
- (EN) `WiFiUDP::begin` enables `SO_BROADCAST` so sends to `255.255.255.255` succeed on Linux.
- (JA) `WiFiUDP::begin` で `SO_BROADCAST` を有効化し、Linux でも `255.255.255.255` への送信が成功するように。
- (EN) Added `WiFiUDP::lastError()` and `localPort()`; receive buffer raised to 65535 B for jumbo UDP frames.
- (JA) `WiFiUDP::lastError()` と `localPort()` を追加。受信バッファを 65535 B に拡大し、jumbo UDP フレームに対応。
- (EN) Added `tests/` directory using `pytest-embedded-arduino-cli` with categories `runtime/` `storage/` `network/`; symlinks the repo into the sketchbook automatically for the duration of the run.
- (JA) `pytest-embedded-arduino-cli` を使う `tests/` を `runtime/` `storage/` `network/` のカテゴリで追加。実行中はリポジトリを sketchbook へ自動 symlink。
- (EN) Added `docs/roadmap.md` / `docs/roadmap.ja.md` with an API support matrix.
- (JA) API サポート状況マトリックスを記載した `docs/roadmap.md` / `docs/roadmap.ja.md` を追加。

## 1.0.5
- (EN) Added a main function for when Arduino compatibility is disabled
- (JA) Arduinoを無効時のmain関数追加

## 1.0.4
- (EN) Added an option to disable Arduino compatibility
- (JA) Arduinoを無効にできるように

## 1.0.3
- (EN) Removed -DARDUINO.
- (JA) -DARDUINOを削除
- (EN) Added SDL2 to the menu.
- (JA) SDL2をメニューに追加

## 1.0.2
- (EN) Added host-backed `FS.h`, `SPIFFS.h`, `LittleFS.h`, `FFat.h`, and `SD.h` compatibility with per-filesystem folders next to the executable.
- (JA) 実行ファイル直下のファイルシステム別フォルダを使う `FS.h`、`SPIFFS.h`、`LittleFS.h`、`FFat.h`、`SD.h` 互換を追加。

## 1.0.1
- (EN) Removed the -std flag to allow the build environment's default version to be used.
- (JA) -std指定を削除して、ビルド環境の標準バージョンに合わせる

## 1.0.0
- (EN) Added AVR-style `__FlashStringHelper` / `F()` compatibility plus `Print` overloads for flash strings.
- (JA) AVR 風の `__FlashStringHelper` / `F()` 互換と flash string 用の `Print` overload を追加。
- (EN) Expanded `pgmspace.h` compatibility with common `_P` helpers such as `memcpy_P`, `memcmp_P`, `strcat_P`, `strncat_P`, and `strstr_P`.
- (JA) `memcpy_P`、`memcmp_P`、`strcat_P`、`strncat_P`、`strstr_P` など、よく使われる `_P` helper を `pgmspace.h` に追加。
- (EN) Added Arduino-style numeric `String` concatenation, 64-bit `Print` output overloads, and additional `Stream` parsing/find overloads.
- (JA) Arduino 風の数値 `String` 連結、64bit 値の `Print` 出力 overload、追加の `Stream` parse / find overload を追加。

## 0.1.5
- (EN) Expanded the host Arduino compatibility layer with Arduino-style `String`, `Print`, `Stream`, and `HardwareSerial` APIs.
- (JA) Arduino 互換レイヤを拡張し、Arduino 風の `String`、`Print`、`Stream`、`HardwareSerial` API を追加。
- (EN) Added common compatibility helpers and headers such as `pgmspace.h`, `Printable.h`, `Print::printf()`, GPIO/timing stubs, bit helpers, random helpers, and PROGMEM macros.
- (JA) `pgmspace.h`、`Printable.h`、`Print::printf()`、GPIO / timing スタブ、bit helper、random helper、PROGMEM マクロなどの互換機能を追加。

## 0.1.4
- (EN) Added changelog-based release notes generation so GitHub Releases use the matching version section as their body.
- (JA) GitHub Release の本文に対象バージョンの changelog 節を使う、changelog ベースのリリースノート生成を追加。
- (EN) Added release preparation that moves `## Unreleased` entries into `## <version>` during the release workflow.
- (JA) release workflow 実行時に `## Unreleased` の内容を `## <version>` へ移動するリリース準備処理を追加。

## 0.1.3
- (EN) Added runtime logging with configurable output path and log level through environment variables.
- (JA) 環境変数で出力先とログレベルを設定できるランタイムログ機能を追加。
- (EN) Documented runtime logging behavior in README and requirements documentation.
- (JA) README と要件定義書にランタイムログの挙動を追記。

## 0.1.2
- (EN) Clarified README documentation for Host Arduino Core behavior and usage.
- (JA) Host Arduino Core の挙動と使い方が分かるよう README を整理。
- (EN) Included `README.ja.md` in the release package and fixed package index metadata.
- (JA) リリースパッケージに `README.ja.md` を含め、package index のメタデータを修正。

## 0.1.1
- (EN) Initial host Arduino Core implementation.
- (JA) Host Arduino Core の初期実装。

## 0.1.0
- (EN) Initial release.
- (JA) 初期リリース。
