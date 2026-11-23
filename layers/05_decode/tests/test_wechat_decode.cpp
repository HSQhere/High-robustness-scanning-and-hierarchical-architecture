#include "qrscan/decode/wechat_decoder.hpp"
#include <iostream>
#include <opencv2/imgcodecs.hpp>

using namespace qrscan::decode;
int main(int argc, char** argv) {
  cv::setNumThreads(1);
  const cv::Mat blank(160, 160, CV_8U, cv::Scalar(255));
  WeChatDecoder missing({});
  const auto miss = missing.Decode(blank, 29, View::kBinary);
  if (miss.status != ReadStatus::kBackendUnavailable || !miss.candidates.empty() || miss.diagnostics.empty() ||
      miss.frame_id != 29 || miss.view != View::kBinary || miss.elapsed_ms < 0) return 1;
  if (missing.Decode(cv::Mat(), 1, View::kOriginal).status != ReadStatus::kInvalidInput ||
      missing.Decode(cv::Mat(80,80,CV_8UC3), 1, View::kOriginal).status != ReadStatus::kInvalidInput) return 2;
  if (argc == 1) return 0;
  if (argc != 3) return 3;
  WeChatDecoder reader(argv[1]);
  if (reader.Decode(blank, 11, View::kEqualized).status != ReadStatus::kNoText) return 4;
  auto gray = cv::imread(argv[2], cv::IMREAD_GRAYSCALE);
  const auto hit = reader.Decode(gray, 12, View::kOriginal);
  if (hit.status != ReadStatus::kSingle || hit.candidates.size()!=1 || hit.candidates[0].text != "156+123+516+231") {
    std::cerr << "Smoke image did not return its reference payload: " << hit.diagnostics << '\n'; return 5;
  }
  for (auto pt : hit.candidates[0].view_points) {
    if (!(pt.x >= -0.6 && pt.x < gray.cols + 0.6 && pt.y >= -0.6 && pt.y < gray.rows + 0.6)) return 6;
  }
  return 0;
}
