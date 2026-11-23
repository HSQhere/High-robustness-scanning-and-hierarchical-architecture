# 步骤 4 实验报告

- run_id: `step4_baseline`
- dataset: `layers/04_binarize/data/input/qr_samples`
- images: 53
- variants: `scanline`, `canny_strict`, `canny_norm`, `canny_norm_round`, `canny_relaxed`, `hybrid`
- OpenCV: 4.14.0
- build commit: `unknown`
- output: 已保存逐图输出（hybrid 三路，其余变体仅二值图）

## 光照样本（n=40）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `scanline` | 0 | 0 | 0.00 / 0.00 | 37/38/39/40 | 1.46 / 1.75 |
| `canny_strict` | 0 | 0 | 0.11 / 0.14 | 37/38/39/40 | 1.22 / 1.68 |
| `canny_norm` | 0 | 0 | 0.06 / 0.14 | 37/38/39/40 | 1.16 / 1.38 |
| `canny_norm_round` | 0 | 0 | 0.00 / 0.00 | 37/38/39/40 | 1.14 / 1.60 |
| `canny_relaxed` | 0 | 0 | 0.08 / 0.14 | 37/38/39/40 | 1.07 / 1.33 |
| `hybrid` | 0 | 0 | 0.06 / 0.14 | 37/38/39/40 | 2.32 / 2.88 |

## 基础条件（n=11）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `scanline` | 0 | 0 | 0.08 / 0.21 | 11/11/10/11 | 2.34 / 4.17 |
| `canny_strict` | 2 | 3 | 0.83 / 6.46 | 11/9/8/11 | 1.50 / 3.93 |
| `canny_norm` | 0 | 0 | 0.93 / 6.17 | 11/11/10/11 | 1.41 / 3.64 |
| `canny_norm_round` | 0 | 0 | 1.10 / 6.60 | 11/11/10/11 | 1.48 / 3.31 |
| `canny_relaxed` | 0 | 0 | 0.65 / 6.31 | 11/11/10/11 | 1.35 / 3.43 |
| `hybrid` | 0 | 0 | 0.04 / 0.19 | 11/11/10/11 | 3.42 / 5.57 |

## 透视条件（n=2）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `scanline` | 0 | 0 | 0.00 / 0.00 | 1/1/1/1 | 2.22 / 2.22 |
| `canny_strict` | 0 | 2 | 0.00 / 0.00 | 1/1/1/1 | 1.10 / 1.10 |
| `canny_norm` | 0 | 0 | 0.00 / 0.00 | 1/1/1/1 | 1.78 / 1.78 |
| `canny_norm_round` | 0 | 0 | 0.00 / 0.00 | 1/1/1/1 | 1.43 / 1.43 |
| `canny_relaxed` | 0 | 0 | 0.00 / 0.00 | 1/1/1/1 | 1.22 / 1.22 |
| `hybrid` | 0 | 0 | 0.00 / 0.00 | 1/1/1/1 | 2.78 / 2.78 |

## 全部（n=53）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `scanline` | 0 | 0 | 0.02 / 0.21 | 49/50/50/52 | 1.51 / 2.58 |
| `canny_strict` | 2 | 5 | 0.24 / 6.46 | 49/48/48/52 | 1.24 / 2.01 |
| `canny_norm` | 0 | 0 | 0.25 / 6.17 | 49/50/50/52 | 1.17 / 1.78 |
| `canny_norm_round` | 0 | 0 | 0.24 / 6.60 | 49/50/50/52 | 1.18 / 1.91 |
| `canny_relaxed` | 0 | 0 | 0.20 / 6.31 | 49/50/50/52 | 1.12 / 1.58 |
| `hybrid` | 0 | 0 | 0.06 / 0.19 | 49/50/50/52 | 2.39 / 3.70 |

## 基础条件逐项模块估计（px）

| condition | gt | scanline | canny_strict | canny_norm | canny_norm_round | canny_relaxed | hybrid |
|---|---:|---:|---:|---:|---:|---:|---:|
| `blur_1` | 9.60 | 9.62 | 3.14 | 3.43 | 3.00 | 3.29 | 9.62 |
| `blur_2` | 9.60 | 9.77 | 9.57 | 9.57 | 10.00 | 9.57 | 9.57 |
| `clean_200` | 8.00 | 8.00 | 8.00 | 8.00 | 8.00 | 8.00 | 8.00 |
| `clean_320` | 12.80 | 12.77 | 12.00 | 12.86 | 13.00 | 12.86 | 12.86 |
| `dim_140` | 9.60 | 9.62 | 9.57 | 9.57 | 10.00 | 9.57 | 9.57 |
| `dim_90` | 9.60 | 9.62 | 9.57 | 9.57 | 10.00 | 9.57 | 9.57 |
| `far_120` | 4.80 | 4.77 | fail | 4.86 | 5.00 | 4.86 | 4.86 |
| `far_80` | 3.20 | 3.23 | 3.14 | 3.14 | 3.00 | 3.14 | 3.14 |
| `persp_15` | -1.00 | 7.00 | 4.93 | 6.86 | 7.00 | 6.86 | 6.86 |
| `persp_30` | -1.00 | 2.23 | 6.21 | 4.36 | 4.00 | 5.25 | 2.23 |
| `rot_15` | 9.94 | 10.15 | 9.93 | 9.93 | 10.00 | 9.93 | 9.93 |
| `rot_30` | 11.09 | 11.23 | 11.14 | 11.07 | 11.00 | 11.14 | 11.07 |
| `rot_45` | 13.58 | 13.38 | fail | 9.79 | 10.00 | 13.07 | 13.38 |

## 异常与失败

- `blur_1_00.png` / `canny_strict`: module=3.14 gt=9.60
- `blur_1_00.png` / `canny_norm`: module=3.43 gt=9.60
- `blur_1_00.png` / `canny_norm_round`: module=3.00 gt=9.60
- `blur_1_00.png` / `canny_relaxed`: module=3.29 gt=9.60
- `clean_320_00.png` / `canny_strict`: module=12.00 gt=12.80, ambiguous
- `far_120_00.png` / `canny_strict`: fail, ambiguous
- `persp_15_00.png` / `canny_strict`: module=4.93 gt=-1.00, ambiguous
- `persp_30_00.png` / `canny_strict`: module=6.21 gt=-1.00, ambiguous
- `rot_45_00.png` / `canny_strict`: fail, ambiguous
- `rot_45_00.png` / `canny_norm`: module=9.79 gt=13.58
- `rot_45_00.png` / `canny_norm_round`: module=10.00 gt=13.58

## 说明

- 模块误差只在有真值的图上统计；透视条件无精确单模块真值。
- `decode` 为三路非空文本计数；任务码合法性尚未接入。
- `hybrid` = 同时运行 `canny_norm` 与 `scanline`；相对差 ≤20% 时优先 Canny，分歧大时以 scanline 为准。
- 本报告由 `qrscan_eval` 自动生成，结论需人工复核。
