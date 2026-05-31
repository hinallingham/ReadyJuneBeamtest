# MALTA2 ラボテスト オンラインモニター

Sr-90 線源を使った 3 枚重ね MALTA2 テレスコープのオンラインモニター起動手順。

---

## 構成

```
MALTA_2  (reference)   ← 上
MALTA_1  (DUT)
MALTA_0  (reference)   ← 下
```

---

## 前提

- MaltaDAQ + Malta2Module はインストール済みとする
- OS: AlmaLinux 9 または Ubuntu

---

## Step 1 — corryvreckan のインストール

```bash
git clone https://gitlab.cern.ch/corryvreckan/corryvreckan
```

---

## Step 2 — EventLoaderMALTA の配置

```bash
cd corryvreckan/src/modules
git clone https://github.com/hinallingham/CorryvreckanMALTALoadModule EventLoaderMALTA
```

---

## Step 3 — OnlineMonitor の配置

```bash
git clone https://github.com/hinallingham/Online-Monitor OnlineMonitor
```

---

## Step 4 — corry のビルド

```bash
cd corryvreckan
mkdir build && cd build
cmake ..
make -j$(nproc)
make install
```

ビルド後に確認:

```bash
ls corryvreckan/bin/corry   # あればOK
```

---

## Step 5 — PATH の設定

`~/.bashrc` に追記してパスを通す（aliasは使わない）:

```bash
echo 'export PATH="/absolute/path/to/corryvreckan/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
corry --version   # バージョンが表示されればOK
```

---

## Step 6 — このリポジトリのインストール

```bash
git clone https://github.com/hinallingham/ReadyJuneBeamtest Ready_June
```

---

## Step 7 — geometry の position を実測値に合わせる

`Ready_June/geometry/lab_test/3malta_init.conf` を編集し、実際に定規で測った z 方向間隔を入れる:

```ini
[MALTA_0]
position = 0mm, 0mm, ___mm   # 一番下のチップ（基準: 0mm でもよい）

[MALTA_1]
position = 0mm, 0mm, ___mm   # 中央チップ（DUT）

[MALTA_2]
position = 0mm, 0mm, ___mm   # 一番上のチップ
```

---

## Step 8 — output ディレクトリを作成する

`check.conf` がログや ROOT ファイルを書き込むディレクトリを事前に作成する:

```bash
mkdir -p Ready_June/config/output
```

---

## Step 9 — `check.conf` を編集する

`Ready_June/config/lab_test/check.conf` の以下の2箇所を毎回のランに合わせて書き換える:

```ini
[EventLoaderMALTA]
base_path = "/path/to/MaltaDAQ/output"  # MaltaMultiDAQ の -o で指定したディレクトリ
run_number = 1                          # MaltaMultiDAQ の -r で指定したrun番号
```

> `base_path` に入れるパスには `run_000001_0.root.root` のようなファイルが生成されるはず。
> ファイルが見つからない場合は `ls /path/to/MaltaDAQ/output/` で確認すること。

---

## Step 10 — MaltaMultiDAQ を起動する

```bash
./MaltaMultiDAQ -r 1 -c /path/to/config.txt -o /path/to/output
```

以下のファイルが生成されていることを確認:

```
/path/to/output/run_000001_0.root.root   ← MALTA_0
/path/to/output/run_000001_1.root.root   ← MALTA_1
/path/to/output/run_000001_2.root.root   ← MALTA_2
```

> 100 イベントごとに AutoSave されるので、しばらく待ってからファイルが存在するか確認する。

---

## Step 11 — corry を実行する

```bash
cd Ready_June/config/lab_test
corry -c check.conf
```

OnlineMonitor の GUI が開き、リアルタイムでヒットマップ・相関・トラックが表示される。

---

## トラブルシューティング

| 症状 | 原因 | 対処 |
|------|------|------|
| `cannot open file` エラー | `base_path` か `run_number` が間違い | MaltaDAQ の出力ファイル名と照合する |
| `output` ディレクトリがないと怒られる | Step 8 未実施 | `mkdir -p Ready_June/config/output` |
| GUI は開くがヒットが出ない | `real_data` の座標補正ずれ | `check.conf` に `real_data = true` があるか確認 |
| 起動直後に `0 entries at startup` と出る | MaltaDAQ がまだ書き込み中 | 100 イベント後に最初の AutoSave が走るので少し待つ |
| トラックが出ない | geometry の z 位置がずれている | Step 7 の position を再確認・再測定する |
