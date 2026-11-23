// 步骤 4 门面：去噪 → 尺度估计 → 光照均衡 → 自适应二值化 → 质量指标。
#include "qrscan/binarize/binarizer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "internal.hpp"

namespace qrscan::binarize {

const char* EstimatorKindName(EstimatorKind kind) {
  switch (kind) {
    case EstimatorKind::kScanline:
      return "scanline";
    case EstimatorKind::kCanny:
      return "canny";
    case EstimatorKind::kHybrid:
      return "hybrid";
  }
  return "unknown";
}

const char* CannyVariantName(CannyVariant variant) {
  switch (variant) {
    case CannyVariant::kStrict:
      return "strict";
    case CannyVariant::kNorm:
      return "norm";
    case CannyVariant::kNormRound:
      return "norm_round";
    case CannyVariant::kRelaxed:
      return "relaxed";
  }
  return "unknown";
}

const char* DenoiseKindName(DenoiseKind kind) {
  switch (kind) {
    case DenoiseKind::kNone:
      return "none";
    case DenoiseKind::kMedian3:
      return "median3";
    case DenoiseKind::kBilateral:
      return "bilateral";
    case DenoiseKind::kGaussian:
      return "gaussian";
  }
  return "unknown";
}

namespace {

double MillisecondsSince(const std::chrono::steady_clock::time_point& start) {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - start)
      .count();
}

cv::Mat ApplyDenoise(const cv::Mat& gray, DenoiseKind kind) {
  cv::Mat output;
  switch (kind) {
    case DenoiseKind::kNone:
      return gray;
    case DenoiseKind::kMedian3:
      cv::medianBlur(gray, output, 3);
      return output;
    case DenoiseKind::kBilateral:
      cv::bilateralFilter(gray, output, 5, 50.0, 50.0);
      return output;
    case DenoiseKind::kGaussian:
      cv::GaussianBlur(gray, output, cv::Size(3, 3), 0.0);
      return output;
  }
  return gray;
}

void ComputeQuality(const cv::Mat& gray, const cv::Mat& illumination,
                    const cv::Mat& equalized, const cv::Mat& binary,
                    BinarizeQuality* quality) {
  const double total = static_cast<double>(gray.total());
  if (total <= 0.0) {
    return;
  }

  cv::Scalar mean;
  cv::Scalar stddev;
  cv::meanStdDev(gray, mean, stddev);
  quality->mean_gray = mean[0];
  quality->std_gray = stddev[0];

  cv::Mat mask;
  cv::compare(gray, 250, mask, cv::CMP_GE);
  quality->saturated_ratio = cv::countNonZero(mask) / total;
  cv::compare(gray, 5, mask, cv::CMP_LE);
  quality->dark_ratio = cv::countNonZero(mask) / total;

  if (!illumination.empty()) {
    cv::meanStdDev(illumination, mean, stddev);
    quality->illumination_mean = mean[0];
    quality->illumination_std = stddev[0];
    quality->illumination_cv =
        mean[0] > 1e-9 ? stddev[0] / mean[0] : 0.0;
  }
  if (!equalized.empty()) {
    cv::meanStdDev(equalized, mean, stddev);
    quality->local_contrast = stddev[0];
  }
  if (!binary.empty()) {
    cv::compare(binary, 0, mask, cv::CMP_EQ);
    quality->binary_dark_ratio = cv::countNonZero(mask) / total;
  }

  cv::Mat laplacian;
  cv::Laplacian(gray, laplacian, CV_64F);
  cv::meanStdDev(laplacian, mean, stddev);
  quality->sharpness = stddev[0] * stddev[0];
  quality->missing_area_ratio = 0.0;  // 没有掩膜时不伪造
}

void EqualizeIllumination(const cv::Mat& gray, double module_px,
                          const BinarizeConfig& config,
                          cv::Mat* illumination, cv::Mat* equalized) {
  const int mean_win =
      detail::OddAtLeast(config.mean_filter_factor * module_px, 3);
  cv::Mat smoothed;
  cv::blur(gray, smoothed, cv::Size(mean_win, mean_win));

  const int close_win = detail::OddAtLeast(
      std::max(module_px + 2.0, config.close_factor * module_px), 5);
  const cv::Mat kernel =
      cv::getStructuringElement(cv::MORPH_RECT, cv::Size(close_win, close_win));
  cv::morphologyEx(smoothed, *illumination, cv::MORPH_CLOSE, kernel);

  cv::Mat illumination_f;
  illumination->convertTo(illumination_f, CV_32F);
  cv::max(illumination_f, 1.0, illumination_f);

  cv::Mat gray_f;
  gray.convertTo(gray_f, CV_32F);
  cv::Mat equalized_f;
  cv::divide(gray_f, illumination_f, equalized_f, config.scale_constant);
  equalized_f.convertTo(*equalized, CV_8U);
}

void AdaptiveBinarize(const cv::Mat& equalized, double module_px,
                      const BinarizeConfig& config, cv::Mat* binary) {
  int window = detail::OddAtLeast(
      config.adaptive_window_factor * module_px, 3);
  const int min_side = std::min(equalized.rows, equalized.cols);
  if (window >= min_side) {
    window = min_side - 1;
  }
  if (window % 2 == 0) {
    --window;
  }
  window = std::max(3, window);
  cv::adaptiveThreshold(equalized, *binary, 255,
                        cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY,
                        window, config.adaptive_c);
}

}  // namespace

