# 04_binarize：光照均衡与自适应二值化

> 当前开发重点。对应任务描述第 4 步：用闭运算估计光照背景、除法均衡，再做自适应二值化；噪声大时先滤波。尺度估计器支持**扫描线 / Canny 双边缘融合**切换，并保留实验对照。

## 1. 职责与范围

**做**：

- 输入单通道 8U 的二维码 ROI。
- 可选去噪。
- 估计二维码模块像素尺度 `module_px`。
- 用模块尺度做均值滤波、闭运算光照估计、除法均衡。
- 用模块尺度做自适应均值二值化。
- 输出原图 / 均衡图 / 二值图三路，以及尺度估计、质量指标、耗时、诊断。

**不做**：

- 不做 ROI 检测、几何裁切、透视校正（分别属于 `01`/`02`/`03`）。
- 不做解码、任务码校验（属于 `05`）。
- 不做多重曝光融合（`07a`）或 CNN 修补（`07b`）。
- 不输出毫米；不做物理标定。

## 2. 算法来源

- 论文：R. Chen et al., *A Fast Adaptive Binarization Method for QR Code Images Based on Dynamic Illumination Equalization*, Electronics 2023, 12(19), 4134. DOI `10.3390/electronics12194134`。
- 旧实现：
  - `perception/src/qrv2/qrv2_preprocess.*`（通用预处理框架）；
  - `perception/tests/qrv2/qr_dynamic_illumination_demo/qr_dynamic_illumination.py`（论文主链 Python 复现）；
  - `perception/tests/qrv2/qr_dynamic_illumination_demo/experiments/finder_compare.py`（扫描线 vs Canny 对照）。
- **不是作者官方代码**：论文未公开全部底层参数，本模块是算法级复现，不承诺逐像素一致。

### 论文步骤与实现的对应

| 论文步骤 | 本模块实现 | 说明 |
|---|---|---|
| 灰度 + 中值滤波 + 闭运算 + 直方图均衡 | 多阈值扫描线 / Canny 边缘融合 | 用 `1:1:3:1:1` 定位图案游程估计尺度 |
| 由定位图案得 `Size` | `Size = 3 × module_px` | 中心黑块宽约 3 模块 |
| 对输入做 Gaussian/均值滤波 | 均值滤波，窗口 ≈1 模块 | 论文 §3.2 与 Algorithm 1 不一致；旧实验 1 模块优于 3 模块 |
| 以 `Size` 为结构元做闭运算 | 闭运算核 ≈`3 × module_px` | 满足 `b > win` |
| `input ÷ closing × 200` | 同式，除法用原图灰度 | 一致 |
| 以 `Size` 为窗口自适应二值化 | 自适应均值阈值，窗口 ≈`3 × module_px`，`C=0` | 一致 |

## 3. 输入 / 输出

| 输入 | 输出 |
|---|---|
| `cv::Mat gray`（`CV_8UC1`） | `BinarizeResult` |
| `BinarizeConfig` | `illumination` / `equalized` / `binary` 三路 |
| — | `ScaleEstimate`（`module_px`、置信度、支持度、拟合误差、歧义） |
| — | `BinarizeQuality`（饱和/暗区/对比度等） |
| — | `Status` + 诊断 + 耗时 |

**硬约束**：必须同时返回原图、均衡图、二值图三路。旧实验在 40 张光照样本上单路最高 39/40，三路并集 40/40；不能只保留二值图。

## 4. 尺度估计器

### 4.1 扫描线（`scanline`）

1. `medianBlur(gray, 3)`；`equalizeHist`。
2. 生成多张二值掩膜：
   - 原图中值图的 Otsu；
   - 均衡图的 Otsu；
   - 均衡图的多尺度自适应均值阈值（`block = odd(min(H,W)/12 × {0.65, 1.0, 1.5})`，`C=3`）。
