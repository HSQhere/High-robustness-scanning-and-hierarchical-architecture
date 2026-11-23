// 步骤 4 实现内部工具；不对外暴露。
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "qrscan/common/binarize_types.hpp"

namespace qrscan::binarize::detail {

inline int OddAtLeast(double value, int minimum = 3) {
  int result = std::max(minimum, static_cast<int>(std::lround(value)));
  if (result % 2 == 0) {
    ++result;
  }
  return result;
}

inline double Median(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  const std::size_t mid = values.size() / 2;
  std::nth_element(values.begin(), values.begin() + mid, values.end());
  double result = values[mid];
  if (values.size() % 2 == 0) {
    std::nth_element(values.begin(), values.begin() + mid - 1, values.begin() + mid);
    result = 0.5 * (result + values[mid - 1]);
  }
  return result;
}

inline bool IsGray8(const cv::Mat& image) {
  return !image.empty() && image.type() == CV_8UC1;
}

inline double Confidence(int support, double error, bool ambiguous,
                         double error_scale) {
  const double support_score = std::min(1.0, support / 8.0);
  const double error_score =
      std::max(0.0, 1.0 - error / std::max(1e-9, error_scale));
  double score = 0.5 * support_score + 0.5 * error_score;
  if (ambiguous) {
    score *= 0.5;
  }
  return std::clamp(score, 0.0, 1.0);
}

inline double Mean(const std::vector<double>& values) {
  if (values.empty()) {
    return 0.0;
  }
  return std::accumulate(values.begin(), values.end(), 0.0) /
         static_cast<double>(values.size());
}

}  // namespace qrscan::binarize::detail
