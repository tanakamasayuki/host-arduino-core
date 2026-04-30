# host-arduino-core（日本語）

日本語 | English: [README.md](README.md)

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
- `.github/workflows/release.yml`: タグ push または手動実行でパッケージを作成し、`package/` を `gh-pages` に公開し、GitHub Releases に成果物を添付します。
- `docs/requirements.ja.md`: 要件定義書。
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

## ランタイム環境変数

- `HOST_ARDUINO_CONNECT_TIMEOUT_MS`: 子プロセスが最初の TCP クライアント接続を待つ時間。既定値は `10000`。
- `HOST_ARDUINO_PARENT_WAIT_MS`: ランチャーが子プロセスの接続情報公開を待つ時間。既定値は `5000`。
- `HOST_ARDUINO_SERIAL_BUFFER_SIZE`: `Serial` 出力バッファの最大バイト数。既定値は `65536`。

接続タイムアウトまでに TCP クライアントが接続しない場合、子プロセスは終了します。接続後は、TCP ソケットが切断されるとスケッチプロセスも終了します。

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

1. 変更を `main` にコミットします。
2. `v0.1.0` のようなタグを push するか、GitHub Actions から `Build and Release Host Arduino Core` を手動実行します。
3. workflow が ZIP を作成し、`package_index.json` を更新し、`package/` を `gh-pages` に公開し、GitHub Release に ZIP と index を添付します。

## 制限事項

- 実機ハードウェアへの書き込みは行いません。
- `arduino-cli upload` は「ホスト実行ファイルを起動する」という意味です。
- `arduino-cli monitor` は非対応です。
- 実シリアルポートや仮想シリアルポートは提供しません。
- TCP ランタイムは localhost でのテスト自動化を想定しています。
- GPIO と analog API は最小スタブであり、実機の正確な再現ではありません。
- 現時点では純粋なロジックテストを対象とし、SDL2 やグラフィカルなホストシミュレーションは含みません。

不具合報告・要望は Issues までお願いします。
