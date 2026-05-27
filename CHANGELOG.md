# Changelog / 変更履歴

## Unreleased
- (EN) Excluded `tests/interop/` from default pytest collection via `addopts = "--save-state --ignore=interop"` in `tests/pyproject.toml`. A bare `pytest` invocation never accidentally runs the interop tests (which would fail without `.env` and a real ESP32). Explicit `pytest interop/` still works — `--ignore` suppresses recursive discovery but not command-line path arguments. Configured at the `pyproject.toml` level (no `conftest.py` mechanism) and uses a blacklist so new top-level categories are auto-collected by default.
- (JA) `tests/pyproject.toml` の `addopts = "--save-state --ignore=interop"` で `tests/interop/` を default の収集対象から外した。素の `pytest` 実行で interop テストが誤って走らない（`.env` 不在 + 実機未接続では失敗するため）。`pytest interop/` の明示指定は引き続き動作 — `--ignore` は再帰探索のみ抑制し、コマンドラインで渡されたパスは尊重される。`pyproject.toml` レベルで完結（`conftest.py` 機構を使わない）、blacklist 方式なので新カテゴリ追加時は自動で収集対象に入る。
- (EN) Expanded `tests/README.md` and `tests/README.ja.md` with an explicit policy section that codifies how host-only and interop tests differ: host tests are automated (`runtime/`, `storage/`, `network/` — run on every change, no hardware), interop tests are manual (`interop/` — run when touching `WiFiClient` / `WiFiClientSecure` / `HTTPClient` / `WiFiUDP` or anywhere host vs silicon parity is at risk). Added concrete criteria for deciding whether a new scenario belongs under `interop/` or `network/`, plus separate "add a new test" templates for the two paths. Also documented the two-tier rule: write the host-only test first, add an interop test on top only when real-silicon verification is needed — content overlap between the two is expected and beneficial.
- (JA) `tests/README.md` と `tests/README.ja.md` を改訂し、host テスト（自動／全変更で実行）と interop テスト（手動／`WiFiClient` / `WiFiClientSecure` / `HTTPClient` / `WiFiUDP` 等の host vs 実機差が出やすい領域を触ったときに実行）の使い分けを明文化。新規シナリオを `interop/` に置くべきか `network/` に置くべきかの判定基準も併記し、それぞれの追加テンプレートを分けて記載。さらに「host 先 → interop 追加」の二段構えルール（host 専用テストを先に書き、実機検証が必要な場合のみ `interop/` に追加。内容の重複は許容かつ望ましい）も明文化。
- (EN) Aligned `HTTPClient` redirect behavior with ESP32 Arduino HTTPClient v3.x: user-added request headers (`addHeader` values) are now dropped across redirect hops. Previously the host implementation preserved them, which let sketches "work on host but fail on real silicon" — exactly the gap the interop test suite is designed to catch. Sketches that need persistent headers should use `HTTPC_DISABLE_FOLLOW_REDIRECTS`, observe the 3xx, and re-issue manually with the headers they want.
- (JA) `HTTPClient` のリダイレクト挙動を ESP32 Arduino HTTPClient v3.x に揃え、`addHeader` で追加した user header をリダイレクトを跨いで破棄するように変更。従来は host 側だけ保持していたため「host では動くのに実機で壊れる」差が生じていた — interop テストスイートが本来狙って検出すべき種類のギャップ。ヘッダを跨いで使いたい場合は `HTTPC_DISABLE_FOLLOW_REDIRECTS` で 3xx を観測して手動で再リクエストする運用に。
- (EN) Added `tests/interop/http_redirect` and `tests/interop/http_chunked`. The redirect test fires `https://httpbin.org/redirect/3` with `HTTPC_FORCE_FOLLOW_REDIRECTS` and verifies the final 200 response lands on httpbin's `/get` endpoint (checked via a stable `httpbin.org/get` substring in the JSON body). The chunked test hits `https://httpbin.org/stream/3`, asserts `getSize() == -1` (the chunked-encoding code path) and counts a unique `X-Test-Tag` header value appearing exactly 3 times in the decoded body (httpbin echoes incoming headers per record). Together they validate that the redirect-following loop and chunked-decode produce identical results on the host (OpenSSL) and real ESP32 (mbedTLS) backends.
- (JA) `tests/interop/http_redirect` と `tests/interop/http_chunked` を追加。redirect テストは `https://httpbin.org/redirect/3` を `HTTPC_FORCE_FOLLOW_REDIRECTS` で叩き、3 ホップの 302 を経て httpbin の `/get` に着地したこと（JSON body 内の `httpbin.org/get` という stable な substring で確認）を検証。chunked テストは `https://httpbin.org/stream/3` を叩き、`getSize() == -1`（chunked エンコード経路を通った証拠）と、ユニークな `X-Test-Tag` ヘッダ値が decode 後 body 内にちょうど 3 回出現することを確認（httpbin はリクエストヘッダをレコードごとにエコーする）。host (OpenSSL) と ESP32 (mbedTLS) でリダイレクトループと chunked decode が同等に動くことを検証。
- (EN) Added `tests/interop/https_get`. The sketch performs an HTTPS GET against `httpbin.org/get` with a unique `X-Test-Tag` request header and the test asserts the same header value appears in the echoed JSON body — a single round-trip that exercises Wi-Fi + TLS handshake + HTTP request + response decode end-to-end on both the host (OpenSSL backend) and real ESP32 (mbedTLS backend). Host profile uses `lang-ship:host:host:tls=openssl` so the TLS backend is wired in.
- (JA) `tests/interop/https_get` を追加。`httpbin.org/get` に `X-Test-Tag` リクエストヘッダ付きで HTTPS GET し、エコーされた JSON body に同じヘッダ値が含まれることを確認 — Wi-Fi + TLS ハンドシェイク + HTTP リクエスト + レスポンス復号を 1 往復で E2E 検証。host プロファイルは `lang-ship:host:host:tls=openssl` を使い OpenSSL バックエンドを有効化、ESP32 は mbedTLS バックエンド。
- (EN) Split `platform.txt`'s `build.extra_flags` into two: `build.core_flags` carries core- and board-menu-driven defines (`-DHOST_ARDUINO=1`, the `arduino` / `sdl2` / `tls` menu fragments), `build.extra_flags` is now left empty for sketch-side overrides. The recipe patterns include both. This lets `build_config.toml` in pytest-embedded-arduino-cli inject sketch-specific defines (e.g. `-DWIFI_SSID="..."`) via `--build-property build.extra_flags=...` without clobbering the menu selections — previously `tls=openssl` was silently lost whenever a test used `build_config.toml`.
- (JA) `platform.txt` の `build.extra_flags` を 2 つに分割。`build.core_flags` に core / ボードメニュー由来の defines（`-DHOST_ARDUINO=1`、`arduino` / `sdl2` / `tls` メニューの展開）を集約し、`build.extra_flags` はスケッチ側上書き用に空欄に。レシピは両方を結合。これで pytest-embedded-arduino-cli の `build_config.toml` が `--build-property build.extra_flags=...` でスケッチ固有の defines（`-DWIFI_SSID="..."` 等）を注入してもメニュー選択が消えなくなる — 従来は `build_config.toml` を使うテストで `tls=openssl` が無効化されていた。
- (EN) Added a new `tests/interop/` category for sketches that compile and run unchanged on both the host runtime and real ESP32 silicon. Initial coverage: `interop/smoke` (boots and prints over Serial on both targets) and `interop/wifi_connect` (`WiFi.begin` / `WiFi.status` / `WiFi.localIP` parity, with credentials injected at compile time via `build_config.toml` + `.env`). Run against ESP32 with `uv run --env-file .env pytest --profile esp32 interop/`. The two existing tests verify the wiring — TLS / redirect / chunked parity tests will be added separately. Policy: interop sketches must compile cleanly without any `#ifdef` so the source itself is the parity check.
- (JA) `tests/interop/` カテゴリを新設。host ランタイムと実 ESP32 シリコンの両方でソース無改変で動作するスケッチをここに置く。初期構成: `interop/smoke`（両ターゲットで起動して Serial 出力）と `interop/wifi_connect`（`WiFi.begin` / `WiFi.status` / `WiFi.localIP` の挙動同値性。SSID/パスワードは `build_config.toml` + `.env` 経由でコンパイル時に注入）。ESP32 で実行する場合は `uv run --env-file .env pytest --profile esp32 interop/`。今回入れた 2 件は配線確認用で、TLS / リダイレクト / chunked の同値性検証は別途追加予定。方針: interop スケッチは `#ifdef` を一切使わずビルド可能であることを必須とし、ソース自体が同値性検証になる。
- (EN) Reverted `tests/runtime/smoke` to host-only — dropped the `esp32` profile from its `sketch.yaml` and removed the 5-second post-`Serial.begin()` delay, since this category targets the host runtime exclusively. The interop smoke check now lives at `tests/interop/smoke`.
- (JA) `tests/runtime/smoke` を host 専用に戻し、`sketch.yaml` から `esp32` プロファイルを削除、`Serial.begin()` 後の 5 秒 delay を撤去。このカテゴリは host ランタイムの検証専用に戻し、両プロファイル動作の確認は `tests/interop/smoke` に移動。
- (EN) Added redirect following to `HTTPClient`: `setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS / HTTPC_STRICT_FOLLOW_REDIRECTS / HTTPC_FORCE_FOLLOW_REDIRECTS)` plus `setRedirectLimit(N)` (default disabled, limit 10). Handles 301/302/303/307/308; absolute and root-relative `Location` headers; scheme transitions (`http` ⇄ `https`) when using the internal client (external-client mode returns the 3xx so the caller can switch client types); POST→GET downgrade for 301/302/303 in FORCE mode. User-added headers persist across redirects. When the hop budget is exhausted, the last 3xx status code is returned and `getLocation()` exposes the unresolved target.
- (JA) `HTTPClient` にリダイレクト自動追従を実装: `setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS / HTTPC_STRICT_FOLLOW_REDIRECTS / HTTPC_FORCE_FOLLOW_REDIRECTS)` と `setRedirectLimit(N)`（デフォルト無効・上限 10）。対応: 301/302/303/307/308、絶対 URL と root-relative `Location`、内部 client モードでの `http` ⇄ `https` の scheme 切替（外部 client モードでは 3xx をそのまま返し client 種別の差し替えは呼び出し側に委ねる）、FORCE モードでの 301/302/303 における POST→GET ダウングレード。`addHeader` で追加したヘッダはリダイレクトを跨いで保持。リダイレクト上限到達時は最後の 3xx を返し、`getLocation()` で未解決の遷移先を取得可能。
- (EN) Added `tests/network/http_redirect` covering all four behavior axes: DISABLE surfaces the 3xx code with Location, STRICT follows GET to the final body, FORCE follows a 4-hop chain via root-relative Locations, and the redirect limit cap returns the last 3xx without infinite looping.
- (JA) `tests/network/http_redirect` を追加し 4 軸を検証: DISABLE は 3xx + Location を返す、STRICT は GET の追従で最終 body を取得、FORCE は root-relative の 4 ホップチェインを辿り切る、リダイレクト上限到達時は無限ループせず最後の 3xx を返す。
- (EN) Implemented `cores/host/HTTPClient.h`. `begin(url)` auto-selects `WiFiClient` for `http://` URLs and `WiFiClientSecure` for `https://` (TLS requires the `tls=openssl` board menu option; otherwise `begin()` returns false with a hint). Supports `GET` / `POST` / `PUT` / `PATCH` / `sendRequest`, `addHeader`, `getString` decoding both `Content-Length` and `Transfer-Encoding: chunked` responses, `getStream`/`getStreamPtr`, `getSize`, `getLocation`, `setTimeout`, `setUserAgent`, `setAuthorization`. Each request opens a fresh `Connection: close` connection — no keep-alive yet. Explicitly out of scope for this pass: automatic redirect following, multipart, gzip, cookies.
- (JA) `cores/host/HTTPClient.h` を実装。`begin(url)` が `http://` は `WiFiClient`、`https://` は `WiFiClientSecure` を自動選択（TLS はボードメニュー `tls=openssl` が必要、未設定時は `begin()` が false を返しヒント出力）。`GET` / `POST` / `PUT` / `PATCH` / `sendRequest`、`addHeader`、`Content-Length` と `Transfer-Encoding: chunked` 両方の body デコード対応の `getString`、`getStream`/`getStreamPtr`、`getSize`、`getLocation`、`setTimeout`、`setUserAgent`、`setAuthorization` をサポート。毎リクエストで `Connection: close` の新規接続（keep-alive 未対応）。本実装の範囲外: リダイレクト自動追従、multipart、gzip、Cookie。
- (EN) Fixed a virtual-dispatch bug in `WiFiClient::connect(const char *host, uint16_t port)`: the name-resolution path used to call `connect(ip, port)` unqualified, which re-entered the subclass override (`WiFiClientSecure`) and caused a double TLS handshake on `WiFiClientSecure::connect(host, port)` paths. Both arms now explicitly call `WiFiClient::connect(ip, port)` so the base class never accidentally drives subclass behavior.
- (JA) `WiFiClient::connect(const char *host, uint16_t port)` の virtual ディスパッチ起因のバグを修正。名前解決パス内で未修飾の `connect(ip, port)` を呼んでおり、サブクラスのオーバーライド（`WiFiClientSecure`）に再 dispatch されて `WiFiClientSecure::connect(host, port)` 経路で TLS ハンドシェイクが二重に走っていた。両分岐とも明示的に `WiFiClient::connect(ip, port)` を呼ぶように修正し、基底クラスが偶発的にサブクラス挙動を駆動しないように。
- (EN) Added `tests/network/http_get` (covers both Content-Length and Transfer-Encoding: chunked response decoding against a local Python `http.server`) and `tests/network/https_get` (covers the internal WiFiClientSecure path against a local TLS server with a self-signed cert).
- (JA) `tests/network/http_get`（Python `http.server` を相手に Content-Length と Transfer-Encoding: chunked の両方の body デコードを検証）と `tests/network/https_get`（自己署名証明書のローカル TLS サーバを相手に内部 WiFiClientSecure 経路を検証）を追加。
- (EN) Implemented `cores/host/WiFiClientSecure.h`. Extends `WiFiClient`, uses OpenSSL when `HOST_ARDUINO_HAVE_OPENSSL` is defined (board menu `TLS=OpenSSL`), and is a benign stub when not — `connect()` returns 0 with a `[HostCore]` hint, so existing sketches that reference `WiFiClientSecure` always compile. Certificate verification is unconditionally skipped on host (real-device test responsibility); `setCACert` / `setCertificate` / `setPrivateKey` / `setInsecure` / `loadCACert(Stream&,size_t)` are no-op shims. `connected()` deliberately does *not* delegate to `WiFiClient::connected()` since the parent's raw `recv()` probe would steal a ciphertext byte out of the TLS record stream and break subsequent `SSL_read()` calls — it uses `SSL_peek` instead. Handshake is driven via `select()` on the non-blocking socket with a 5s budget.
- (JA) `cores/host/WiFiClientSecure.h` を実装。`WiFiClient` を継承し、`HOST_ARDUINO_HAVE_OPENSSL` 定義時（ボードメニュー `TLS=OpenSSL`）は OpenSSL を使用、未定義時はビルドだけ通るスタブとして振る舞い、`connect()` が 0 + `[HostCore]` ヒントを返す。証明書検証は host では常時スキップ（実機テストの責務）。`setCACert` / `setCertificate` / `setPrivateKey` / `setInsecure` / `loadCACert(Stream&,size_t)` は no-op で受理。`connected()` は親 `WiFiClient::connected()` を**呼ばない** — 親は素のソケットから 1 バイト `recv()` してキャッシュする実装で、TLS 記録の暗号バイトを盗んで後続の `SSL_read()` を壊すため、`SSL_peek` ベースで判定。ハンドシェイクは非ブロッキングソケット上で `select()` を用い、5 秒予算でループ。
- (EN) Added `tests/network/tls_secure_connect` — generates an ephemeral self-signed cert via the `openssl` CLI, spins up a Python TLS echo server, asks the sketch to `WiFiClientSecure.connect()` to it, send `PING`, and confirm the server's `PONG` is read back over the encrypted stream. Verifies handshake, read, write, `available()`, and clean shutdown end-to-end.
- (JA) `tests/network/tls_secure_connect` を追加。`openssl` CLI で自己署名証明書を生成し、Python の TLS エコーサーバを立てて、スケッチから `WiFiClientSecure.connect()` で接続、`PING` を送り、暗号化ストリーム経由で `PONG` を読み返せるかを確認。ハンドシェイク・read・write・`available()`・クリーン shutdown を E2E で検証。
- (EN) Bumped the `readLine` deadline in `tests/network/tcp_client/tcp_client.ino` from 10s to 30s. The previous value occasionally timed out when the test ran as part of a long suite where launcher startup and TCP setup accumulated delay before the CONNECT line arrived.
- (JA) `tests/network/tcp_client/tcp_client.ino` の `readLine` デッドラインを 10s → 30s に延長。長い suite の中で実行された際に、ランチャー起動や TCP セットアップの遅延が積み重なって CONNECT 行到着前にタイムアウトすることがあったため。
- (EN) Fixed Windows build failures when a sketch (or transitively, a library like OpenSSL via `<openssl/ssl.h>` → `<winsock2.h>` → `<windows.h>`) pulls in `winuser.h` or `rpcndr.h`. Those headers declare `struct INPUT` and `typedef unsigned char boolean` respectively, which collide with the Arduino API's `INPUT` macro and `bool`-based `boolean` typedef. `cores/host/Arduino.h` (and defensively `HostSocket.h`) now `#define WIN32_LEAN_AND_MEAN`, `NOUSER`, `NOGDI`, `NOMINMAX` on `_WIN32` before any other include, so subsequent inclusions of `windows.h` skip the conflicting parts. Verified manually on Windows with the `TLSProbe` example (OpenSSL 3.5.2).
- (JA) Windows でスケッチが（または OpenSSL のように間接的に `<openssl/ssl.h>` → `<winsock2.h>` → `<windows.h>` 経由で）`winuser.h` や `rpcndr.h` を取り込んだ際のビルド失敗を修正。これらは `struct INPUT` と `typedef unsigned char boolean` を宣言し、Arduino API の `INPUT` マクロ・`bool` ベースの `boolean` typedef と衝突する。`cores/host/Arduino.h`（保険として `HostSocket.h` にも）の最上段で `_WIN32` のとき `WIN32_LEAN_AND_MEAN` / `NOUSER` / `NOGDI` / `NOMINMAX` を `#define` し、後段の `windows.h` が衝突部分をスキップするように。Windows 上で `TLSProbe` example（OpenSSL 3.5.2）の手動検証で動作確認済。
- (EN) Documented alternative ways to connect to the TCP-backed `Serial` endpoint in `README.md` / `README.ja.md` (nc, socat, telnet, PuTTY `-raw` CLI, TeraTerm GUI). Useful for quick sanity checks without writing a Python connector — especially on Windows.
- (JA) TCP `Serial` エンドポイントへの代替接続手段を `README.md` / `README.ja.md` に記載（nc / socat / telnet / PuTTY の `-raw` CLI / TeraTerm GUI）。Python を書かずに動作確認したい場合の利便性向上（特に Windows）。

