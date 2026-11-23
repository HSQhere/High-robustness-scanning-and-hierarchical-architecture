# 步骤 4 实验报告

- run_id: `denoise_bilateral`
- dataset: `layers/04_binarize/data/input/qr_samples`
- images: 53
- variants: `hybrid`
- OpenCV: 4.14.0
- build commit: `3e08885`
- output: 未保存逐图输出（hybrid 三路，其余变体仅二值图）

## 光照样本（n=40）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.04 / 0.14 | 37/37/39/40 | 3.34 / 4.42 |

## 基础条件（n=11）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.78 / 7.04 | 11/11/11/11 | 4.95 / 7.35 |

## 透视条件（n=2）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.00 / 0.00 | 1/1/1/1 | 4.18 / 4.18 |

## 全部（n=53）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.20 / 7.04 | 49/49/51/52 | 3.56 / 5.08 |

## 基础条件逐项模块估计（px）

| condition | gt | hybrid |
|---|---:|---:|
| `blur_1` | 9.60 | 9.77 |
| `blur_2` | 9.60 | 9.57 |
| `clean_200` | 8.00 | 8.00 |
| `clean_320` | 12.80 | 12.86 |
| `dim_140` | 9.60 | 9.57 |
| `dim_90` | 9.60 | 9.57 |
| `far_120` | 4.80 | 4.86 |
| `far_80` | 3.20 | 3.14 |
| `persp_15` | -1.00 | 7.00 |
| `persp_30` | -1.00 | 2.15 |
| `rot_15` | 9.94 | 10.00 |
| `rot_30` | 11.09 | 12.15 |
| `rot_45` | 13.58 | 6.54 |

## 异常与失败

- `rot_30_00.png` / `hybrid`: module=12.15 gt=11.09
- `rot_45_00.png` / `hybrid`: module=6.54 gt=13.58

## 说明

- 模块误差只在有真值的图上统计；透视条件无精确单模块真值。
- `decode` 为三路非空文本计数；任务码合法性尚未接入。
- `hybrid` = 同时运行 `canny_norm` 与 `scanline`；相对差 ≤20% 时优先 Canny，分歧大时以 scanline 为准。
- 本报告由 `qrscan_eval` 自动生成，结论需人工复核。
