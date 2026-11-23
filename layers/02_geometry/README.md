# 02_geometry：ROI 重映射、裁切与超分

## 职责

- 把 `01_roi_detect` 给出的四角从检测副本反变换回原图坐标。
- 在原分辨率上裁切二维码区域，保留静区（quiet zone）。
- 可选：对模块过小的 ROI 做超分/二维码专用重构；不做生成式补全（那是 `07b_inpaint`）。
- 输出统一的 `RoiImage`，供 `03_restore`/`04_binarize` 使用。

## 输入 / 输出

| 输入 | 输出 |
|---|---|
| 原图 `Frame.bgr` | `RoiImage`（原分辨率裁切） |
| `RoiProposal.quad` | `source_quad`、`scale_from_original` |
| `GeometryConfig` | 状态 + 诊断 |

## 对外接口（草案）

```cpp
class IGeometry {
 public:
  virtual ~IGeometry() = default;
  virtual Status Remap(const cv::Mat& original,
                       const RoiProposal& proposal,
                       const GeometryConfig& config,
                       RoiImage* roi) = 0;
};
```

- 角点顺序：顺时针，从左上开始；顺序错误返回 `kInvalidInput`。
- 裁切框必须在原图内；越界时夹取并记录 `clipped=true`。
- 静区比例默认 `0.08`，可配置；不能把定位图案切掉。
- 超分关闭时必须原样返回裁切图，不能偷偷缩放。
- 输出 `scale_from_original`；后续 `module_px` 必须按此尺度解释。

## 数据与实验

- `data/input/`：ROI 角点 JSON、冒烟图。
- `data/output/`：裁切/校正可视化、超分前后对比（默认不入 Git）。
- 实验：裁切边界、插值方式（最近邻用于二值图）、超分 on/off 的模块误差与延迟、8MP 下的内存峰值。

## 当前状态

骨架。超分模型与是否启用 `[待确认]`。

## 待确认

- [ ] 超分模型选型（Real-ESRGAN、QR 专用 SR、或传统重构）。
- [ ] 是否需要在几何阶段做第一次透视校正，还是全部交给 `03_restore`。
- [ ] 输出 ROI 的目标边长策略（原尺度 / 固定 512 / 按模块数）。
