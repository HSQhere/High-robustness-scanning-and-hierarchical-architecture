// 步骤 4（光照均衡 + 自适应二值化）的公共类型与配置。
#pragma once

#include <string>

#include <opencv2/core.hpp>

#include "qrscan/common/types.hpp"

namespace qrscan::binarize {

enum class EstimatorKind {
  kScanline,
  kCanny,
  kHybrid,
};

enum class CannyVariant {
  kStrict,
  kNorm,
  kNormRound,
  kRelaxed,
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
  CannyVariant canny_variant = CannyVariant::kNorm;
  double module_px = 0.0;
  double confidence = 0.0;
  int support = 0;
  int candidates = 0;
  double fit_error = 0.0;
  bool ambiguous = false;
  double elapsed_ms = 0.0;
  std::string diagnostics;
};

struct BinarizeConfig {
  EstimatorKind estimator = EstimatorKind::kHybrid;
  CannyVariant canny_variant = CannyVariant::kNorm;
  DenoiseKind denoise = DenoiseKind::kNone;

  // 论文复现参数；默认值来自旧实验，不是官方参数。
  double mean_filter_factor = 1.0;
  double close_factor = 3.0;
  double scale_constant = 200.0;
  double adaptive_window_factor = 3.0;
  double adaptive_c = 0.0;

  bool fallback_to_scanline = true;

  bool return_original = true;
  bool return_equalized = true;
  bool return_binary = true;

  // Canny 估计器参数。
  int canny_low_threshold = 50;
  int canny_high_threshold = 150;
  int canny_close_kernel = 3;
  int canny_collapse_gap = 1;
  double canny_ratio_tolerance = 0.2;
  double canny_fit_error_max = 0.55;

  // 扫描线估计器参数。
  int scanline_median_kernel = 3;
  double scanline_scale_min = 1.25;
  double scanline_scale_max = 100.0;
  double scanline_fit_error_max = 0.48;
  double scanline_bin_width_px = 0.5;
  double scanline_adaptive_c = 3.0;
};

struct BinarizeQuality {
  double saturated_ratio = 0.0;
  double dark_ratio = 0.0;
  double mean_gray = 0.0;
  double std_gray = 0.0;
  double illumination_mean = 0.0;
  double illumination_std = 0.0;
  double illumination_cv = 0.0;   // std/mean，越小越平坦
  double local_contrast = 0.0;    // 均衡图标准差
  double sharpness = 0.0;         // 灰度 Laplacian 方差
  double missing_area_ratio = 0.0;
  double binary_dark_ratio = 0.0;
};

struct BinarizeResult {
  Status status = Status::kInvalidInput;
  cv::Mat original;       // 输入灰度的浅拷贝；默认保留
  cv::Mat illumination;
  cv::Mat equalized;
  cv::Mat binary;
  ScaleEstimate scale;
  BinarizeQuality quality;
  double elapsed_ms = 0.0;
  double denoise_ms = 0.0;
  double equalize_ms = 0.0;
  double binary_ms = 0.0;
  std::string diagnostics;
};

const char* EstimatorKindName(EstimatorKind kind);
const char* CannyVariantName(CannyVariant variant);
const char* DenoiseKindName(DenoiseKind kind);

}  // namespace qrscan::binarize
