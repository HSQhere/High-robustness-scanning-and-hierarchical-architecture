# 01_roi_detect：ROI 检测

## 职责

- 输入约 640×640 的检测副本，输出二维码候选四角、置信度、检测器名称。
- 视场角过大（如 120°）或二维码距离过远时，避免在全幅 8MP 上跑重操作。
- 支持 YOLOv8n / QRDet 等模型；模型不可用时回退全幅。

## 输入 / 输出

| 输入 | 输出 |
|---|---|
| 640×640 letterbox 图 + 原图尺寸 | `std::vector<RoiProposal>`（按置信度降序） |
| 原图（可选，用于坐标反变换） | 四角坐标（原图坐标系） |

## 对外接口（草案）

```cpp
class IRoiDetector {
 public:
  virtual ~IRoiDetector() = default;
  virtual Status Detect(const cv::Mat& detection_image,
                        std::vector<RoiProposal>* proposals) = 0;
};
```

- 四角顺序固定：顺时针，从左上开始。
- 必须把 letterbox 的缩放/填充反变换回原图坐标。
- 没有候选时返回 `kNotFound`，不返回空 `RoiProposal` 冒充成功。
- 模型加载失败返回 `kModelUnavailable`；流水线可回退全幅。

## 数据

- `data/input/`：检测冒烟图（小样本）。
- `data/output/`：检测框可视化、逐图 IoU/置信度（默认不入 Git）。
- 训练/标注数据后续放入本层 `data/input/`；当前阶段未纳入仓库。

## 指标

- 召回率、IoU、中心误差、空场误检数、p50/p95 延迟。
- 8MP 下必须报告 `detect_scale` 对召回与延迟的影响。

## 当前状态

骨架。模型选型、训练数据、TensorRT 部署均 `[待确认]`。

## 待确认

- [ ] YOLOv8n 与 QRDet 是否并联，还是先单模型。
- [ ] 检测输入分辨率是否固定 640×640。
- [ ] 模型输出格式（ONNX/TensorRT engine）与后处理。
