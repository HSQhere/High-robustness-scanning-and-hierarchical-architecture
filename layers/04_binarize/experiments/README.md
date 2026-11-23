# 步骤 4 实验：扫描线 vs Canny 与消融

## 目的

在把步骤 4 接入流水线之前，用同一批数据回答：

1. 扫描线与 Canny 双边缘融合，哪个估计器更稳、更快？
2. Canny 的“字面相邻比例”和“归一化比例向量”两种读法，差异多大？
3. 混合（hybrid）回退是否真的能消除 Canny 的“自信地错”？
4. 去噪、均值窗、闭运算核、自适应 C 各自贡献多少？
5. 原图 / 均衡图 / 二值图三路是否都必须保留？
6. C++ 实现与 Python 参考是否逐像素一致？

## 目录

```text
experiments/
├── README.md                 # 本文件
├── configs/                  # 实验配置（JSON）
│   ├── estimator_compare.json
│   ├── ablation.json
│   └── golden_compare.json
└── results/                  # 可提交的小结果
    └── <run_id>/
        ├── run_meta.json
        ├── metrics.json
        ├── per_image.csv
        └── report.md
```

`data/output/` 只放可视化中间图，默认不入 Git；`experiments/results/` 只放 JSON/CSV/Markdown。

## 数据

- 主实验：`../data/input/qr_samples/`（53 张唯一图，随仓库提交）。
- 清单：`../data/input/qr_samples/manifest.csv`（类别、划分、真值、期望文本）。
- 真值：光照 8.0 px；`clean_320=12.8`、`clean_200=8.0`、`far_120=4.8`、`far_80=3.2`、`dim/blur=9.6`；旋转按扫描线投影 `9.6/cos(θ)`；透视无精确真值。

## 变体

| 变体 | 说明 |
|---|---|
| `scanline` | 多阈值 + 1:1:3:1:1 游程 |
| `canny_strict` | 字面相邻比例 + 拟合误差 |
| `canny_norm` | 归一化比例向量（尺度无关） |
| `canny_norm_round` | `canny_norm` + 整数取整 |
| `canny_relaxed` | 放宽比例/拟合容差 |
| `hybrid` | 同时运行 `canny_norm` 与 `scanline`；相对差 ≤20% 时优先 Canny，分歧大时以 scanline 为准 |

## 运行（待实现）

```bash
# C++（M1 完成后）
apps/qrscan_eval --config layers/04_binarize/experiments/configs/estimator_compare.json \
                 --dataset qr_samples --out layers/04_binarize/experiments/results/<run_id>

# Python 参考（迁移前，旧工程）
cd <old_perception>/tests/qrv2/qr_dynamic_illumination_demo
python experiments/finder_compare.py
```

## 指标

- 模块尺度：`fail`、`ambiguous`、`error_mean/max`、`≤0.5 px`、`≤1 px`。
- 解码：original / equalized / binary / union，按 cv2、WeChat、ZXing 分列。
- 任务码合法率（文本非空 **且** `ParseTaskCode` 通过）。
- 延迟：p50/p95/p99；注明机器、OpenCV/后端版本、图像尺寸。
- 质量：`saturation_ratio`、`dark_ratio`、`missing_area_ratio`、PSNR/SSIM。

## 旧实验基线（待 C++ 复核）

| 数据集 | 变体 | 失败 | 模块误差 mean/max | 三路 union | p50 |
|---|---|---:|---|---:|---:|
| 40 光照 | scanline | 0 | 0 / 0 | 40/40 | 105–116 ms |
| 40 光照 | canny_norm | 0 | 0.06 / 0.143 | 40/40 | 23–25 ms |
| 11 基础 | scanline | 0 | 0.078 / 0.215 | 11/11 | 137–153 ms |
| 11 基础 | canny_strict | 2 | 0.83 / 6.457 | 9/11 | 19–22 ms |
| 11 基础 | canny_norm | 0 | 0.931 / 6.171 | 11/11 | 32–33 ms |
| 2 透视 | scanline | 0 | — | 1/2 | 98–109 ms |
| 2 透视 | canny_norm | 0 | — | 1/2 | 19 ms |

> 旧数据是 Python + OpenCV 4.10，不能直接当 C++/Jetson 指标；C++ 结果必须重新测。

## 结果格式

`metrics.json` 至少包含：

```json
{
  "dataset": "qr_samples",
  "subset": "illum",
  "variant": "hybrid",
  "n": 40,
  "module": {"fail": 0, "ambiguous": 0, "error_mean": 0.0, "error_max": 0.0},
  "decode": {"original": 37, "equalized": 39, "binary": 39, "union": 40},
  "latency_ms": {"p50": 0.0, "p95": 0.0},
  "quality": {"saturation_ratio_mean": 0.0, "missing_area_ratio_mean": 0.0}
}
```

`report.md` 按 [`docs/04-experiments.md`](../../../docs/04-experiments.md) §6 模板写。

## 验收

- 40 光照：hybrid 模块估计失败 0，三路 union ≥40/40。
- 11 基础：hybrid 失败 0，union ≥11/11。
- `blur_1` 不被 Canny 直接采纳；必须回退或低置信度。
- C++ 与 Python 二值图不一致像素 ≤0.1%，模块尺度差异 0（量化后）。
- 结果可复现：同一 commit + 配置 + 数据，重复运行逐像素一致。

## 注意

- 不要只报 union；单路退化会被掩盖。
- 不要用单张冒烟图报告准确率；统计必须用 53 张唯一图。
- 透视条件下两种估计器都不可靠；不要把透视失败归因于二值化。
