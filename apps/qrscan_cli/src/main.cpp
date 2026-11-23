// qrscan_cli：单图运行步骤 4，保存三路结果与 result.json。
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

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
using qrscan::binarize::DenoiseKind;
using qrscan::binarize::EstimatorKind;

namespace {

void PrintUsage() {
  std::cout
      << "Usage: qrscan_cli --image <path> [--out <dir>] [options]\n"
      << "Options:\n"
      << "  --config <json>          read binarize section from JSON\n"
      << "  --estimator <name>       scanline | canny | hybrid |\n"
      << "                           canny_strict | canny_norm | canny_norm_round | canny_relaxed\n"
      << "  --canny-variant <name>   strict | norm | norm_round | relaxed\n"
      << "  --denoise <name>         none | median3 | bilateral | gaussian\n"
      << "  --mean-factor <double>   mean filter factor (default 1.0)\n"
      << "  --close-factor <double>  closing factor (default 3.0)\n"
      << "  --scale-constant <num>   equalization constant (default 200)\n"
      << "  --adaptive-c <double>    adaptive threshold C (default 0)\n";
}

bool ReadImageGray(const std::string& path, cv::Mat* gray, std::string* error) {
  const cv::Mat image = cv::imread(path, cv::IMREAD_UNCHANGED);
  if (image.empty()) {
    *error = "cannot read image: " + path;
    return false;
  }
  if (image.depth() != CV_8U) {
    *error = "only 8-bit images are supported";
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
  *error = "unsupported channel count: " + std::to_string(image.channels());
  return false;
}

cv::Mat ToBgr(const cv::Mat& image) {
  if (image.empty()) {
    return cv::Mat(32, 32, CV_8UC3, cv::Scalar(255, 255, 255));
  }
  if (image.channels() == 1) {
    cv::Mat bgr;
    cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
    return bgr;
  }
  return image;
}

cv::Mat MakeMontage(const BinarizeResult& result) {
  std::vector<std::pair<std::string, cv::Mat>> panels;
  panels.emplace_back("input", ToBgr(result.original));
  panels.emplace_back("illumination", ToBgr(result.illumination));
  panels.emplace_back("equalized", ToBgr(result.equalized));
  panels.emplace_back("binary", ToBgr(result.binary));

  int width = 0;
  int height = 0;
  for (const auto& panel : panels) {
    width += panel.second.cols;
    height = std::max(height, panel.second.rows);
  }
  const int header = 34;
  cv::Mat canvas(height + header, width, CV_8UC3, cv::Scalar(255, 255, 255));
  int x = 0;
  for (const auto& panel : panels) {
    const cv::Mat& image = panel.second;
    image.copyTo(canvas(cv::Rect(x, header, image.cols, image.rows)));
    cv::putText(canvas, panel.first, cv::Point(x + 6, 22),
                cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 0), 1,
                cv::LINE_AA);
    x += image.cols;
  }
  const std::string info =
      "module=" + std::to_string(result.scale.module_px) +
      " support=" + std::to_string(result.scale.support) +
      " status=" + qrscan::StatusName(result.status);
  cv::putText(canvas, info, cv::Point(6, height + 26),
              cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 0, 0), 1,
              cv::LINE_AA);
  return canvas;
}

void WriteResultJson(const std::string& path, const std::string& image_path,
                     const cv::Mat& gray, const BinarizeResult& result,
                     const BinarizeConfig& config) {
  cv::FileStorage fs(path, cv::FileStorage::WRITE);
  fs << "api_version" << qrscan::kApiVersion;
  fs << "status" << qrscan::StatusName(result.status);
  fs << "image" << "{";
  fs << "path" << image_path;
  fs << "width" << gray.cols;
  fs << "height" << gray.rows;
  fs << "}";
  fs << "estimator" << qrscan::binarize::EstimatorKindName(config.estimator);
  fs << "canny_variant" << qrscan::binarize::CannyVariantName(config.canny_variant);
  fs << "denoise" << qrscan::binarize::DenoiseKindName(config.denoise);
  fs << "scale" << "{";
  fs << "method" << qrscan::binarize::EstimatorKindName(result.scale.method);
  fs << "module_px" << result.scale.module_px;
  fs << "confidence" << result.scale.confidence;
  fs << "support" << result.scale.support;
  fs << "candidates" << result.scale.candidates;
  fs << "fit_error" << result.scale.fit_error;
  fs << "ambiguous" << (result.scale.ambiguous ? 1 : 0);
  fs << "elapsed_ms" << result.scale.elapsed_ms;
  fs << "diagnostics" << result.scale.diagnostics;
  fs << "}";
  fs << "timing_ms" << "{";
  fs << "total" << result.elapsed_ms;
  fs << "denoise" << result.denoise_ms;
  fs << "equalize" << result.equalize_ms;
  fs << "binary" << result.binary_ms;
  fs << "}";
  fs << "quality" << "{";
  fs << "saturated_ratio" << result.quality.saturated_ratio;
  fs << "dark_ratio" << result.quality.dark_ratio;
  fs << "mean_gray" << result.quality.mean_gray;
  fs << "std_gray" << result.quality.std_gray;
  fs << "illumination_mean" << result.quality.illumination_mean;
  fs << "illumination_std" << result.quality.illumination_std;
  fs << "illumination_cv" << result.quality.illumination_cv;
  fs << "local_contrast" << result.quality.local_contrast;
  fs << "sharpness" << result.quality.sharpness;
  fs << "missing_area_ratio" << result.quality.missing_area_ratio;
  fs << "binary_dark_ratio" << result.quality.binary_dark_ratio;
  fs << "}";
  fs << "diagnostics" << result.diagnostics;
}

