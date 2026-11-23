# 步骤 4 实验报告

- run_id: `denoise_none`
- dataset: `layers/04_binarize/data/input/qr_samples`
- images: 53
- variants: `hybrid`
- OpenCV: 4.14.0
- build commit: `3e08885`
- output: 未保存逐图输出（hybrid 三路，其余变体仅二值图）

## 光照样本（n=40）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.06 / 0.14 | 37/38/39/40 | 3.26 / 4.10 |

## 基础条件（n=11）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.04 / 0.19 | 11/11/10/11 | 4.66 / 7.10 |

## 透视条件（n=2）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.00 / 0.00 | 1/1/1/1 | 4.11 / 4.11 |

## 全部（n=53）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.06 / 0.19 | 49/50/50/52 | 3.39 / 4.91 |

## 基础条件逐项模块估计（px）

| condition | gt | hybrid |
|---|---:|---:|
| `blur_1` | 9.60 | 9.62 |
| `blur_2` | 9.60 | 9.57 |
| `clean_200` | 8.00 | 8.00 |
| `clean_320` | 12.80 | 12.86 |
| `dim_140` | 9.60 | 9.57 |
| `dim_90` | 9.60 | 9.57 |
| `far_120` | 4.80 | 4.86 |
| `far_80` | 3.20 | 3.14 |
| `persp_15` | -1.00 | 6.86 |
| `persp_30` | -1.00 | 2.23 |
| `rot_15` | 9.94 | 9.93 |
| `rot_30` | 11.09 | 11.07 |
| `rot_45` | 13.58 | 13.38 |

## 异常与失败

- 无。

## 说明

- 模块误差只在有真值的图上统计；透视条件无精确单模块真值。
- `decode` 为三路非空文本计数；任务码合法性尚未接入。
- `hybrid` = 同时运行 `canny_norm` 与 `scanline`；相对差 ≤20% 时优先 Canny，分歧大时以 scanline 为准。
- 本报告由 `qrscan_eval` 自动生成，结论需人工复核。
