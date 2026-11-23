// 扫描线估计器：多阈值 + 1:1:3:1:1 游程。
//
// 与旧 Python 参考实现保持行为一致：
//   - medianBlur(gray, 3) → equalizeHist
//   - Otsu(原图)、Otsu(均衡图)、多尺度自适应均值阈值
//   - 逐行/逐列找黑/白/黑/白/黑 5 段，最小二乘拟合 scale
//   - 0.5 px 分箱，按 support / (0.05 + median_error)^1.2 选簇
#include "qrscan/binarize/binarizer.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/imgproc.hpp>

#include "internal.hpp"

namespace qrscan::binarize {
namespace {

struct Candidate {
  double module_px = 0.0;
  double error = 0.0;
  std::string view;
};

struct Run {
  int value = 0;
  int width = 0;
};

std::vector<Run> BuildRuns(const unsigned char* data, int length) {
  std::vector<Run> runs;
  if (length <= 0) {
    return runs;
  }
  int start = 0;
  for (int i = 1; i <= length; ++i) {
    if (i == length || data[i] != data[start]) {
      runs.push_back({data[start] == 0 ? 1 : 0, i - start});
      start = i;
    }
  }
  return runs;
}

void ScanLine(const unsigned char* data, int length, const std::string& view,
              const BinarizeConfig& config, std::vector<Candidate>* candidates) {
  const std::vector<Run> runs = BuildRuns(data, length);
  if (runs.size() < 5) {
    return;
  }
  constexpr double kExpected[5] = {1.0, 1.0, 3.0, 1.0, 1.0};
  // dot(expected, expected) = 1+1+9+1+1 = 13（不是 7）。
  constexpr double kExpectedNorm = 13.0;

  for (std::size_t i = 0; i + 4 < runs.size(); ++i) {
    if (runs[i].value != 1 || runs[i + 1].value != 0 || runs[i + 2].value != 1 ||
        runs[i + 3].value != 0 || runs[i + 4].value != 1) {
      continue;
    }
    const double w[5] = {static_cast<double>(runs[i].width),
                         static_cast<double>(runs[i + 1].width),
                         static_cast<double>(runs[i + 2].width),
                         static_cast<double>(runs[i + 3].width),
                         static_cast<double>(runs[i + 4].width)};
    double dot = 0.0;
    for (int j = 0; j < 5; ++j) {
      dot += w[j] * kExpected[j];
    }
    const double scale = dot / kExpectedNorm;
    if (scale < config.scanline_scale_min || scale > config.scanline_scale_max) {
      continue;
    }
    double error = 0.0;
    for (int j = 0; j < 5; ++j) {
      error += std::abs(w[j] / scale - kExpected[j]);
    }
    error /= 5.0;
    if (error > config.scanline_fit_error_max) {
      continue;
    }
    candidates->push_back({scale, error, view});
  }
}

void ScanMask(const cv::Mat& mask, const std::string& view,
              const BinarizeConfig& config, std::vector<Candidate>* candidates) {
  for (int y = 0; y < mask.rows; ++y) {
    ScanLine(mask.ptr<unsigned char>(y), mask.cols, view, config, candidates);
  }
  std::vector<unsigned char> column(static_cast<std::size_t>(mask.rows));
  for (int x = 0; x < mask.cols; ++x) {
    for (int y = 0; y < mask.rows; ++y) {
      column[static_cast<std::size_t>(y)] = mask.at<unsigned char>(y, x);
    }
    ScanLine(column.data(), mask.rows, view, config, candidates);
  }
}

cv::Mat OtsuMask(const cv::Mat& source) {
  cv::Mat mask;
  cv::threshold(source, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
  return mask;
}

cv::Mat AdaptiveMask(const cv::Mat& source, int block_size, double c) {
  cv::Mat mask;
  cv::adaptiveThreshold(source, mask, 255, cv::ADAPTIVE_THRESH_MEAN_C,
                        cv::THRESH_BINARY, block_size, c);
  return mask;
}

}  // namespace

ScaleEstimate EstimateScanline(const cv::Mat& gray, const BinarizeConfig& config) {
  const auto start = std::chrono::steady_clock::now();
  ScaleEstimate estimate;
  estimate.method = EstimatorKind::kScanline;

  if (!detail::IsGray8(gray)) {
    estimate.status = Status::kInvalidInput;
    estimate.diagnostics = "scanline: input must be CV_8UC1";
    return estimate;
  }
  if (gray.rows < 16 || gray.cols < 16) {
    estimate.status = Status::kInvalidInput;
    estimate.diagnostics = "scanline: image too small";
    return estimate;
  }

  cv::Mat median;
  cv::medianBlur(gray, median, detail::OddAtLeast(config.scanline_median_kernel, 3));
  cv::Mat equalized;
  cv::equalizeHist(median, equalized);

  std::vector<std::pair<std::string, cv::Mat>> masks;
  masks.emplace_back("otsu", OtsuMask(median));
  masks.emplace_back("equalized_otsu", OtsuMask(equalized));

  const int min_side = std::min(gray.rows, gray.cols);
  const int base = detail::OddAtLeast(min_side / 12.0, 15);
  for (double factor : {0.65, 1.0, 1.5}) {
    const int block = detail::OddAtLeast(base * factor, 15);
    if (block >= min_side) {
      continue;
    }
    masks.emplace_back("adaptive_" + std::to_string(block),
                       AdaptiveMask(equalized, block, config.scanline_adaptive_c));
  }

  std::vector<Candidate> candidates;
  for (const auto& entry : masks) {
    ScanMask(entry.second, entry.first, config, &candidates);
  }

  estimate.candidates = static_cast<int>(candidates.size());
  if (candidates.empty()) {
    estimate.status = Status::kNotFound;
    estimate.diagnostics = "scanline: no 1:1:3:1:1 candidate";
    estimate.elapsed_ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - start)
                              .count();
    return estimate;
  }

