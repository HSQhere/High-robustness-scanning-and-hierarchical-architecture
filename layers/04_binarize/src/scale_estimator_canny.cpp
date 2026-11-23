// Canny 双边缘融合估计器（论文 §3.1 / Algorithm 1 的工程复现）。
//
// 三种比例筛选：
//   - strict  字面相邻比例 9/11, 11/27, 27/11, 11/9
//   - norm    归一化比例向量 [9,11,27,11,9]/67（尺度无关）
//   - relaxed 放宽的 1:1:3:1:1 检查
// module = 6 个边缘点的 7 模块总跨度 / 7。
#include "qrscan/binarize/binarizer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include <opencv2/imgproc.hpp>

#include "internal.hpp"

namespace qrscan::binarize {
namespace {

constexpr std::array<double, 5> kPaperExpected = {9.0, 11.0, 27.0, 11.0, 9.0};
constexpr std::array<double, 4> kPaperRatios = {9.0 / 11.0, 11.0 / 27.0,
                                                27.0 / 11.0, 11.0 / 9.0};
constexpr std::array<double, 5> kPaperNorm = {9.0 / 67.0, 11.0 / 67.0,
                                              27.0 / 67.0, 11.0 / 67.0,
                                              9.0 / 67.0};

struct Candidate {
  double module_px = 0.0;
  double error = 0.0;
};

std::vector<double> CollapsePoints(const unsigned char* data, int length,
                                   int gap) {
  std::vector<int> indices;
  for (int i = 0; i < length; ++i) {
    if (data[i] != 0) {
      indices.push_back(i);
    }
  }
  std::vector<double> points;
  if (indices.empty()) {
    return points;
  }
  int start = indices[0];
  int previous = indices[0];
  for (std::size_t i = 1; i < indices.size(); ++i) {
    const int x = indices[i];
    if (x - previous <= gap) {
      previous = x;
    } else {
      points.push_back(0.5 * (start + previous));
      start = previous = x;
    }
  }
  points.push_back(0.5 * (start + previous));
  return points;
}

bool GapsFromPoints(const std::vector<double>& points, std::size_t index,
                    std::array<double, 5>* gaps) {
  for (int j = 0; j < 5; ++j) {
    (*gaps)[j] = points[index + j + 1] - points[index + j];
    if ((*gaps)[j] <= 0.0) {
      return false;
    }
  }
  return true;
}

void StrictCandidates(const std::vector<double>& points,
                      const BinarizeConfig& config,
                      std::vector<Candidate>* candidates) {
  for (std::size_t i = 0; i + 5 < points.size(); ++i) {
    std::array<double, 5> gaps{};
    if (!GapsFromPoints(points, i, &gaps)) {
      continue;
    }
    bool ratios_ok = true;
    for (int j = 0; j < 4; ++j) {
      if (std::abs(gaps[j] / gaps[j + 1] - kPaperRatios[j]) >=
          config.canny_ratio_tolerance) {
        ratios_ok = false;
        break;
      }
    }
    if (!ratios_ok) {
      continue;
    }
    double dot = 0.0;
    double norm = 0.0;
    for (int j = 0; j < 5; ++j) {
      dot += gaps[j] * kPaperExpected[j];
      norm += kPaperExpected[j] * kPaperExpected[j];
    }
    const double scale = dot / norm;
    if (scale <= 0.0) {
      continue;
    }
    double error = 0.0;
    for (int j = 0; j < 5; ++j) {
      error += std::abs(gaps[j] / scale - kPaperExpected[j]);
    }
    error = error / 5.0 / 9.0;
    if (error > config.canny_fit_error_max) {
      continue;
    }
    const double module = (gaps[0] + gaps[1] + gaps[2] + gaps[3] + gaps[4]) / 7.0;
    if (module > config.scanline_scale_min && module < config.scanline_scale_max) {
      candidates->push_back({module, error});
    }
  }
}

void NormCandidates(const std::vector<double>& points, const cv::Size& image_size,
                    const BinarizeConfig& config,
                    std::vector<Candidate>* candidates) {
  for (std::size_t i = 0; i + 5 < points.size(); ++i) {
    std::array<double, 5> gaps{};
    if (!GapsFromPoints(points, i, &gaps)) {
      continue;
    }
    double total = 0.0;
    for (double gap : gaps) {
      total += gap;
    }
    if (total <= 0.0) {
      continue;
    }
    double max_deviation = 0.0;
    for (int j = 0; j < 5; ++j) {
      max_deviation = std::max(max_deviation,
                               std::abs(gaps[j] / total - kPaperNorm[j]));
    }
    if (max_deviation >= config.canny_ratio_tolerance) {
      continue;
    }
    const double win = total;
    const double limit = (image_size.width + image_size.height) / 2.0 / 3.0;
    if (!(14.0 < win && win < limit)) {
      continue;
    }
    const double module = total / 7.0;
    if (module > config.scanline_scale_min && module < config.scanline_scale_max) {
      candidates->push_back({module, max_deviation});
    }
  }
}

void RelaxedCandidates(const std::vector<double>& points,
                       const BinarizeConfig& config,
                       std::vector<Candidate>* candidates) {
  for (std::size_t i = 0; i + 5 < points.size(); ++i) {
    std::array<double, 5> gaps{};
    if (!GapsFromPoints(points, i, &gaps)) {
      continue;
    }
    const double s = 0.5 * (gaps[0] + gaps[4]);
    if (s < 1.0) {
      continue;
    }
    const double outer_ratio = gaps[0] / gaps[4];
    if (!(outer_ratio > 0.65 && outer_ratio < 1.55)) {
      continue;
    }
    if (!(gaps[1] > 0.45 * s && gaps[1] < 1.9 * s && gaps[3] > 0.45 * s &&
          gaps[3] < 1.9 * s)) {
      continue;
    }
    if (!(gaps[2] > 1.9 * s && gaps[2] < 4.1 * s)) {
      continue;
    }
    if (std::abs(gaps[1] - gaps[3]) / s > 0.7) {
      continue;
    }
    const double module = (gaps[0] + gaps[1] + gaps[2] + gaps[3] + gaps[4]) / 7.0;
    if (module > config.scanline_scale_min && module < config.scanline_scale_max) {
      candidates->push_back({module, 0.0});
    }
  }
}

struct Mode {
  double module_px = 0.0;
  double error = 0.0;
  int support = 0;
  bool ambiguous = true;
};

Mode PickMode(const std::vector<Candidate>& candidates) {
  Mode mode;
  if (candidates.empty()) {
    return mode;
  }
  std::map<int, int> counts;
  for (const Candidate& candidate : candidates) {
    ++counts[static_cast<int>(std::llround(candidate.module_px * 2.0))];
  }
  int best_key = 0;
  int best_support = 0;
  for (const auto& entry : counts) {
    if (entry.second > best_support) {
      best_support = entry.second;
      best_key = entry.first;
    }
  }
  const double center = best_key / 2.0;
  std::vector<double> modules;
  std::vector<double> errors;
  for (const Candidate& candidate : candidates) {
    if (std::abs(candidate.module_px - center) <= 0.75) {
      modules.push_back(candidate.module_px);
      errors.push_back(candidate.error);
    }
  }
  mode.module_px = detail::Median(modules);
  mode.error = detail::Median(errors);
  mode.support = best_support;
  mode.ambiguous = best_support < 2;
  return mode;
}

void ScanEdges(const std::vector<double>& points, CannyVariant variant,
               const cv::Size& image_size, const BinarizeConfig& config,
               std::vector<Candidate>* candidates) {
  switch (variant) {
    case CannyVariant::kStrict:
      StrictCandidates(points, config, candidates);
      break;
    case CannyVariant::kNorm:
    case CannyVariant::kNormRound:
      NormCandidates(points, image_size, config, candidates);
      break;
    case CannyVariant::kRelaxed:
      RelaxedCandidates(points, config, candidates);
      break;
  }
}

}  // namespace

ScaleEstimate EstimateCanny(const cv::Mat& gray, const BinarizeConfig& config,
                            CannyVariant variant) {
  const auto start = std::chrono::steady_clock::now();
  ScaleEstimate estimate;
  estimate.method = EstimatorKind::kCanny;
  estimate.canny_variant = variant;

  if (!detail::IsGray8(gray)) {
    estimate.status = Status::kInvalidInput;
    estimate.diagnostics = "canny: input must be CV_8UC1";
    return estimate;
  }
  if (gray.rows < 16 || gray.cols < 16) {
    estimate.status = Status::kInvalidInput;
    estimate.diagnostics = "canny: image too small";
    return estimate;
  }

  cv::Mat median;
  cv::medianBlur(gray, median, 3);
  const int close_size = detail::OddAtLeast(config.canny_close_kernel, 3);
  const cv::Mat kernel =
      cv::getStructuringElement(cv::MORPH_RECT, cv::Size(close_size, close_size));
  cv::Mat closed;
  cv::morphologyEx(median, closed, cv::MORPH_CLOSE, kernel);
  cv::Mat equalized;
  cv::equalizeHist(closed, equalized);

  cv::Mat edge1;
  cv::Mat edge2;
  cv::Canny(closed, edge1, config.canny_low_threshold,
            config.canny_high_threshold);
  cv::Canny(equalized, edge2, config.canny_low_threshold,
            config.canny_high_threshold);
  cv::Mat edges;
  cv::bitwise_or(edge1, edge2, edges);

  std::vector<Candidate> candidates;
  const int gap = std::max(0, config.canny_collapse_gap);
  for (int y = 0; y < edges.rows; ++y) {
    const std::vector<double> points =
        CollapsePoints(edges.ptr<unsigned char>(y), edges.cols, gap);
    ScanEdges(points, variant, gray.size(), config, &candidates);
  }
  std::vector<unsigned char> column(static_cast<std::size_t>(edges.rows));
  for (int x = 0; x < edges.cols; ++x) {
    for (int y = 0; y < edges.rows; ++y) {
      column[static_cast<std::size_t>(y)] = edges.at<unsigned char>(y, x);
    }
    const std::vector<double> points =
        CollapsePoints(column.data(), edges.rows, gap);
    ScanEdges(points, variant, gray.size(), config, &candidates);
  }

  const Mode mode = PickMode(candidates);
  estimate.candidates = static_cast<int>(candidates.size());
  estimate.support = mode.support;
  estimate.ambiguous = mode.ambiguous;
  estimate.fit_error = mode.error;
  if (mode.support == 0) {
    estimate.status = Status::kNotFound;
    estimate.diagnostics = "canny: no ratio candidate";
  } else {
    estimate.status = Status::kOk;
    estimate.module_px = (variant == CannyVariant::kNormRound)
                             ? std::round(mode.module_px)
                             : mode.module_px;
    const double error_scale =
        (variant == CannyVariant::kStrict) ? config.canny_fit_error_max
                                           : config.canny_ratio_tolerance;
    estimate.confidence = detail::Confidence(mode.support, mode.error,
                                             mode.ambiguous, error_scale);
    estimate.diagnostics = std::string("canny: variant=") +
                           CannyVariantName(variant) +
                           " candidates=" + std::to_string(candidates.size()) +
                           " support=" + std::to_string(mode.support) +
                           (mode.ambiguous ? " ambiguous" : "");
  }
  estimate.elapsed_ms = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - start)
                            .count();
  return estimate;
}

}  // namespace qrscan::binarize