bool SetEstimator(const std::string& value, BinarizeConfig* config) {
  if (value == "scanline") {
    config->estimator = EstimatorKind::kScanline;
  } else if (value == "canny") {
    config->estimator = EstimatorKind::kCanny;
  } else if (value == "hybrid") {
    config->estimator = EstimatorKind::kHybrid;
  } else if (value == "canny_strict") {
    config->estimator = EstimatorKind::kCanny;
    config->canny_variant = CannyVariant::kStrict;
  } else if (value == "canny_norm") {
    config->estimator = EstimatorKind::kCanny;
    config->canny_variant = CannyVariant::kNorm;
  } else if (value == "canny_norm_round") {
    config->estimator = EstimatorKind::kCanny;
    config->canny_variant = CannyVariant::kNormRound;
  } else if (value == "canny_relaxed") {
    config->estimator = EstimatorKind::kCanny;
    config->canny_variant = CannyVariant::kRelaxed;
  } else {
    return false;
  }
  return true;
}

bool SetCannyVariant(const std::string& value, BinarizeConfig* config) {
  if (value == "strict") {
    config->canny_variant = CannyVariant::kStrict;
  } else if (value == "norm") {
    config->canny_variant = CannyVariant::kNorm;
  } else if (value == "norm_round") {
    config->canny_variant = CannyVariant::kNormRound;
  } else if (value == "relaxed") {
    config->canny_variant = CannyVariant::kRelaxed;
  } else {
    return false;
  }
  return true;
}

bool SetDenoise(const std::string& value, BinarizeConfig* config) {
  if (value == "none") {
    config->denoise = DenoiseKind::kNone;
  } else if (value == "median3") {
    config->denoise = DenoiseKind::kMedian3;
  } else if (value == "bilateral") {
    config->denoise = DenoiseKind::kBilateral;
  } else if (value == "gaussian") {
    config->denoise = DenoiseKind::kGaussian;
  } else {
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::map<std::string, std::string> options;
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    if (key == "--help" || key == "-h") {
      PrintUsage();
      return 0;
    }
    if (key.size() < 3 || key.rfind("--", 0) != 0) {
      std::cerr << "Unknown argument: " << key << "\n";
      PrintUsage();
      return 2;
    }
    if (i + 1 >= argc) {
      std::cerr << "Missing value for " << key << "\n";
      return 2;
    }
    options[key.substr(2)] = argv[++i];
  }

  const auto image_it = options.find("image");
  if (image_it == options.end()) {
    PrintUsage();
    return 2;
  }

  BinarizeConfig config;
  if (options.count("config") != 0) {
    std::string error;
    if (!qrscan::binarize::LoadBinarizeConfig(options["config"], &config,
                                              &error)) {
      std::cerr << "Config error: " << error << "\n";
      return 2;
    }
  }

  if (options.count("estimator") != 0 &&
      !SetEstimator(options["estimator"], &config)) {
    std::cerr << "Unknown estimator: " << options["estimator"] << "\n";
    return 2;
  }
  if (options.count("canny-variant") != 0 &&
      !SetCannyVariant(options["canny-variant"], &config)) {
    std::cerr << "Unknown canny variant: " << options["canny-variant"] << "\n";
    return 2;
  }
  if (options.count("denoise") != 0 &&
      !SetDenoise(options["denoise"], &config)) {
    std::cerr << "Unknown denoise: " << options["denoise"] << "\n";
    return 2;
  }
  try {
    if (options.count("mean-factor") != 0) {
      config.mean_filter_factor = std::stod(options["mean-factor"]);
    }
    if (options.count("close-factor") != 0) {
      config.close_factor = std::stod(options["close-factor"]);
    }
    if (options.count("scale-constant") != 0) {
      config.scale_constant = std::stod(options["scale-constant"]);
    }
    if (options.count("adaptive-c") != 0) {
      config.adaptive_c = std::stod(options["adaptive-c"]);
    }
  } catch (const std::exception& exception) {
    std::cerr << "Bad numeric option: " << exception.what() << "\n";
    return 2;
  }

  cv::Mat gray;
  std::string error;
  if (!ReadImageGray(image_it->second, &gray, &error)) {
    std::cerr << error << "\n";
    return 2;
  }

  const Binarizer binarizer;
  const BinarizeResult result = binarizer.Process(gray, config);

  const fs::path out_dir =
      options.count("out") != 0 ? fs::path(options["out"]) : fs::path("out");
  std::error_code ec;
  fs::create_directories(out_dir, ec);
  if (ec) {
    std::cerr << "Cannot create output directory: " << ec.message() << "\n";
    return 2;
  }

  cv::imwrite((out_dir / "input_gray.png").string(), gray);
  if (!result.illumination.empty()) {
    cv::imwrite((out_dir / "illumination.png").string(), result.illumination);
  }
  if (!result.equalized.empty()) {
    cv::imwrite((out_dir / "equalized.png").string(), result.equalized);
  }
  if (!result.binary.empty()) {
    cv::imwrite((out_dir / "binary.png").string(), result.binary);
  }
  cv::imwrite((out_dir / "montage.png").string(), MakeMontage(result));
  WriteResultJson((out_dir / "result.json").string(), image_it->second, gray,
                  result, config);

  std::cout << "status=" << qrscan::StatusName(result.status)
            << " module_px=" << result.scale.module_px
            << " support=" << result.scale.support
            << " confidence=" << result.scale.confidence
            << " elapsed_ms=" << result.elapsed_ms << "\n";
  std::cout << "output=" << out_dir.string() << "\n";
  return result.status == Status::kOk ? 0 : 1;
}
