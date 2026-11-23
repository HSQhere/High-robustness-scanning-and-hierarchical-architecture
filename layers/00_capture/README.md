# 00_capture：采集与相机预设

## 职责

- 按预设固定曝光、ISO、色温、亮度、增益等参数，保证静止扫码的可复现性。
- 采集原始帧（全分辨率 BGR），同时产生约 640×640 的检测副本。
- 记录每帧的相机参数快照、时间戳、帧号。
- 为步骤 7-1 提供多重曝光包围采集能力（预设值 / 阈值 / 下一值各一帧）。

## 输入 / 输出

| 输入 | 输出 |
|---|---|
| `CameraSettings` 预设 | `Frame`（`bgr` + `timestamp_ns` + `settings` + `status`） |
| 包围曝光请求 | `std::vector<Frame>`（3 帧，带曝光参数） |

## 对外接口（草案）

```cpp
class ICapture {
 public:
  virtual ~ICapture() = default;
  virtual Status Open(const CameraSettings& preset) = 0;
  virtual Status Capture(Frame* frame) = 0;
  virtual Status CaptureBracket(const BracketRequest& request,
                                std::vector<Frame>* frames) = 0;
  virtual Status Close() = 0;
};
```

- 所有方法返回 `Status`；失败时 `Frame` 必须为空且带诊断。
- 不在本模块做去噪、缩放、ROI 检测；640×640 副本由 `01_roi_detect` 或流水线生成。
- 相机 SDK 差异封装在实现内部，接口不暴露厂商类型。

## 数据

- `data/input/`：相机参数预设 JSON、标定文件（待采集）。
- `data/output/`：冒烟采集帧（默认不入 Git）。
- 实验：曝光/增益/色温重复性、包围曝光时间、丢帧率。

## 当前状态

骨架。相机型号、SDK、是否支持硬件 bracketing 均 `[待确认]`。

## 待确认

- [ ] 相机 SDK 与平台（Windows / Jetson）。
- [ ] 是否支持硬件触发与硬件包围曝光。
- [ ] 固定参数的现场标定流程。
