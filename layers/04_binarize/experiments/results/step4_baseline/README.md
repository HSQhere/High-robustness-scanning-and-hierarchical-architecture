# step4_baseline：输入 → 输出对照

本目录由 `qrscan_eval` 生成，输入为 `layers/04_binarize/data/input/qr_samples/` 的 53 张唯一图。

## 目录与命名

设输入文件为 `<stem>.png`（例如 `clean_200_00.png`，`stem=clean_200_00`）：

| 路径 | 内容 |
|---|---|
| `input_gray/<stem>.png` | 输入灰度图（与原始 PNG 同名） |
| `hybrid/<stem>_illumination.png` | 闭运算估计的光照背景 |
| `hybrid/<stem>_equalized.png` | 除法均衡图 |
| `hybrid/<stem>_binary.png` | 自适应二值图 |
| `montage/hybrid_<stem>.png` | 四联图：input / illumination / equalized / binary |
| `<variant>/<stem>_binary.png` | 其他 5 个变体的二值图（默认只存二值） |

`variant` 取值为 `scanline`、`canny_strict`、`canny_norm`、`canny_norm_round`、`canny_relaxed`、`hybrid`。

## 指标文件

| 文件 | 内容 |
|---|---|
| `metrics.json` | 分组汇总 + 逐图逐变体模块尺度/解码/质量指标 |
| `per_image.csv` | 逐图逐变体扁平表，便于 Excel/Pandas |
| `report.md` | 人读报告：汇总表、逐条件模块估计、异常列表 |
| `run_meta.json` | run_id、commit、OpenCV 版本、数据集、输出策略 |

## 关键结果

- `hybrid`：53 张 0 失败、模块误差 mean 0.06 / max 0.19 px、三路 union 52/53。
- `blur_1`：Canny 估成 3.0–3.4 px，`hybrid` 回退扫描线 9.62 px。
- `rot_45`：Canny 9.79 px，`hybrid` 用扫描线投影 13.38 px。
- 三路解码 original 49 / equalized 50 / binary 50 / union 52；不能只保留二值图。

## 复现

```bash
./build/qrscan_eval --dataset layers/04_binarize/data/input/qr_samples \
                    --out layers/04_binarize/experiments/results/step4_baseline \
                    --run-id step4_baseline
```

> `input_gray` 是处理前灰度图；`hybrid` 三路是对应同一输入的输出。所有图像均为 280×280。