const char* ScanlineEstimator::Name() const {
  return "scanline";
}

ScaleEstimate ScanlineEstimator::Estimate(const cv::Mat& gray,
                                          const BinarizeConfig& config) const {
  return EstimateScanline(gray, config);
}

const char* CannyEstimator::Name() const {
  return "canny";
}

ScaleEstimate CannyEstimator::Estimate(const cv::Mat& gray,
                                       const BinarizeConfig& config) const {
  return EstimateCanny(gray, config, config.canny_variant);
}

const char* HybridEstimator::Name() const {
  return "hybrid";
}

ScaleEstimate HybridEstimator::Estimate(const cv::Mat& gray,
                                        const BinarizeConfig& config) const {
  return EstimateHybrid(gray, config);
}

std::unique_ptr<IScaleEstimator> CreateScaleEstimator(EstimatorKind kind) {
  switch (kind) {
    case EstimatorKind::kScanline:
      return std::make_unique<ScanlineEstimator>();
    case EstimatorKind::kCanny:
      return std::make_unique<CannyEstimator>();
    case EstimatorKind::kHybrid:
      return std::make_unique<HybridEstimator>();
  }
  return nullptr;
}

ScaleEstimate EstimateHybrid(const cv::Mat& gray,
                             const BinarizeConfig& config) {
  // fallback_to_scanline=false 时退化为“只用 Canny”的轻量模式。
  if (!config.fallback_to_scanline) {
    ScaleEstimate result = EstimateCanny(gray, config, config.canny_variant);
    result.method = EstimatorKind::kHybrid;
    result.diagnostics =
        "hybrid: canny only (fallback_to_scanline=false); " + result.diagnostics;
    return result;
  }

  // 双估计 + 一致性门控：
  //   - 两者都有效且相对差异 <= 20%：优先 Canny（通常更稳、更快）；
  //   - 两者都有效但分歧大：以扫描线为准（旧实验里 Canny 在强模糊上会
  //     “自信地错”，扫描线在 blur_1 上仍正确）；
  //   - 只有一路有效：用有效的一路；
  //   - 都失败：返回 kNotFound。
  const ScaleEstimate canny = EstimateCanny(gray, config, config.canny_variant);
  const ScaleEstimate scanline = EstimateScanline(gray, config);

  const bool canny_ok = canny.status == Status::kOk && canny.module_px > 0.0;
  const bool scanline_ok =
      scanline.status == Status::kOk && scanline.module_px > 0.0;

  ScaleEstimate result;
  result.method = EstimatorKind::kHybrid;
  result.elapsed_ms = canny.elapsed_ms + scanline.elapsed_ms;

  if (canny_ok && scanline_ok) {
    const double denominator =
        std::max(1e-9, std::max(canny.module_px, scanline.module_px));
    const double relative_difference =
        std::abs(canny.module_px - scanline.module_px) / denominator;
    if (relative_difference <= 0.20) {
      result = canny;
      result.diagnostics = "hybrid: canny accepted; agree with scanline; " +
                           canny.diagnostics;
    } else {
      result = scanline;
      result.diagnostics = "hybrid: scanline accepted (disagree with canny " +
                           std::to_string(relative_difference) + "); " +
                           scanline.diagnostics;
    }
  } else if (canny_ok) {
    result = canny;
    result.diagnostics =
        "hybrid: canny only; scanline=" + scanline.diagnostics;
  } else if (scanline_ok) {
    result = scanline;
    result.diagnostics =
        "hybrid: scanline only; canny=" + canny.diagnostics;
  } else {
    result.status = Status::kNotFound;
    result.diagnostics = "hybrid: both failed; canny=" + canny.diagnostics +
                         "; scanline=" + scanline.diagnostics;
    result.elapsed_ms = canny.elapsed_ms + scanline.elapsed_ms;
    return result;
  }
  result.method = EstimatorKind::kHybrid;
  result.elapsed_ms = canny.elapsed_ms + scanline.elapsed_ms;
  return result;
}

