# MALTA2 Grafana Online Monitor

```
 ███╗   ███╗ █████╗ ██╗  ████████╗ █████╗ ██████╗
 ████╗ ████║██╔══██╗██║  ╚══██╔══╝██╔══██╗╚════██╗
 ██╔████╔██║███████║██║     ██║   ███████║  ███╔═╝
 ██║╚██╔╝██║██╔══██║██║     ██║   ██╔══██║ ███╔═╝
 ██║ ╚═╝ ██║██║  ██║███████╗██║   ██║  ██║ ██████╗
 ╚═╝     ╚═╝╚═╝  ╚═╝╚══════╝╚═╝   ╚═╝  ╚═╝╚═════╝
        Corryvreckan Real-Time Beam Monitor
```

ビームテスト中の検出器・トリガー状態をリアルタイムで監視するGrafanaモニター。
corryのOnlineMonitorを改造してInfluxDBにメトリクスを流し込み、ダークテーマのGrafanaダッシュボードで可視化する。

---

## できること

| 機能 | 詳細 |
|------|------|
| **リアルタイム監視** | corry実行中に5秒ごと自動更新 |
| **RUN STATUS** | RUNNING / OFFLINE をひと目で確認 |
| **BEAM STATUS** | ビームロス検出時に WARNING に切り替わる |
| **イベントレート** | evt/s をスパークライン付きで表示 |
| **Tracks / Event** | トラッキング効率の時系列監視 |
| **Clusters / Event** | 検出器ごとのヒット数（MALTA_0/1/2 それぞれ個別タイル） |
| **Residual X/Y RMS** | アライメント品質の変化をリアルタイム追跡 |
| **Track χ²/ndof** | トラッキング品質。閾値超えで色が変わる |
| **Cluster Size Mean** | 検出器ごとのクラスターサイズ平均 |
| **ラン経過時間** | hh:mm:ss 形式で表示 |
| **PF-AR風ダッシュボード** | ダークテーマ・大型カラータイル・セクション折り畳み |
| **自動プロビジョニング** | `docker compose up -d` 一発で即使える状態になる |

---

## アーキテクチャ

```
┌─────────────────────────────────────────────────────────┐
│                      KEK / Dev PC                       │
│                                                         │
│   corry (Corryvreckan)                                  │
│   └─ OnlineMonitor (改造済み)                            │
│        │  5秒ごとに送信                                  │
│        │  ・event_rate / tracks_per_event               │
│        │  ・clusters_per_event (検出器別)                │
│        │  ・residual X/Y RMS (検出器別)                  │
│        │  ・chi2ndof_mean / cluster mean_size           │
│        │  ・elapsed_seconds / warning_active            │
│        ▼                                                │
│   ┌─────────────────────────────────┐                  │
│   │       Docker Compose            │                  │
│   │                                 │                  │
│   │  InfluxDB v2  (:8086)           │                  │
│   │  Grafana      (:3000)  ◄────────┼── ブラウザ        │
│   │  nginx        (:8080)           │                  │
│   └─────────────────────────────────┘                  │
└─────────────────────────────────────────────────────────┘
```

---

## ダッシュボード構成

```
┌─ MALTA2 Run Status ────────────────────────────────────────────┐
│ [RUNNING]  [150 evt/s]  [12345 evt]  [00:12:34]  [BEAM OK]  [χ²=1.2] │
│  緑/灰      橙スパーク    青スパーク    紫           緑/赤        緑/黄/赤  │
└────────────────────────────────────────────────────────────────┘
┌─ Detector Status — Clusters/Event ─────────────────────────────┐
│    [MALTA_0: 1.23]        [MALTA_1: 0.98]       [MALTA_2: 1.05] │
│     緑/黄/赤 大タイル        緑/黄/赤 大タイル      緑/黄/赤 大タイル  │
└────────────────────────────────────────────────────────────────┘
┌─ Alignment Quality — Residuals ────────────────────────────────┐
│  [Residual X RMS 時系列]          [Residual Y RMS 時系列]        │
└────────────────────────────────────────────────────────────────┘
┌─ Rate Monitor ─────────────────────────────────────────────────┐
│  [Event Rate 時系列]               [Clusters/Event 時系列]       │
└────────────────────────────────────────────────────────────────┘
┌─ Tracking Quality ─────────────────────────────────────────────┐
│  [χ²/ndof 時系列]                  [Cluster Size 時系列]         │
└────────────────────────────────────────────────────────────────┘
```

各セクションは `>` で折り畳める。

---

## 起動手順（このPC）

### 前提

- [ ] corryvreckan をリビルド済み（変更後は必ず `make && make install`）
- [ ] Docker / Docker Compose インストール済み

### Step 1 — Dockerグループへの追加（初回のみ）

```bash
sudo usermod -aG docker $USER
# ログアウト → ログインして反映
docker ps   # エラーなければOK
```

