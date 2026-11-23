# 步骤 4 实验结果索引

所有结果由 `qrscan_eval` 生成；主结果目录包含逐图输出，消融目录只保留指标。

| 目录 | 内容 | 输出图 | 说明 |
|---|---|---|---|
| [`step4_baseline/`](step4_baseline/) | 6 个估计器 × 53 张唯一图 | 是 | 主实验；`hybrid` 三路 + montage，其余变体二值图 |
| [`denoise_none/`](denoise_none/) | `hybrid` + 不额外去噪 | 否 | 当前默认 |
| [`denoise_median3/`](denoise_median3/) | `hybrid` + 中值去噪 | 否 | 精度相同，额外耗时 |
| [`denoise_bilateral/`](denoise_bilateral/) | `hybrid` + 双边去噪 | 否 | 误差变大 |
| [`denoise_gaussian/`](denoise_gaussian/) | `hybrid` + 高斯去噪 | 否 | 掉一张解码 |

## 主结果（53 张，OpenCV 4.14，`denoise=none`）

| 变体 | 失败 | 歧义 | 模块误差 mean/max (px) | 三路 union | p50/p95 (ms) |
|---|---:|---:|---|---:|---:|
| `scanline` | 0 | 0 | 0.02 / 0.21 | 52/53 | 1.51 / 2.58 |
| `canny_strict` | 2 | 5 | 0.24 / 6.46 | 52/53 | 1.24 / 2.01 |
| `canny_norm` | 0 | 0 | 0.25 / 6.17 | 52/53 | 1.17 / 1.78 |
| `canny_norm_round` | 0 | 0 | 0.24 / 6.60 | 52/53 | 1.18 / 1.91 |
| `canny_relaxed` | 0 | 0 | 0.20 / 6.31 | 52/53 | 1.12 / 1.58 |
| `hybrid` | **0** | **0** | **0.06 / 0.19** | **52/53** | 2.39 / 3.70 |

关键点：

- `hybrid` 同时跑 `canny_norm` 与 `scanline`；相对差 ≤20% 时用 Canny，分歧大时用扫描线。
- `blur_1` 上 Canny 估成 3.0–3.4 px（真值 9.6），`hybrid` 正确回退到扫描线 9.62 px。
- `rot_45` 上 Canny 估成 9.79 px，扫描线投影 13.38 px（真值 13.58），`hybrid` 用扫描线。
- 三路解码 original 49、equalized 50、binary 50，union 52；**不能只保留二值图**。
- 2 个透视条件里只有 `persp_15` 可解；透视问题应由 ROI/几何解决。

## 复现

```bash
# 主实验（会写 12 MB 逐图输出）
./build/qrscan_eval --dataset layers/04_binarize/data/input/qr_samples \
                    --out layers/04_binarize/experiments/results/step4_baseline \
                    --run-id step4_baseline

# 去噪消融（只写指标）
./build/qrscan_eval --dataset layers/04_binarize/data/input/qr_samples \
                    --out layers/04_binarize/experiments/results/denoise_none \
                    --run-id denoise_none \
                    --config layers/04_binarize/experiments/configs/denoise_none.json \
                    --variants hybrid --no-save
```

更完整的实验协议见 [`docs/04-experiments.md`](../../../../docs/04-experiments.md)。
