#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>

namespace qrscan::decode {
enum class ReadStatus { kSingle, kNoText, kMultiple, kInvalidInput, kBackendUnavailable, kBackendError };
enum class View { kOriginal, kEqualized, kBinary };

struct Candidate {
  std::string text;
  std::array<cv::Point2f, 4> view_points;  // Decoder order, in input-view pixels, not original-frame coordinates.
};

struct ReadResult {
  ReadStatus status;
  uint64_t frame_id;
  View view;
  std::vector<Candidate> candidates;
  double elapsed_ms;
  std::string diagnostics;
};

// One instance per worker. Each call returns evidence for one view, never a task-code decision.
class WeChatDecoder {
 public:
  explicit WeChatDecoder(const std::filesystem::path& model_dir);
  ~WeChatDecoder();
  ReadResult Decode(const cv::Mat& gray, uint64_t frame_id, View view);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace qrscan::decode
