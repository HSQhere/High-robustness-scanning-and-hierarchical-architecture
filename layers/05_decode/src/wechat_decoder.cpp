#include "qrscan/decode/wechat_decoder.hpp"

#include <chrono>
#include <utility>
#ifdef QRSCAN_HAS_WECHAT
#include <opencv2/wechat_qrcode.hpp>
#endif

namespace qrscan::decode {
struct WeChatDecoder::Impl {
  std::string reason;
#ifdef QRSCAN_HAS_WECHAT
  std::unique_ptr<cv::wechat_qrcode::WeChatQRCode> backend;
#endif
};

WeChatDecoder::WeChatDecoder(const std::filesystem::path& dir) : impl_(std::make_unique<Impl>()) {
#ifdef QRSCAN_HAS_WECHAT
  if (dir.empty()) { impl_->reason = "No model directory configured"; return; }
  std::array<std::string, 4> paths;
  const std::array<const char*, 4> names{"detect_2021nov.prototxt", "detect_2021nov.caffemodel",
                                        "sr_2021nov.prototxt", "sr_2021nov.caffemodel"};
  for (size_t i = 0; i < names.size(); ++i) {
    auto path = dir / names[i];
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
      impl_->reason = std::string("Model file unavailable: ") + names[i];
      return;
    }
    paths[i] = path.string();
  }
  try {
    impl_->backend = std::make_unique<cv::wechat_qrcode::WeChatQRCode>(paths[0], paths[1], paths[2], paths[3]);
  } catch (const cv::Exception& e) {
    impl_->reason = e.what();
  }
#else
  (void)dir;
  impl_->reason = "Built without OpenCV wechat_qrcode";
#endif
}

WeChatDecoder::~WeChatDecoder() = default;

ReadResult WeChatDecoder::Decode(const cv::Mat& gray, uint64_t id, View view) {
  using Clock = std::chrono::steady_clock;
  const auto start = Clock::now();
  ReadResult result{ReadStatus::kBackendUnavailable, id, view, {}, 0.0, {}};
  if (gray.empty() || gray.dims != 2 || gray.type() != CV_8UC1) {
    result.status = ReadStatus::kInvalidInput;
    result.diagnostics = "Expected nonempty CV_8UC1 image";
  } else if (!impl_->reason.empty()) {
    result.diagnostics = impl_->reason;
  }
#ifdef QRSCAN_HAS_WECHAT
  else {
    try {
      std::vector<cv::Mat> quads;
      auto texts = impl_->backend->detectAndDecode(gray, quads);
      CV_Assert(texts.size() == quads.size());
      for (size_t i = 0; i < texts.size(); ++i) {
        const cv::Mat& q = quads[i];
        CV_Assert(!texts[i].empty() && q.checkVector(2, CV_32F) == 4 && cv::checkRange(q));
        const auto pairs = q.reshape(1, 4);
        Candidate item{std::move(texts[i]), {}};
        for (int j = 0; j < 4; ++j) item.view_points[j] = {pairs.at<float>(j, 0), pairs.at<float>(j, 1)};
        result.candidates.push_back(std::move(item));
      }
      result.status = texts.empty() ? ReadStatus::kNoText : texts.size() == 1 ? ReadStatus::kSingle : ReadStatus::kMultiple;
    } catch (const cv::Exception& e) {
      result.candidates.clear();
      result.status = ReadStatus::kBackendError;
      result.diagnostics = e.what();
    }
  }
#endif
  result.elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
  return result;
}
}  // namespace qrscan::decode