BinarizeResult Binarizer::Process(const cv::Mat& gray,
                                  const BinarizeConfig& config) const {
  const auto start = std::chrono::steady_clock::now();
  BinarizeResult result;

  if (!detail::IsGray8(gray)) {
    result.status = Status::kInvalidInput;
    result.diagnostics = "binarize: input must be CV_8UC1";
    return result;
  }
  if (gray.rows < 32 || gray.cols < 32) {
    result.status = Status::kInvalidInput;
    result.diagnostics = "binarize: image smaller than 32x32";
    return result;
  }

  result.original = config.return_original ? gray : cv::Mat();

  const auto t_denoise = std::chrono::steady_clock::now();
  cv::Mat prepared = ApplyDenoise(gray, config.denoise);
  result.denoise_ms = MillisecondsSince(t_denoise);

  const auto t_scale = std::chrono::steady_clock::now();
  const auto estimator = CreateScaleEstimator(config.estimator);
  if (estimator) {
    result.scale = estimator->Estimate(prepared, config);
  } else {
    result.scale.status = Status::kInternalError;
    result.scale.diagnostics = "binarize: unknown estimator";
  }
  result.scale.elapsed_ms = MillisecondsSince(t_scale);

  if (result.scale.status != Status::kOk || result.scale.module_px <= 0.0) {
    result.status = Status::kNotFound;
    result.diagnostics = result.scale.diagnostics;
    result.elapsed_ms = MillisecondsSince(start);
    return result;
  }

  const auto t_equalize = std::chrono::steady_clock::now();
  EqualizeIllumination(prepared, result.scale.module_px, config,
                       &result.illumination, &result.equalized);
  result.equalize_ms = MillisecondsSince(t_equalize);

  const auto t_binary = std::chrono::steady_clock::now();
  AdaptiveBinarize(result.equalized, result.scale.module_px, config,
                   &result.binary);
  result.binary_ms = MillisecondsSince(t_binary);

  if (!config.return_equalized) {
    result.equalized.release();
  }
  if (!config.return_binary) {
    result.binary.release();
  }

  ComputeQuality(gray, result.illumination, result.equalized, result.binary,
                 &result.quality);
  result.status = Status::kOk;
  result.diagnostics = "estimator=" +
                       std::string(EstimatorKindName(result.scale.method)) +
                       " module_px=" + std::to_string(result.scale.module_px) +
                       " confidence=" + std::to_string(result.scale.confidence);
  result.elapsed_ms = MillisecondsSince(start);
  return result;
}