### Step 2 — Grafanaスタックの起動

```bash
cd /home/hinata/MALTA2/Ready_June/grafana-monitor
docker compose up -d
docker compose ps
# malta-influxdb / malta-grafana / malta-nginx が Up になればOK
```

### Step 3 — corryを実行

`template_analysis.conf` にはInfluxDB設定済み。普通に実行するだけ:

```bash
cd /home/hinata/MALTA2/Ready_June/config/2malta_dut
bash run_alignment.sh 002 001
```

### Step 4 — ブラウザで確認

```
http://localhost:3000   (admin / admin)
```

ダッシュボード `MALTA Online Monitor` が自動ロードされている。
corryが動き始めると5秒ごとにパネルが更新される。

---

## 起動手順（KEKのPC / AlmaLinux 9）

### Step 1 — ファイルをコピー

```bash
rsync -av /home/hinata/MALTA2/Ready_June/grafana-monitor/ user@kek-pc:/path/to/grafana-monitor/
```

### Step 2 — corryvreckan をビルド

```bash
cd /path/to/corryvreckan/build
make -j$(nproc) OnlineMonitor && make install
```

### Step 3 — Docker をインストール（AlmaLinux 9）

AlmaLinux 9はRHEL系なので`dnf`で入れる:

```bash
sudo dnf install -y yum-utils
sudo yum-config-manager --add-repo https://download.docker.com/linux/rhel/docker-ce.repo
sudo dnf install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin
sudo systemctl enable --now docker
sudo usermod -aG docker $USER
# ログインし直す
```

> **`docker-compose`コマンドについて**
> AlmaLinux 9でプラグイン版を入れた場合は `docker compose`（スペース区切り）を使う。
> `docker-compose`（ハイフン）は動かない場合がある。

### Step 4 — ファイアウォールの設定

AlmaLinux 9は`firewalld`が有効なのでポートを開ける:

```bash
sudo firewall-cmd --add-port=3000/tcp --permanent   # Grafana
sudo firewall-cmd --add-port=8086/tcp --permanent   # InfluxDB
sudo firewall-cmd --add-port=8080/tcp --permanent   # nginx
sudo firewall-cmd --reload
```

### Step 5 — 起動

```bash
cd /path/to/grafana-monitor
docker compose up -d
# → http://localhost:3000 (admin/admin)
```

> **SELinuxについて**
> `docker-compose.yml`のbindマウントには`:z`フラグを付けてあるのでSELinuxのままで動く。
> もし権限エラーが出たら `sudo setenforce 0` で一時的にpermissiveにして試す。

---

## 他のconfigに追加する方法

`[OnlineMonitor]` セクションに3行追記するだけ:

```ini
[OnlineMonitor]
# ... 既存の設定 ...
influxdb_url   = "http://localhost:8086"
influxdb_token = "malta-monitor-token"
auto_save_dir  = "/path/to/grafana-monitor/snapshots/"
```

---

## 停止

```bash
docker compose down          # 停止（データは残る）
docker compose down -v       # 停止 + データも全消去
```

---

## トラブルシューティング

```bash
# InfluxDBにデータが届いているか確認
docker exec malta-influxdb influx query \
  'from(bucket:"corry") |> range(start:-1m) |> limit(n:5)' \
  --org malta --token malta-monitor-token

# Grafanaのログ確認
docker compose logs grafana --tail=20

# コンテナ再起動
docker compose restart grafana
```

---

## InfluxDB 認証情報

| 項目 | 値 |
|------|-----|
| URL | `http://localhost:8086` |
| Organization | `malta` |
| Bucket | `corry` |
| Token | `malta-monitor-token` |
| Admin User | `admin` |
| Admin Password | `malta-admin-password` |

変更したい場合は `.env` を編集。

---

## 送信メトリクス一覧

| measurement | tags | field | 説明 |
|-------------|------|-------|------|
| `corry_rate` | — | `event_rate` | イベントレート [evt/s] |
| `corry_rate` | — | `tracks_per_event` | トラック数/イベント |
| `corry_rate` | — | `event_number` | 総イベント数 |
| `corry_status` | — | `elapsed_seconds` | ラン経過時間 [s] |
| `corry_status` | — | `warning_active` | 警告状態 (0=OK / 1=WARNING) |
| `corry_detector` | `detector` | `clusters_per_event` | クラスター数/イベント（検出器別） |
| `corry_residual` | `detector`, `axis=X/Y` | `mean`, `rms` | Residual 平均・RMS（検出器別・軸別） |
| `corry_tracking` | — | `chi2ndof_mean` | Track χ²/ndof 平均 |
| `corry_cluster` | `detector` | `mean_size` | クラスターサイズ平均（検出器別） |

送信間隔: **5秒ごと**（`gui_update()` の wallClock サイクルに同期）

---

*Corryvreckan OnlineMonitor — InfluxDB bridge by hinata0311*
