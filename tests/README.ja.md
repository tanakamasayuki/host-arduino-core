# tests

English: [README.md](README.md)

`host-arduino-core` のローカルテスト一式です。
[`pytest-embedded-arduino-cli`](https://pypi.org/project/pytest-embedded-arduino-cli/)
の上に作られています。`tests/` 直下の各サブディレクトリが 1 つのテスト対
象（スケッチ + `sketch.yaml` + `test_*.py`）に対応します。`smoke/` は最
小限のビルド・起動確認用で、必要に応じて同じ形式でテストを追加していきま
す。

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

## 実行

```bash
cd tests
uv run pytest -v
```

## ローカル版を使う仕組み

`sketch.yaml` ではプラットフォームを **バージョン番号なし・
`platform_index_url` なし** で指定しています：

```yaml
platforms:
  - platform: lang-ship:host
```

バージョン pin が無いと、arduino-cli はすでにインストール済みの
`lang-ship:host` をそのまま使います。sketchbook の
`hardware/lang-ship/host` symlink もこの対象になるため、ビルドはローカル
ソースツリーに対して実行されます。コアを編集して `pytest` を再実行すれば
反映されます。リリース不要です。

逆に *リリース済み* のバージョンを検証したい場合は、バージョンと index
URL を戻します：

```yaml
- platform: lang-ship:host (1.0.5)
  platform_index_url: https://tanakamasayuki.github.io/host-arduino-core/package_index.json
```

## 新しいテストを追加する

```
tests/<name>/
  <name>.ino
  sketch.yaml      # smoke/sketch.yaml をコピー
  test_<name>.py   # `dut` fixture を利用
```
