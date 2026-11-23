# 03 接口：数据契约、阶段 API 与配置

> 本文是**设计草案**，用于冻结跨模块约定；尚未生成对应头文件。
> 实现时以 `common/include/qrscan/common/` 为唯一权威，本文随接口变更同步更新。

## 1. 命名空间与版本

```cpp
namespace qrscan {
inline constexpr int kApiVersion = 1;
}
```

- 所有公开类型在 `qrscan` 下；阶段实现可放 `qrscan::binarize` 等子命名空间，但跨阶段数据结构只用 `qrscan` 顶层类型。
- 头文件路径：`#include "qrscan/common/types.hpp"`、`"qrscan/binarize/binarizer.hpp"`。
- 不兼容变更必须升 `kApiVersion`，并在 PR 中列出迁移步骤。

## 2. 状态码

```cpp
enum class Status {
  kOk = 0,
  kInvalidInput,
  kNotFound,
  kLowConfidence,
  kUnsaturatedInfoLost,
  kModelUnavailable,
  kTimeout,
  kInternalError,
};
```

约定：

- 任何阶段返回 `Status` + 可读诊断；禁止只返回空 `cv::Mat` 或空字符串表示多种失败。
- `kLowConfidence` 表示“有结果但不足以单独下结论”，调用方仍可使用结果做融合/回退。
- `kUnsaturatedInfoLost` 专指过曝/欠曝/遮挡导致信息不可逆丢失，供步骤 6 选择多重曝光或修补。

## 3. 采集与帧

```cpp
struct CameraSettings {
  int exposure_us = 0;      // 曝光时间，微秒
  int iso = 0;              // 或等价增益
  int white_balance_k = 0;  // 色温，K
  double brightness = 0.0;  // 相机亮度/数字增益
  double gain = 0.0;
  int64_t frame_duration_us = 0;
};

struct Frame {
  uint64_t frame_id = 0;
  int64_t timestamp_ns = 0;
  cv::Mat bgr;              // CV_8UC3，原始分辨率
  CameraSettings settings;
  Status status = Status::kOk;
  std::string diagnostics;
};
```

`Frame` 是只读数据；需要缩放/裁切时返回新 `cv::Mat`，不原地改。

## 4. ROI 与几何

```cpp
struct Quad {
  std::array<cv::Point2f, 4> points;  // 顺时针，从左上开始
  bool valid = false;
};

struct RoiProposal {
  Quad quad;
  float confidence = 0.0f;   // [0, 1]
  std::string detector;      // "yolov8n" / "qrdet" / "full_frame"
  cv::Rect bounding_box;     // 原图坐标，便于快速裁切
};

struct RoiImage {
  cv::Mat bgr;               // 原分辨率裁切/校正结果
  cv::Mat gray;              // 可延迟生成；空表示尚未转换
  Quad source_quad;          // 对应原图四角
  cv::Size original_size;
  double scale_from_original = 1.0;  // 若发生缩放/超分，记录比例
  Status status = Status::kOk;
};
```

坐标规则：

- 所有 `Quad`/`Rect` 默认在**原始帧坐标**；若阶段输出缩放图，必须同时给 `scale_from_original`。
- `module_px` 始终相对当前 `RoiImage.bgr` 的像素尺度；缩放后必须重新估计或按比例换算并标注来源。

## 5. 步骤 4：尺度估计与二值化