  const double bin_width = std::max(0.1, config.scanline_bin_width_px);
  std::map<int, std::vector<Candidate>> bins;
  for (const Candidate& candidate : candidates) {
    const int key = static_cast<int>(std::llround(candidate.module_px / bin_width));
    bins[key].push_back(candidate);
  }

  double best_score = -1.0;
  int best_key = 0;
  int best_support = 0;
  const bool debug = std::getenv("QRSCAN_DEBUG") != nullptr;
  if (debug) {
    std::cerr << "[scanline] candidates=" << candidates.size() << "\n";
    for (std::size_t i = 0; i < std::min<std::size_t>(20, candidates.size()); ++i) {
      std::cerr << "  cand[" << i << "] scale=" << candidates[i].module_px
                << " err=" << candidates[i].error
                << " view=" << candidates[i].view << "\n";
    }
  }
  for (const auto& entry : bins) {
    std::vector<double> errors;
    errors.reserve(entry.second.size());
    for (const Candidate& candidate : entry.second) {
      errors.push_back(candidate.error);
    }
    const double median_error = detail::Median(errors);
    const double support = static_cast<double>(entry.second.size());
    const double score = support / std::pow(0.05 + median_error, 1.2);
    if (debug) {
      std::cerr << "  bin key=" << entry.first
                << " center=" << entry.first * bin_width
                << " support=" << entry.second.size()
                << " med_err=" << median_error << " score=" << score << "\n";
    }
    if (score > best_score) {
      best_score = score;
      best_key = entry.first;
      best_support = static_cast<int>(entry.second.size());
    }
  }

  const double center = best_key * bin_width;
  std::vector<double> modules;
  std::vector<double> errors;
  for (const Candidate& candidate : candidates) {
    if (std::abs(candidate.module_px - center) <= 0.75) {
      modules.push_back(candidate.module_px);
      errors.push_back(candidate.error);
    }
  }

  estimate.status = Status::kOk;
  estimate.module_px = detail::Median(modules);
  estimate.support = best_support;
  estimate.fit_error = detail::Median(errors);
  estimate.ambiguous = false;
  estimate.confidence = detail::Confidence(
      estimate.support, estimate.fit_error, estimate.ambiguous,
      config.scanline_fit_error_max);
  estimate.diagnostics = "scanline: views=" + std::to_string(masks.size()) +
                         " candidates=" + std::to_string(candidates.size()) +
                         " support=" + std::to_string(best_support);
  estimate.elapsed_ms = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - start)
                            .count();
  return estimate;
}

}  // namespace qrscan::binarize
