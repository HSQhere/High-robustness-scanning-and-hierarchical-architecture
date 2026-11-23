// 公共基础类型：跨阶段只依赖本文件与 binarize_types.hpp。
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <opencv2/core.hpp>

namespace qrscan {

inline constexpr int kApiVersion = 1;

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

inline const char* StatusName(Status status) {
  switch (status) {
    case Status::kOk:
      return "ok";
    case Status::kInvalidInput:
      return "invalid_input";
    case Status::kNotFound:
      return "not_found";
    case Status::kLowConfidence:
      return "low_confidence";
    case Status::kUnsaturatedInfoLost:
      return "information_lost";
    case Status::kModelUnavailable:
      return "model_unavailable";
    case Status::kTimeout:
      return "timeout";
    case Status::kInternalError:
      return "internal_error";
  }
  return "unknown";
}

struct CameraSettings {
  int exposure_us = 0;
  int iso = 0;
  int white_balance_k = 0;
  double brightness = 0.0;
  double gain = 0.0;
};

struct Frame {
  uint64_t frame_id = 0;
  int64_t timestamp_ns = 0;
  cv::Mat bgr;
  CameraSettings settings;
  Status status = Status::kOk;
  std::string diagnostics;
};

struct Quad {
  std::array<cv::Point2f, 4> points{};
  bool valid = false;
};

struct RoiImage {
  cv::Mat bgr;
  cv::Mat gray;
  Quad source_quad;
  cv::Size original_size;
  double scale_from_original = 1.0;
  Status status = Status::kOk;
};

}  // namespace qrscan