3. 对每张掩膜逐行、逐列做游程；找连续 5 段 `黑/白/黑/白/黑`。
4. 最小二乘拟合尺度：`scale = dot(w, [1,1,3,1,1]) / dot([1,1,3,1,1],[1,1,3,1,1])`。
5. 过滤：`1.25 ≤ scale ≤ 100`，`mean(|w/scale - expected|) ≤ 0.48`。
6. 按 0.5 px 分箱；`score = support / (0.05 + median_error)^1.2`；取最高分箱，簇内中位数为 `module_px`。
7. 输出 `candidate_count`、`cluster_support`、`median_fit_error`、`threshold_views`。

**特点**：在旧实验的 40 张光照集上模块估计全为 8.0 px；11 个基础条件 0 失败、最大误差 0.215 px。Python 实现慢（约 105–153 ms），主要开销是逐行扫描；C++ 需优化。

### 4.2 Canny 双边缘融合（`canny`）

1. `medianBlur(gray, 3)`；`close(3×3)`；`equalizeHist`。
2. `Canny(close, 50, 150) | Canny(equ, 50, 150)`。
3. 每行/列把相邻边缘像素合并为一个边缘点（`gap=1`）；连续 6 个点取 5 个间距 `P1…P5`。
4. 比例筛选：
   - **strict**：逐条检查 `P1/P2≈9/11`、`P2/P3≈11/27`、`P3/P4≈27/11`、`P4/P5≈11/9`，`T=0.2`，拟合误差 ≤0.55；
   - **norm**：`[P1…P5]/ΣP` 与 `[9,11,27,11,9]/67` 逐项比较，最大偏差 <0.2（尺度无关）；
   - **relaxed**：放宽容差，仅作对照。
5. `module = (P1+…+P5) / 7`（总跨度/7）。
6. 窗口限制 `14 < win < IS/3`，`IS=(W+H)/2`；按 0.5 px 分箱、支持度/误差打分取中位数。
7. `support < 2` 标记 `ambiguous`；可选整数取整（只对 `center/3` 必要，`total/7` 不需要）。

**特点**：Python 下比扫描线快 4.5–7 倍；但强模糊 `blur_1` 上所有 Canny 变体都估成 3.0–3.4 px（真值 9.6），会“自信地错”。必须加置信度门控 + 扫描线回退。

### 4.3 混合（`hybrid`，默认）

```text
1. 同时运行 canny_norm 与 scanline；
2. 两者都有效且相对差 <=20% → 采用 Canny（通常更稳、更快）；
3. 两者都有效但分歧大 → 采用 scanline（旧实验里 Canny 在强模糊上会“自信地错”）；
4. 只有一路有效 → 采用有效的一路；
5. 两者都失败 → 返回 kNotFound。
```

**默认策略**：C++ 实测 `hybrid` 在 53 张唯一图上 0 失败、模块误差 mean 0.06 / max 0.19 px、三路 union 52/53；`blur_1` 与 `rot_45` 都正确回退到扫描线。默认估计器暂定 `hybrid`，仍需真实 8MP ROI 复核。配置项 `fallback_to_scanline` 保留用于未来改成“Canny 优先、失败回退”的轻量模式。

## 5. 光照均衡与二值化

```text
mean_win   = odd(max(3, mean_filter_factor × module_px))
close_win  = odd(max(module_px + 2, close_factor × module_px), 5)
smoothed   = blur(gray, mean_win)
illum      = morphologyEx(smoothed, MORPH_CLOSE, rect(close_win))
equalized  = clip(gray / max(illum, 1) × scale_constant, 0, 255)
adaptive_win = odd(adaptive_window_factor × module_px)
adaptive_win = clamp(adaptive_win, 3, min(H,W) - 1, odd)
binary     = adaptiveThreshold(equalized, 255, ADAPTIVE_THRESH_MEAN_C,
                               THRESH_BINARY, adaptive_win, adaptive_c)
```

注意：

- 除法用**原图灰度**，不是平滑图；这是论文公式。
- `adaptive_win` 必须为奇数且小于图像短边；越界时夹取并记录。
- 二值图输出 `0/255`，不返回 `0/1`。
- 若 `module_px` 不可用，返回 `kNotFound`，不猜测窗口。

## 6. 配置参数