## 1.1.0
- (EN) Added `examples/TLSProbe` — a user-facing sketch that compiles in both `tls=disabled` and `tls=openssl` modes, prints compile-time and runtime OpenSSL versions, exercises `SSL_CTX_new`, and emits `PROBE_RESULT=PASS|FAIL`. Intended for Windows / macOS users to validate the `tls=openssl` board menu on platforms not yet covered by CI.
- (JA) `examples/TLSProbe` を追加。`tls=disabled` / `tls=openssl` のどちらでもビルドでき、コンパイル時と実行時の OpenSSL バージョンを表示し、`SSL_CTX_new` を呼び、`PROBE_RESULT=PASS|FAIL` を出力。Windows / macOS ユーザが `tls=openssl` メニューを各自の環境で検証するために用意。
- (EN) Release ZIP now bundles `examples/` (previously only `cores/`, `libraries/`, board metadata, and docs were shipped), so Boards Manager users can open the included sketches directly from the Arduino IDE.
- (JA) リリース ZIP に `examples/` を同梱するように（従来は `cores/`、`libraries/`、ボードメタデータ、ドキュメントのみ）。Boards Manager 経由のユーザが Arduino IDE から同梱スケッチを直接開けるように。
- (EN) Added a `tls` board menu option. Default is `disabled` (no change in behavior). Selecting `tls=openssl` adds `-DHOST_ARDUINO_HAVE_OPENSSL -lssl -lcrypto` so sketches can call into OpenSSL. Verified on Linux with `libssl-dev` (3.0.x). Windows MSYS2 should work with the same flags but is untested; macOS is out of scope for this minimal pass (requires `-I` / `-L` to Homebrew paths).
- (JA) ボードメニューに `tls` を追加。デフォルトは `disabled` で動作変更なし。`tls=openssl` を選ぶと `-DHOST_ARDUINO_HAVE_OPENSSL -lssl -lcrypto` が付与され、スケッチから OpenSSL を呼べる。Linux + `libssl-dev` (3.0.x) で動作確認済。Windows MSYS2 も同フラグで動く想定だが未検証、macOS は本最小実装では対象外（Homebrew パスへの `-I` / `-L` 解決が必要）。
- (EN) Added `tests/network/tls_openssl` probe sketch that reports compile-time and runtime OpenSSL versions and exercises `SSL_CTX_new` / `SSL_CTX_free` to confirm linkage.
- (JA) `tests/network/tls_openssl` を追加。コンパイル時と実行時の OpenSSL バージョンを表示し、`SSL_CTX_new` / `SSL_CTX_free` を呼んでリンクが解決していることを確認。
- (EN) Added abstract `Client` and `Server` base classes, plus `WiFiClient` (TCP) and `WiFiServer` (TCP) host implementations. `WiFiClient` shares its socket state via `shared_ptr` so values returned from `WiFiServer::available()` behave correctly when copied. Both classes expose `lastError()` and emit `[HostCore]` diagnostic hints via `HostDiag` on misuse. Extracted shared POSIX/Winsock helpers into `cores/host/HostSocket.h` (used by `WiFiUDP`, `WiFiClient`, `WiFiServer`).
- (JA) 抽象基底 `Client` と `Server` を追加し、host 向け TCP 実装として `WiFiClient` と `WiFiServer` を追加。`WiFiClient` は `shared_ptr` でソケット状態を共有するため、`WiFiServer::available()` の戻り値を値コピーしても正しく動作。両クラスとも `lastError()` を公開し、誤用時は `HostDiag` 経由で `[HostCore]` ヒントを出力。POSIX / Winsock 共通ヘルパを `cores/host/HostSocket.h` に集約（`WiFiUDP` / `WiFiClient` / `WiFiServer` で共有）。
- (EN) Added `tests/network/tcp_echo` (sketch as TCP server) and `tests/network/tcp_client` (sketch as TCP client) to cover the new classes end-to-end.
- (JA) `tests/network/tcp_echo`（スケッチを TCP サーバとして動作）と `tests/network/tcp_client`（スケッチを TCP クライアントとして動作）を追加し、新クラスの動作を E2E で確認。
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