```cpp
enum class EstimatorKind {
  kScanline,   // 多阈值 + 1:1:3:1:1 游程
  kCanny,      // Canny 双边缘融合 + 9:11:27:11:9 比例
  kHybrid,     // Canny 优先，低置信度/失败回退 scanline
};

enum class DenoiseKind {
  kNone,
  kMedian3,
  kBilateral,
  kGaussian,
};

struct ScaleEstimate {
  Status status = Status::kNotFound;
  EstimatorKind method = EstimatorKind::kScanline;
  double module_px = 0.0;
  double confidence = 0.0;     // [0, 1]，由支持度、拟合误差、歧义度综合
  int support = 0;             // 候选/边缘簇支持数
  double fit_error = 0.0;      // 比例拟合误差
  bool ambiguous = false;      // 最高分箱支持度不足
  double elapsed_ms = 0.0;
  std::string diagnostics;
};

struct BinarizeConfig {
  EstimatorKind estimator = EstimatorKind::kHybrid;
  DenoiseKind denoise = DenoiseKind::kMedian3;

  // 论文复现参数；默认值来自旧实验，不是最终冻结值。
  double mean_filter_factor = 1.0;    // 均值窗 ≈ factor × module
  double close_factor = 3.0;          // 闭运算核 ≈ factor × module
  double scale_constant = 200.0;      // equalized = gray / closing × constant
  double adaptive_window_factor = 3.0;// 自适应窗 ≈ factor × module
  double adaptive_c = 0.0;            // adaptiveThreshold 的 C

  bool fallback_to_scanline = true;   // Canny 失败/歧义时是否回退
  bool return_original = true;        // 必须保留原图解码路径
  bool return_equalized = true;
  bool return_binary = true;
};

struct BinarizeQuality {
  double saturated_ratio = 0.0;       // gray >= 250 的比例
  double dark_ratio = 0.0;            // gray <= 5 的比例
  double mean_gray = 0.0;
  double std_gray = 0.0;
  double illumination_uniformity = 0.0; // 光照图平坦度，定义待实验冻结
  double local_contrast = 0.0;
  double missing_area_ratio = 0.0;    // 估计不可读面积；无掩膜时为 0
};

struct BinarizeResult {
  Status status = Status::kOk;
  cv::Mat illumination;   // CV_8UC1，闭运算估计的光照背景
  cv::Mat equalized;      // CV_8UC1
  cv::Mat binary;         // CV_8UC1，0/255
  ScaleEstimate scale;
  BinarizeQuality quality;
  double elapsed_ms = 0.0;
  std::string diagnostics;
};
```

### 5.1 阶段接口

```cpp
class IScaleEstimator {
 public:
  virtual ~IScaleEstimator() = default;
  virtual const char* Name() const = 0;
  virtual ScaleEstimate Estimate(const cv::Mat& gray) const = 0;
};

class IBinarizer {
 public:
  virtual ~IBinarizer() = default;
  virtual BinarizeResult Process(const cv::Mat& gray,
                                 const BinarizeConfig& config) const = 0;
};
```

实现约定：

- `Process` 不修改输入；所有中间图内部持有。
- 输入必须是 `CV_8UC1`；非灰度输入返回 `kInvalidInput`，由调用方先转灰度（避免隐式转换导致行为不一致）。
- 空输入/尺寸过小（例如 `< 32×32`）返回 `kInvalidInput`。
- `module_px` 非法（`<= 0` 或大于图像边长）返回 `kNotFound`/`kLowConfidence`，不抛异常。
- 两种估计器都必须可单独实例化；`kHybrid` 只是组合策略，不引入第三种算法。

## 6. 解码

```cpp
enum class DecodeStatus {
  kOk,
  kNoText,
  kInvalidTaskCode,
  kAmbiguous,
  kBackendUnavailable,
  kTimeout,
  kInternalError,
};

struct DecodeResult {
  DecodeStatus status = DecodeStatus::kNoText;
  std::string text;
  std::string backend;        // "wechat" / "zxing" / "quirc"
  double confidence = 0.0;
  uint64_t frame_id = 0;
  int frame_index = 0;
  double elapsed_ms = 0.0;
  std::string diagnostics;
};

class IDecoder {
 public:
  virtual ~IDecoder() = default;
  virtual DecodeResult Decode(const cv::Mat& image) const = 0;
};
```

任务码校验不放进 `IDecoder`：由 `08_pipeline` 注入 `ITaskCodeValidator`，避免解码模块依赖赛规。

## 7. 质量判别

