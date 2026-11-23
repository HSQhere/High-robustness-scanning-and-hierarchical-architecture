# 07b_inpaint：受损二维码 CNN 修补

## 职责

- 对局部损坏/遮挡的二维码 ROI 做 CNN 修补，使其重新可解码。
- 输出修补图 + 损伤掩膜 + 不确定性/重建残差。
- 仅在“局部损坏且未饱和”时启用；过曝/欠曝优先走 `07a_multiexposure`。
- 修补结果必须当作“生成观测”，不能与真实像素等价。

## 输入 / 输出

| 输入 | 输出 |
|---|---|
| 受损 `RoiImage` | 修补后的 `RoiImage` |
| `damage_mask` | `inpaint_mask`、`uncertainty_map` |
| `InpaintConfig` | `Status`、耗时、诊断 |

## 对外接口（草案）

```cpp
class IInpainter {
 public:
  virtual ~IInpainter() = default;
  virtual Status Inpaint(const RoiImage& input,
                         const cv::Mat& damage_mask,
                         const InpaintConfig& config,
                         RoiImage* output,
                         cv::Mat* uncertainty) = 0;
};
```

- 模型缺失返回 `kModelUnavailable`，可回退 `03_restore` 的传统修补。
- 掩膜为空时不应调用；返回 `kInvalidInput`。
- 不确定性高于阈值时，结果标记 `kLowConfidence`，不得单独用于接受任务码。
- 修补后必须重新解码并做任务码校验；建议要求第二帧独立确认。

## 数据与实验

- `data/input/`：合成遮挡/损伤样本（已知真值）+ 实拍遮挡样本。
- `data/output/`：修补前后、掩膜、不确定性图（默认不入 Git）。
- 实验：
  - 合成损伤：PSNR/SSIM、模块错误率、解码成功率；
  - 实拍遮挡：解码成功率、错误接受率；
  - 幻觉率：修补后解出但与真值不符的比例；
  - 延迟：模型加载/推理 p50/p95；
  - 与多重曝光的代价对比。

## 当前状态

实时接口仍为骨架。另有[格点级恢复 CNN 离线参考](experiments/cell_reference.md)：在已对齐的二维码格网上估计黑白和不确定程度，不替代像素级修补接口。模型选型、训练数据、Jetson 部署 `[待确认]`。

## 待确认

- [ ] 用哪个修补模型（通用 inpainting vs QR 专用）。
- [ ] 损伤掩膜来源（分割模型/传统 CV/人工）。
- [ ] 允许的“生成比例”上限；超过则直接失败。
- [ ] 是否必须第二帧确认才能接受修补结果。