| 字段 | 默认 | 说明 |
|---|---:|---|
| `estimator` | `hybrid` | `scanline` / `canny` / `hybrid` |
| `denoise` | `none` | `none` / `median3` / `bilateral` / `gaussian`；默认不额外去噪，噪点多时再开 |
| `mean_filter_factor` | 1.0 | 均值窗相对模块尺度 |
| `close_factor` | 3.0 | 闭运算核相对模块尺度 |
| `scale_constant` | 200.0 | 均衡公式常数 |
| `adaptive_window_factor` | 3.0 | 自适应窗相对模块尺度 |
| `adaptive_c` | 0.0 | 自适应阈值偏置 |
| `fallback_to_scanline` | true | Canny 失败/歧义时回退 |
| `return_original` | true | 必须保留原图路 |
| `return_equalized` | true | 保留均衡图路 |
| `return_binary` | true | 保留二值图路 |
| Canny 低/高阈值 | 50 / 150 | 论文未公开，工程选择 |
| Canny 边缘合并 gap | 1 | 相邻边缘像素合并 |
| 比例容差 `T` | 0.2 | strict/norm |
| 拟合误差上限 | 0.55 | strict |
| 尺度范围 | 1.25–100 px | 过滤 |
| 分箱 | 0.5 px | 聚类 |
| 最小支持度 | 2 | 低于则 ambiguous |

> 所有默认值只是旧实验的工程选择，不是论文官方参数；冻结前必须有 C++/真实 ROI 数据支撑。

## 7. 接口

见 [`docs/03-interfaces.md`](../../docs/03-interfaces.md) §5。核心：

```cpp
class IScaleEstimator {
 public:
  virtual ScaleEstimate Estimate(const cv::Mat& gray) const = 0;
};

class IBinarizer {
 public:
  virtual BinarizeResult Process(const cv::Mat& gray,
                                 const BinarizeConfig& config) const = 0;
};
```

实现拆分建议：

```text
src/
├── scale_estimator_scanline.cpp
├── scale_estimator_canny.cpp
├── scale_estimator_hybrid.cpp
├── illumination_equalizer.cpp
├── adaptive_binarizer.cpp
├── binarizer.cpp              # 门面：去噪 → 估计 → 均衡 → 二值化 → 质量
└── quality_metrics.cpp
```

## 8. 数据

| 数据集 | 位置 | 用途 |
|---|---|---|
| `qr_samples` | `data/input/qr_samples/` | 53 张唯一图：40 光照 + 11 基础 + 2 透视；随仓库提交 |

当前阶段需要的图像全部在本模块内，不依赖外部路径、环境变量或数据集注册表。`data/input/qr_samples/manifest.csv` 记录文件名、类别、划分、真值和期望文本。

## 9. 实验计划

### E4.1 估计器对照

变体：`scanline`、`canny_strict`、`canny_norm`、`canny_norm_round`、`canny_relaxed`、`hybrid`。
指标：模块误差 mean/max、失败、歧义、三路解码、union、p50/p95。

C++ 实测（2026-11-23，OpenCV 4.14，53 张唯一图，`denoise=none`）：

- 40 光照：三路 union 均 40/40；扫描线模块误差 0；`canny_norm` 0.06 px。
- 11 基础：扫描线 0 失败、max 0.21 px；`canny_strict` 失败 2；`blur_1` 所有 Canny 变体估错。
- 2 透视：两者都不可靠，只统计解码。
- `hybrid`：0 失败、误差 mean 0.06 / max 0.19 px、union 52/53；`blur_1` 与 `rot_45` 正确回退扫描线。
- 完整报告：[`experiments/results/step4_baseline/report.md`](experiments/results/step4_baseline/report.md)。

### E4.2 消融

去噪（none/median3/bilateral/gaussian）、均值窗（1×/3×）、闭运算核（2×/3×/4×）、缩放常数（180/200/220）、自适应 C（0/3/5）、阈值方式（mean/gaussian）。每次只变一个因子。

已跑的去噪消融（`hybrid`，53 张，`--no-save`）：

