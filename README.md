# ReadyJune Beamtest — MALTA2 Telescope Analysis

```
  ____                _         _                  
 |  _ \ ___  __ _  __| |_   _  | |_   _ _ __   ___ 
 | |_) / _ \/ _` |/ _` | | | | | | | | | '_ \ / _ \
 |  _ <  __/ (_| | (_| | |_| | | | |_| | | | |  __/
 |_| \_\___|\__,_|\__,_|\__, | |_|\__,_|_| |_|\___|
                         |___/
   MALTA2 Telescope  |  Corryvreckan  |  KEK PF-AR
```

広島大学クォーク物理研究室による、KEK PF-ARビームテスト用MALTA2テレスコープ解析パイプライン。
Corryvreckan (corry) を使ったアライメント・トラッキング・解析の自動化スクリプト一式。

---

## テレスコープ構成

| 構成名 | 検出器数 | 説明 |
|--------|----------|------|
| `2malta_dut` | 3 (MALTA_0/1/2) | 2枚リファレンス + 1枚DUT |
| `3malta_ref` | 3 (MALTA_0/1/2) | 3枚リファレンス（垂直入射） |
| `3malta_ref_30tilt` | 3 (MALTA_0/1/2) | 3枚リファレンス（30°チルト） |
| `3malta_ref_60tilt` | 3 (MALTA_0/1/2) | 3枚リファレンス（60°チルト） |

---

## ディレクトリ構成

```
Ready_June/
├── config/
│   ├── 2malta_dut/          # DUT構成の設定・スクリプト
│   ├── 3malta_ref/          # リファレンス構成（垂直入射）
│   ├── 3malta_ref_30tilt/   # リファレンス構成（30°チルト）
│   └── 3malta_ref_60tilt/   # リファレンス構成（60°チルト）
│       ├── run_alignment.sh      # メインパイプラインスクリプト
│       ├── template_align.conf   # アライメント設定テンプレート
│       ├── template_prealign.conf
│       ├── template_mask.conf    # ペデスタルマスク設定
│       └── template_analysis.conf  # 解析設定（InfluxDB連携含む）
│
├── geometry/
│   ├── 2malta_dut/
│   ├── 3malta_ref/
│   ├── 3malta_ref_30tilt/
│   └── 3malta_ref_60tilt/
│       ├── *_init.conf          # 初期ジオメトリ
│       ├── *_ped_masked.conf    # ペデスタルマスク後
│       ├── *_prealigned.conf    # プレアライメント後
│       └── *_aligned.conf       # 最終アライメント済み
│
├── DAQ/
│   ├── check_full_qc.C          # テレスコープQCマクロ (ROOT)
│   └── check_full_DUT_qc.C      # DUT QCマクロ (ROOT)
│
├── grafana-monitor/             # リアルタイムオンラインモニター
│   └── README.md                # → 詳細はこちらを参照
│
├── data/                        # スクリーンショット・参考データ
└── output/                      # corry出力先
```

---

## パイプライン概要

`run_alignment.sh` を実行すると以下の4フェーズが自動で走る:

```
Phase 1: Pedestal Masking
  └─ ノイズピクセルを検出してマスクファイルを生成

Phase 2: Prealignment
  └─ 粗いアライメント（スペーシャル相関で初期位置補正）

Phase 3: Millepede Global Alignment
  └─ 高精度アライメント（全検出器を同時最適化）

Phase 3.5: Physics Analysis
  └─ アライメント済みジオメトリでトラッキング・DUT解析

Phase 4: QC & Discord通知
  └─ ROOTマクロでQCプロット生成 → Discordに自動送信
```

---

## 実行方法

```bash
cd config/<構成名>/
bash run_alignment.sh <RUN_NUMBER> <PED_RUN>

# 例: 2malta_dut構成、Runナンバー002、Pedestal Run001
bash run_alignment.sh 002 001
```

---

## オンラインモニター

ビームテスト中のリアルタイム監視は `grafana-monitor/` を使う。
Corryvreckan OnlineMonitorを改造してInfluxDB + Grafanaにメトリクスを流し込む。

```bash
cd grafana-monitor/
docker compose up -d
# → http://localhost:3000 (admin/admin)
```

監視できる項目:
- イベントレート / トラック数 / クラスター数（検出器別）
- Residual X/Y RMS（アライメント品質）
- Track χ²/ndof / クラスターサイズ
- ビームステータス（BEAM OK / WARNING）

詳細 → [`grafana-monitor/README.md`](grafana-monitor/README.md)

---

## 依存関係

| ツール | 用途 |
|--------|------|
| [Corryvreckan](https://gitlab.cern.ch/corryvreckan/corryvreckan) | トラッキング・アライメントフレームワーク |
| ROOT | QCマクロ・ヒストグラム解析 |
| Docker / Docker Compose | Grafanaモニタースタック |
| curl | Discord通知・InfluxDB書き込み |

---

## 作者

Hinata Nakamura — Quark Physics Laboratory, Hiroshima University
