// 步骤 4 单元/冒烟测试：无第三方测试框架，返回非 0 表示失败。
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "qrscan/binarize/binarizer.hpp"

namespace fs = std::filesystem;
using qrscan::Status;
using qrscan::binarize::BinarizeConfig;
using qrscan::binarize::BinarizeResult;
using qrscan::binarize::Binarizer;
using qrscan::binarize::CannyVariant;
using qrscan::binarize::EstimatorKind;

namespace {

int g_failures = 0;

void Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

bool ReadGray(const fs::path& path, cv::Mat* gray) {
  const cv::Mat image = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
  if (image.empty() || image.depth() != CV_8U) {
    return false;
  }
  if (image.channels() == 1) {
    *gray = image;
    return true;
  }
  if (image.channels() == 3) {
    cv::cvtColor(image, *gray, cv::COLOR_BGR2GRAY);
    return true;
  }
  if (image.channels() == 4) {
    cv::cvtColor(image, *gray, cv::COLOR_BGRA2GRAY);
    return true;
  }
  return false;
}

BinarizeConfig BaseConfig() {
  BinarizeConfig config;
  config.estimator = EstimatorKind::kHybrid;
  config.canny_variant = CannyVariant::kNorm;
  config.denoise = qrscan::binarize::DenoiseKind::kNone;
  return config;
}

void CheckModule(const BinarizeResult& result, double expected,
                 double tolerance, const std::string& label) {
  Check(result.status == Status::kOk, label + ": status ok");
  Check(result.scale.module_px > 0.0, label + ": module > 0");
  Check(std::abs(result.scale.module_px - expected) <= tolerance,
        label + ": module " + std::to_string(result.scale.module_px) +
            " expected " + std::to_string(expected) + " +/- " +
            std::to_string(tolerance));
  Check(!result.equalized.empty(), label + ": equalized not empty");
  Check(!result.binary.empty(), label + ": binary not empty");
  Check(result.binary.type() == CV_8UC1, label + ": binary CV_8UC1");
}

}  // namespace

int main() {
  const fs::path data_dir = QRSCAN_TEST_DATA_DIR;
  const Binarizer binarizer;

  cv::Mat clean;
  Check(ReadGray(data_dir / "clean_200_00.png", &clean),
        "read clean_200_00.png");

  BinarizeConfig config = BaseConfig();

  config.estimator = EstimatorKind::kScanline;
  BinarizeResult scanline = binarizer.Process(clean, config);
  CheckModule(scanline, 8.0, 0.5, "scanline clean_200");

  config.estimator = EstimatorKind::kCanny;
  config.canny_variant = CannyVariant::kNorm;
  BinarizeResult canny = binarizer.Process(clean, config);
  CheckModule(canny, 8.0, 0.7, "canny_norm clean_200");

  config.estimator = EstimatorKind::kHybrid;
  BinarizeResult hybrid = binarizer.Process(clean, config);
  CheckModule(hybrid, 8.0, 0.5, "hybrid clean_200");

  // hybrid 必须在强模糊上回退到扫描线，而不是接受 Canny 的 3.0–3.4 px。
  cv::Mat blur;
  Check(ReadGray(data_dir / "blur_1_00.png", &blur), "read blur_1_00.png");
  BinarizeResult blur_result = binarizer.Process(blur, config);
  CheckModule(blur_result, 9.6, 0.7, "hybrid blur_1");

  // 非灰度输入必须明确报错，而不是隐式转换。
  cv::Mat color(64, 64, CV_8UC3, cv::Scalar(255, 255, 255));
  BinarizeResult invalid = binarizer.Process(color, config);
  Check(invalid.status == Status::kInvalidInput, "color input rejected");

  // 确定性：同一输入 + 配置，两次二值图逐像素一致。
  BinarizeResult first = binarizer.Process(clean, config);
  BinarizeResult second = binarizer.Process(clean, config);
  Check(!first.binary.empty() && !second.binary.empty(), "determinism: binary");
  if (!first.binary.empty() && !second.binary.empty()) {
    Check(cv::norm(first.binary, second.binary, cv::NORM_INF) == 0.0,
          "determinism: binary bit-exact");
  }

  // 全数据集冒烟：统计成功数，不要求每张都正确。
  int total = 0;
  int success = 0;
  for (const auto& entry : fs::directory_iterator(data_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".png") {
      continue;
    }
    cv::Mat gray;
    if (!ReadGray(entry.path(), &gray)) {
      continue;
    }
    ++total;
    const BinarizeResult result = binarizer.Process(gray, BaseConfig());
    if (result.status == Status::kOk && result.scale.module_px > 0.0 &&
        !result.binary.empty()) {
      ++success;
    }
  }
  std::cout << "dataset smoke: " << success << "/" << total << " succeeded\n";
  Check(total >= 53, "dataset has at least 53 images");
  Check(success >= 51, "dataset smoke success >= 51");

  if (g_failures == 0) {
    std::cout << "ALL TESTS PASSED\n";
    return 0;
  }
  std::cerr << g_failures << " test(s) failed\n";
  return 1;
}