| denoise | 模块误差 mean | union | 备注 |
|---|---:|---:|---|
| none | 0.06 px | 52/53 | 当前默认 |
| median3 | 0.06 px | 52/53 | 等精度，额外耗时 |
| bilateral | 0.20 px | 52/53 | 误差变大 |
| gaussian | 0.22 px | 51/53 | 掉一张 |

结果目录：`experiments/results/denoise_{none,median3,bilateral,gaussian}/`。

### E4.3 三路必要性

分别统计 original / equalized / binary 的失败集合，验证并集收益；给出每路贡献与耗时。
当前 53 张：original 49、equalized 50、binary 50，union 52；三路缺一不可。

### E4.4 真实 ROI

后续阶段采集实拍 ROI 后，重点看局部镜面高光、半透明遮罩、实体遮挡、色温差异、缩放；记录 `saturation_ratio`/`missing_area_ratio` 与可救性的关系。当前仓库未纳入实拍视频。

### E4.5 C++ / Python 黄金对照

模块尺度差异 0（整数/半像素量化）；浮点容差 ≤1e-3 px；二值图不一致像素 ≤0.1%；中间图 PSNR ≥50 dB 或逐像素容差 ≤1。

### E4.6 与解码联合

固定解码器，比较“只用原图”“三路”“加三帧”的端到端任务码合法率与耗时增量。当前只统计 `cv2.QRCodeDetector` 非空文本；任务码校验在步骤 5 接入。

## 10. 迁移计划

| 阶段 | 内容 | 完成标志 |
|---|---|---|
| P0 | 冻结 `BinarizeConfig`/`ScaleEstimate`/`BinarizeResult` | 头文件评审通过 |
| P1 | 扫描线 C++ 移植 | 与 Python 逐图模块尺度一致 |
| P2 | Canny 变体 + hybrid 回退 | 支持三种模式切换 |
| P3 | 光照均衡/二值化 C++ | 三路图与 Python 一致 |
| P4 | CLI + 黄金测试 | 单图/批量可跑 |
| P5 | 53 张实验 + 报告 | 复现旧表并解释差异 |
| P6 | 接入 `08_pipeline` | 端到端静态流程可跑 |

## 11. 验收标准

- 40 张光照：hybrid 模块估计失败 0，三路 union ≥40/40。
- 11 基础：hybrid 失败 0，union ≥11/11。
- `blur_1` 不得被 Canny 直接采纳；必须回退或低置信度。
- 同输入 + 同配置逐像素一致。
- 280×280 全流程 p50 ≤5 ms；1MP ROI p50 ≤15 ms（暂定，Jetson 实测后冻结）。
- 无警告编译；单元/黄金测试通过；失败路径有明确 `Status`。

## 12. 已知限制

1. 透视下尺度估计不可靠；应由 ROI/几何解决，不是本模块。
2. 强模糊会让 Canny “自信地错”；必须依赖 hybrid 回退。
3. 局部饱和高光不可逆；本模块只能报告 `saturation_ratio`，不能恢复信息。
4. 论文参数未公开；当前是工程复现，换参数可能改变结论。
5. 旧数据是 280×280 合成图 + 少量 1280×800 实拍，不是 8MP ROI；未验证前不得声称通用提升。
6. 二值图不能作为唯一输出；必须保留原图/均衡图路。

## 13. 当前状态

- C++ 模块、`qrscan_cli`、`qrscan_eval`、单元测试和 53 张实验均已完成。
- 实验输出：`experiments/results/step4_baseline/`（三路图、montage、metrics、per-image CSV、report）。
- 去噪消融：`experiments/results/denoise_{none,median3,bilateral,gaussian}/`（仅指标）。
- 下一步：接入 `08_pipeline`（P6），并在真实 8MP ROI 上复测默认值。

## 14. 待确认

- [ ] 默认估计器是否从 `scanline` 切到 `hybrid`；需 C++ + 真实 ROI 数据。
- [ ] 去噪默认用中值还是双边；旧实验两者解码等价，但延迟不同。
- [ ] 均值窗 1× 还是 3×；旧实验 1× 更好，但需更多真实样本。
- [ ] 是否需要整数取整；`total/7` 不需要，`center/3` 需要。
- [ ] 质量指标 `illumination_uniformity` 的定义。
