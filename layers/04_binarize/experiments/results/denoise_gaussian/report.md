# 步骤 4 实验报告

- run_id: `denoise_gaussian`
- dataset: `layers/04_binarize/data/input/qr_samples`
- images: 53
- variants: `hybrid`
- OpenCV: 4.14.0
- build commit: `unknown`
- output: 未保存逐图输出（hybrid 三路，其余变体仅二值图）

## 光照样本（n=40）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.06 / 0.07 | 37/38/35/39 | 2.72 / 3.35 |

## 基础条件（n=11）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.82 / 6.96 | 11/10/10/11 | 4.01 / 6.43 |

## 透视条件（n=2）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.00 / 0.00 | 1/1/1/1 | 3.61 / 3.61 |

## 全部（n=53）

| variant | fail | ambiguous | module err mean/max (px) | decode orig/eq/bin/union | p50/p95 ms |
|---|---:|---:|---|---:|---:|
| `hybrid` | 0 | 0 | 0.22 / 6.96 | 49/49/46/51 | 2.84 / 4.26 |

## 基础条件逐项模块估计（px）

| condition | gt | hybrid |
|---|---:|---:|
| `blur_1` | 9.60 | 9.62 |
| `blur_2` | 9.60 | 9.57 |
| `clean_200` | 8.00 | 8.07 |
| `clean_320` | 12.80 | 12.93 |
| `dim_140` | 9.60 | 9.64 |
| `dim_90` | 9.60 | 9.64 |
| `far_120` | 4.80 | 4.93 |
| `far_80` | 3.20 | 3.36 |
| `persp_15` | -1.00 | 7.54 |
| `persp_30` | -1.00 | 2.15 |
| `rot_15` | 9.94 | 10.54 |
| `rot_30` | 11.09 | 11.92 |
| `rot_45` | 13.58 | 6.62 |

## 异常与失败

- `rot_45_00.png` / `hybrid`: module=6.62 gt=13.58

## 说明

- 模块误差只在有真值的图上统计；透视条件无精确单模块真值。
- `decode` 为三路非空文本计数；任务码合法性尚未接入。
- `hybrid` = `canny_norm` 优先，失败/歧义/低置信度回退 `scanline`。
- 本报告由 `qrscan_eval` 自动生成，结论需人工复核。