bool LoadBinarizeConfig(const std::string& path, BinarizeConfig* config,
                        std::string* error) {
  if (config == nullptr) {
    if (error) {
      *error = "null config";
    }
    return false;
  }
  try {
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) {
      if (error) {
        *error = "cannot open config: " + path;
      }
      return false;
    }
    cv::FileNode node = fs["binarize"];
    if (node.isNone()) {
      node = fs.root();
    }
    if (node.isNone()) {
      if (error) {
        *error = "config has no binarize section";
      }
      return false;
    }

    auto read_string = [&](const cv::FileNode& parent, const char* key,
                           std::string* value) {
      const cv::FileNode child = parent[key];
      if (!child.isNone()) {
        child >> *value;
      }
    };
    auto read_double = [&](const cv::FileNode& parent, const char* key,
                           double* value) {
      const cv::FileNode child = parent[key];
      if (!child.isNone()) {
        child >> *value;
      }
    };
    auto read_int = [&](const cv::FileNode& parent, const char* key,
                        int* value) {
      const cv::FileNode child = parent[key];
      if (!child.isNone()) {
        child >> *value;
      }
    };
    auto read_bool = [&](const cv::FileNode& parent, const char* key,
                         bool* value) {
      const cv::FileNode child = parent[key];
      if (!child.isNone()) {
        int raw = *value ? 1 : 0;
        child >> raw;
        *value = raw != 0;
      }
    };

    std::string estimator;
    read_string(node, "estimator", &estimator);
    if (estimator == "scanline") {
      config->estimator = EstimatorKind::kScanline;
    } else if (estimator == "canny") {
      config->estimator = EstimatorKind::kCanny;
    } else if (estimator == "hybrid") {
      config->estimator = EstimatorKind::kHybrid;
    }

    std::string canny_variant;
    read_string(node, "canny_variant", &canny_variant);
    if (canny_variant == "strict") {
      config->canny_variant = CannyVariant::kStrict;
    } else if (canny_variant == "norm") {
      config->canny_variant = CannyVariant::kNorm;
    } else if (canny_variant == "norm_round") {
      config->canny_variant = CannyVariant::kNormRound;
    } else if (canny_variant == "relaxed") {
      config->canny_variant = CannyVariant::kRelaxed;
    }

    std::string denoise;
    read_string(node, "denoise", &denoise);
    if (denoise == "none") {
      config->denoise = DenoiseKind::kNone;
    } else if (denoise == "median3") {
      config->denoise = DenoiseKind::kMedian3;
    } else if (denoise == "bilateral") {
      config->denoise = DenoiseKind::kBilateral;
    } else if (denoise == "gaussian") {
      config->denoise = DenoiseKind::kGaussian;
    }

    read_double(node, "mean_filter_factor", &config->mean_filter_factor);
    read_double(node, "close_factor", &config->close_factor);
    read_double(node, "scale_constant", &config->scale_constant);
    read_double(node, "adaptive_window_factor",
                &config->adaptive_window_factor);
    read_double(node, "adaptive_c", &config->adaptive_c);
    read_bool(node, "fallback_to_scanline", &config->fallback_to_scanline);
    read_bool(node, "return_original", &config->return_original);
    read_bool(node, "return_equalized", &config->return_equalized);
    read_bool(node, "return_binary", &config->return_binary);

    const cv::FileNode canny_node = node["canny"];
    if (!canny_node.isNone()) {
      read_int(canny_node, "low_threshold", &config->canny_low_threshold);
      read_int(canny_node, "high_threshold", &config->canny_high_threshold);
      read_int(canny_node, "close_kernel", &config->canny_close_kernel);
      read_int(canny_node, "collapse_gap", &config->canny_collapse_gap);
      read_double(canny_node, "ratio_tolerance",
                  &config->canny_ratio_tolerance);
      read_double(canny_node, "fit_error_max", &config->canny_fit_error_max);
    }

    const cv::FileNode scanline_node = node["scanline"];
    if (!scanline_node.isNone()) {
      read_int(scanline_node, "median_kernel",
               &config->scanline_median_kernel);
      read_double(scanline_node, "scale_min", &config->scanline_scale_min);
      read_double(scanline_node, "scale_max", &config->scanline_scale_max);
      read_double(scanline_node, "fit_error_max",
                  &config->scanline_fit_error_max);
      read_double(scanline_node, "bin_width_px",
                  &config->scanline_bin_width_px);
      read_double(scanline_node, "adaptive_c",
                  &config->scanline_adaptive_c);
    }
    return true;
  } catch (const cv::Exception& exception) {
    if (error) {
      *error = exception.what();
    }
    return false;
  }
}

}  // namespace qrscan::binarize
