# Changelog / 変更履歴

## Unreleased

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