```cpp
enum class Action {
  kDecodeOtherRoute,
  kRetryEstimator,
  kBackToGeometry,
  kMultiExposure,
  kInpaint,
  kSuperRes,
  kFail,
};

struct QualityReport {
  BinarizeQuality binarize;
  double sharpness = 0.0;       // Laplacian 方差
  double scale_confidence = 0.0;
  double cnn_latency_ms = 0.0;
  double cnn_uncertainty = 0.0;
  double estimated_failure_probability = 0.0;
  std::vector<Action> available_actions;
  Action recommended = Action::kDecodeOtherRoute;
  std::string reason;
};

class IQualityJudge {
 public:
  virtual ~IQualityJudge() = default;
  virtual QualityReport Evaluate(const BinarizeResult& binarize,
                                 const DecodeResult& decode) const = 0;
};
```

## 8. 阶段编排

```cpp
struct PipelineConfig {
  int max_frames = 3;
  int64_t total_budget_ms = 0;   // 0 = 不限制；最终值待实测
  BinarizeConfig binarize;
  // 其余阶段配置按节扩展
};

struct PipelineResult {
  Status status = Status::kOk;
  std::string task_code;
  std::string failure_reason;
  std::vector<DecodeResult> attempts;
  QualityReport quality;
  double elapsed_ms = 0.0;
};

class IPipeline {
 public:
  virtual ~IPipeline() = default;
  virtual PipelineResult Run(const std::vector<Frame>& frames,
                             const PipelineConfig& config) = 0;
};
```

`08_pipeline` 负责：帧预算、阶段调用顺序、回退、超时、遥测；阶段不感知全局状态。

## 9. 配置示例

```json
{
  "api_version": 1,
  "capture": {
    "exposure_us": 8000,
    "iso": 200,
    "white_balance_k": 5000,
    "brightness": 0.0
  },
  "roi": {
    "detector": "yolov8n",
    "input_size": 640,
    "confidence_threshold": 0.35
  },
  "geometry": {
    "enable_crop": true,
    "enable_super_resolution": false,
    "quiet_zone_ratio": 0.08
  },
  "restore": {
    "enable_traditional_repair": true,
    "enable_perspective_correction": true
  },
  "binarize": {
    "estimator": "hybrid",
    "denoise": "none",
    "mean_filter_factor": 1.0,
    "close_factor": 3.0,
    "scale_constant": 200.0,
    "adaptive_window_factor": 3.0,
    "adaptive_c": 0.0,
    "fallback_to_scanline": true,
    "return_original": true,
    "return_equalized": true,
    "return_binary": true
  },
  "decode": {
    "primary": "wechat",
    "fallbacks": ["zxing"],
    "max_frames": 3
  },
  "quality": {
    "saturation_threshold": 0.02,
    "dark_threshold": 0.05,
    "missing_area_threshold": 0.15,
    "weights": {
      "fail": 1.0,
      "latency": 0.3,
      "risk": 0.8
    }
  }
}
```

> 以上阈值只是**占位默认值**，必须由实验和现场标定后冻结；未冻结前不得作为验收依据。

## 10. 生命周期、线程与错误

- 阶段对象可复用；`Process`/`Estimate` 为 `const`，内部不得缓存跨帧状态。
- 并发：不同帧可在不同线程调用同一阶段对象；若实现需要缓存，必须内部加锁或改为每线程实例。
- 异常：阶段内部捕获 OpenCV/后端异常，转换为 `Status`；异常不得穿过接口。
- 所有权：`cv::Mat` 按值返回，依赖 OpenCV 引用计数；调用方不得假设与输入共享内存。
- 日志：阶段只填 `diagnostics` 字符串，不直接写日志文件；由 `08_pipeline`/`apps` 统一输出。

## 11. 待冻结项

- [ ] `BinarizeQuality.illumination_uniformity` 的具体定义与量纲。
- [ ] `confidence` 的标定方法（支持度/拟合误差/歧义度的组合权重）。
- [ ] `Action` 是否还需要 `kReCapture`（与 `kMultiExposure` 区分）。
- [ ] `PipelineConfig` 的阶段预算字段。
- [ ] 是否需要稳定的 C ABI；当前只承诺 C++ 接口。
