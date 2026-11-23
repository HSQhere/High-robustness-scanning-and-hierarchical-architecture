// 步骤 4 的公开接口。
#pragma once

#include <memory>

#include "qrscan/common/binarize_types.hpp"

namespace qrscan::binarize {

class IScaleEstimator {
 public:
  virtual ~IScaleEstimator() = default;
  virtual const char* Name() const = 0;
  virtual ScaleEstimate Estimate(const cv::Mat& gray,
                                 const BinarizeConfig& config) const = 0;
};

class ScanlineEstimator final : public IScaleEstimator {
 public:
  const char* Name() const override;
  ScaleEstimate Estimate(const cv::Mat& gray,
                         const BinarizeConfig& config) const override;
};

class CannyEstimator final : public IScaleEstimator {
 public:
  const char* Name() const override;
  ScaleEstimate Estimate(const cv::Mat& gray,
                         const BinarizeConfig& config) const override;
};

class HybridEstimator final : public IScaleEstimator {
 public:
  const char* Name() const override;
  ScaleEstimate Estimate(const cv::Mat& gray,
                         const BinarizeConfig& config) const override;
};

std::unique_ptr<IScaleEstimator> CreateScaleEstimator(EstimatorKind kind);

// 对外门面：去噪 → 尺度估计 → 光照均衡 → 自适应二值化 → 质量指标。
class Binarizer {
 public:
  BinarizeResult Process(const cv::Mat& gray, const BinarizeConfig& config) const;
};

// 独立入口，便于实验与单元测试。
ScaleEstimate EstimateScanline(const cv::Mat& gray, const BinarizeConfig& config);
ScaleEstimate EstimateCanny(const cv::Mat& gray, const BinarizeConfig& config,
                            CannyVariant variant);
ScaleEstimate EstimateHybrid(const cv::Mat& gray, const BinarizeConfig& config);

// 配置解析：读取 JSON 中的 "binarize" 节；失败时保留默认值并返回 false。
bool LoadBinarizeConfig(const std::string& path, BinarizeConfig* config,
                        std::string* error);

}  // namespace qrscan::binarize
