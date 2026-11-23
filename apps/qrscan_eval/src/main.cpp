// qrscan_eval：在 53 张唯一图上批量运行步骤 4，输出三路图、逐图 CSV、汇总 JSON 与报告。
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

#include "qrscan/binarize/binarizer.hpp"

namespace fs = std::filesystem;
using qrscan::Status;
using qrscan::binarize::BinarizeConfig;
using qrscan::binarize::BinarizeResult;
using qrscan::binarize::Binarizer;
using qrscan::binarize::CannyVariant;
using qrscan::binarize::EstimatorKind;

namespace {

struct Meta {
  std::string file;
  std::string category;
  std::string split;
  std::string condition;
  std::string expected_text;
  double gt_module_px = -1.0;
};

struct VariantMetrics {
  double module_px = -1.0;
  double confidence = 0.0;
  double fit_error = 0.0;
  double total_ms = 0.0;
  double scale_ms = 0.0;
  int support = 0;
  int candidates = 0;
  bool ambiguous = false;
  bool fail = true;
  bool dec_original = false;
  bool dec_equalized = false;
  bool dec_binary = false;
  bool exp_original = false;
  bool exp_equalized = false;
  bool exp_binary = false;
  double saturated_ratio = 0.0;
  double dark_ratio = 0.0;
  double illumination_cv = 0.0;
  double sharpness = 0.0;
};

struct Row {
  Meta meta;
  std::map<std::string, VariantMetrics> variants;
};

struct VariantSpec {
  std::string name;
  EstimatorKind estimator = EstimatorKind::kHybrid;
  CannyVariant canny = CannyVariant::kNorm;
};

struct Summary {
  int n = 0;
  int fail = 0;
  int ambiguous = 0;
  int with_gt = 0;
  int le_0_5 = 0;
  int le_1 = 0;
  int dec_original = 0;
  int dec_equalized = 0;
  int dec_binary = 0;
  int dec_union = 0;
  int exp_original = 0;
  int exp_equalized = 0;
  int exp_binary = 0;
  std::vector<double> errors;
  std::vector<double> total_ms;
  std::vector<double> scale_ms;
};

void PrintUsage() {
  std::cout
      << "Usage: qrscan_eval [options]\n"
      << "  --dataset <dir>     default layers/04_binarize/data/input/qr_samples\n"
      << "  --out <dir>         default layers/04_binarize/experiments/results/run_<time>\n"
      << "  --run-id <id>       default timestamp\n"
      << "  --config <json>     base binarize config\n"
      << "  --variants <list>   default scanline,canny_strict,canny_norm,canny_norm_round,canny_relaxed,hybrid\n"
      << "  --no-save           do not write per-image outputs\n"
      << "  --save-all          save illumination/equalized/binary for every variant\n";
}

std::string Trim(const std::string& value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitCsv(const std::string& line) {
  std::vector<std::string> fields;
  std::string current;
  bool quoted = false;
  for (char c : line) {
    if (c == '"') {
      quoted = !quoted;
    } else if (c == ',' && !quoted) {
      fields.push_back(Trim(current));
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  fields.push_back(Trim(current));
  return fields;
}

std::map<std::string, Meta> ReadManifest(const fs::path& path) {
  std::map<std::string, Meta> result;
  std::ifstream input(path);
  if (!input) {
    return result;
  }
  std::string line;
  if (!std::getline(input, line)) {
    return result;
  }
  const std::vector<std::string> header = SplitCsv(line);
  std::map<std::string, std::size_t> index;
  for (std::size_t i = 0; i < header.size(); ++i) {
    index[header[i]] = i;
  }
  auto get = [&](const std::vector<std::string>& fields,
                 const std::string& key) -> std::string {
    const auto it = index.find(key);
    if (it == index.end() || it->second >= fields.size()) {
      return "";
    }
    return fields[it->second];
  };
  while (std::getline(input, line)) {
    if (Trim(line).empty()) {
      continue;
    }
    const std::vector<std::string> fields = SplitCsv(line);
    Meta meta;
    meta.file = get(fields, "file");
    meta.category = get(fields, "category");
    meta.split = get(fields, "split");
    meta.condition = get(fields, "condition");
    meta.expected_text = get(fields, "expected_text");
    const std::string gt = get(fields, "gt_module_px");
    if (!gt.empty()) {
      try {
        meta.gt_module_px = std::stod(gt);
      } catch (...) {
        meta.gt_module_px = -1.0;
      }
    }
    if (!meta.file.empty()) {
      result[meta.file] = meta;
    }
  }
  return result;
}

bool ReadImageGray(const fs::path& path, cv::Mat* gray) {
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

std::string DecodeOne(cv::QRCodeDetector* detector, const cv::Mat& image) {
  if (image.empty()) {
    return "";
  }
  try {
    return detector->detectAndDecode(image);
  } catch (const cv::Exception&) {
    return "";
  }
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
    panel.second.copyTo(
        canvas(cv::Rect(x, header, panel.second.cols, panel.second.rows)));
    cv::putText(canvas, panel.first, cv::Point(x + 6, 22),
                cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 0), 1,
                cv::LINE_AA);
    x += panel.second.cols;
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

void SaveImage(const fs::path& path, const cv::Mat& image) {
  if (image.empty()) {
    return;
  }
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (!ec) {
    cv::imwrite(path.string(), image);
  }
}

std::string NowRunId() {
  std::time_t now = std::time(nullptr);
  std::tm* local = std::localtime(&now);
  char buffer[32] = {0};
  if (local != nullptr) {
    std::strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%S", local);
  }
  return buffer[0] == '\0' ? "run" : std::string(buffer);
}

double Percentile(std::vector<double> values, double p) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const double index = (values.size() - 1) * p;
  const std::size_t i = static_cast<std::size_t>(std::llround(index));
  return values[std::min(values.size() - 1, i)];
}

std::string GroupFor(const Meta& meta) {
  if (meta.category == "illum") {
    return "illum";
  }
  if (meta.category == "persp") {
    return "persp";
  }
  return "core";
}

void AddToSummary(const Meta& meta, const VariantMetrics& metrics,
                  Summary* summary) {
  ++summary->n;
  if (metrics.fail) {
    ++summary->fail;
  }
  if (metrics.ambiguous) {
    ++summary->ambiguous;
  }
  if (metrics.module_px >= 0.0 && meta.gt_module_px > 0.0) {
    const double error = std::abs(metrics.module_px - meta.gt_module_px);
    summary->errors.push_back(error);
    ++summary->with_gt;
    if (error <= 0.5) {
      ++summary->le_0_5;
    }
    if (error <= 1.0) {
      ++summary->le_1;
    }
  }
  summary->dec_original += metrics.dec_original ? 1 : 0;
  summary->dec_equalized += metrics.dec_equalized ? 1 : 0;
  summary->dec_binary += metrics.dec_binary ? 1 : 0;
  summary->dec_union +=
      (metrics.dec_original || metrics.dec_equalized || metrics.dec_binary) ? 1
                                                                           : 0;
  summary->exp_original += metrics.exp_original ? 1 : 0;
  summary->exp_equalized += metrics.exp_equalized ? 1 : 0;
  summary->exp_binary += metrics.exp_binary ? 1 : 0;
  summary->total_ms.push_back(metrics.total_ms);
  summary->scale_ms.push_back(metrics.scale_ms);
}

double MeanOf(const std::vector<double>& values) {
  if (values.empty()) {
    return 0.0;
  }
  double sum = 0.0;
  for (double value : values) {
    sum += value;
  }
  return sum / static_cast<double>(values.size());
}

double MaxOf(const std::vector<double>& values) {
  return values.empty() ? 0.0 : *std::max_element(values.begin(), values.end());
}

std::string Fixed(double value, int precision = 2) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(precision) << value;
  return stream.str();
}

void WriteReport(const fs::path& path, const std::string& run_id,
                 const fs::path& dataset, const std::vector<VariantSpec>& variants,
                 const std::vector<Row>& rows,
                 const std::map<std::string, std::map<std::string, Summary>>&
                     summaries,
                 bool save_outputs, bool save_all) {
  std::ofstream out(path);
  if (!out) {
    return;
  }
  out << "# 步骤 4 实验报告\n\n";
  out << "- run_id: `" << run_id << "`\n";
  out << "- dataset: `" << dataset.string() << "`\n";
  out << "- images: " << rows.size() << "\n";
  out << "- variants: ";
  for (std::size_t i = 0; i < variants.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << "`" << variants[i].name << "`";
  }
  out << "\n";
  out << "- OpenCV: " << CV_VERSION << "\n";
  out << "- build commit: `" << QRSCAN_GIT_COMMIT << "`\n";
  out << "- output: " << (save_outputs ? "已保存逐图输出" : "未保存逐图输出");
  out << (save_all ? "（所有变体三路）" : "（hybrid 三路，其余变体仅二值图）")
      << "\n\n";

  struct GroupInfo {
    std::string key;
    std::string title;
  };
  const std::vector<GroupInfo> groups = {
      {"illum", "光照样本"}, {"core", "基础条件"}, {"persp", "透视条件"},
      {"all", "全部"}};

  for (const GroupInfo& group : groups) {
    const auto group_it = summaries.find(group.key);
    if (group_it == summaries.end()) {
      continue;
    }
    out << "## " << group.title << "（n="
        << (group_it->second.empty() ? 0 : group_it->second.begin()->second.n)
        << "）\n\n";
    out << "| variant | fail | ambiguous | module err mean/max (px) | "
           "decode orig/eq/bin/union | p50/p95 ms |\n";
    out << "|---|---:|---:|---|---:|---:|\n";
    for (const VariantSpec& variant : variants) {
      const auto it = group_it->second.find(variant.name);
      if (it == group_it->second.end()) {
        continue;
      }
      const Summary& s = it->second;
      out << "| `" << variant.name << "` | " << s.fail << " | " << s.ambiguous
          << " | " << Fixed(MeanOf(s.errors)) << " / "
          << Fixed(MaxOf(s.errors)) << " | " << s.dec_original << "/"
          << s.dec_equalized << "/" << s.dec_binary << "/" << s.dec_union
          << " | " << Fixed(Percentile(s.total_ms, 0.5)) << " / "
          << Fixed(Percentile(s.total_ms, 0.95)) << " |\n";
    }
    out << "\n";
  }

  // 基础条件逐项模块估计（不含 40 张光照，避免表过长）。
  out << "## 基础条件逐项模块估计（px）\n\n";
  out << "| condition | gt |";
  for (const VariantSpec& variant : variants) {
    out << " " << variant.name << " |";
  }
  out << "\n|---|---:|";
  for (std::size_t i = 0; i < variants.size(); ++i) {
    out << "---:|";
  }
  out << "\n";

  std::map<std::string, std::map<std::string, double>> modules;
  std::map<std::string, double> gt;
  for (const Row& row : rows) {
    if (row.meta.category == "illum") {
      continue;
    }
    gt[row.meta.condition] = row.meta.gt_module_px;
    for (const auto& entry : row.variants) {
      modules[row.meta.condition][entry.first] = entry.second.module_px;
    }
  }
  for (const auto& entry : modules) {
    out << "| `" << entry.first << "` | " << Fixed(gt[entry.first]) << " |";
    for (const VariantSpec& variant : variants) {
      const auto it = entry.second.find(variant.name);
      out << " " << (it == entry.second.end() || it->second < 0.0
                         ? "fail"
                         : Fixed(it->second))
          << " |";
    }
    out << "\n";
  }
  out << "\n";

  out << "## 异常与失败\n\n";
  int anomalies = 0;
  for (const Row& row : rows) {
    for (const VariantSpec& variant : variants) {
      const auto it = row.variants.find(variant.name);
      if (it == row.variants.end()) {
        continue;
      }
      const VariantMetrics& metrics = it->second;
      const bool failed = metrics.module_px < 0.0;
      const bool large_error =
          metrics.module_px >= 0.0 && row.meta.gt_module_px > 0.0 &&
          std::abs(metrics.module_px - row.meta.gt_module_px) > 1.0;
      if (failed || large_error || metrics.ambiguous) {
        if (anomalies < 40) {
          out << "- `" << row.meta.file << "` / `" << variant.name << "`: ";
          if (failed) {
            out << "fail";
          } else {
            out << "module=" << Fixed(metrics.module_px) << " gt="
                << Fixed(row.meta.gt_module_px);
          }
          if (metrics.ambiguous) {
            out << ", ambiguous";
          }
          out << "\n";
        }
        ++anomalies;
      }
    }
  }
  if (anomalies == 0) {
    out << "- 无。\n";
  } else if (anomalies > 40) {
    out << "- ……共 " << anomalies << " 条，仅列前 40 条。\n";
  }

  out << "\n## 说明\n\n";
  out << "- 模块误差只在有真值的图上统计；透视条件无精确单模块真值。\n";
  out << "- `decode` 为三路非空文本计数；任务码合法性尚未接入。\n";
  out << "- `hybrid` = 同时运行 `canny_norm` 与 `scanline`；相对差 ≤20% 时优先 Canny，分歧大时以 scanline 为准。\n";
  out << "- 本报告由 `qrscan_eval` 自动生成，结论需人工复核。\n";
}

void WriteCsv(const fs::path& path, const std::vector<Row>& rows,
              const std::vector<VariantSpec>& variants) {
  std::ofstream out(path);
  if (!out) {
    return;
  }
  out << "file,category,split,condition,gt_module_px,variant,module_px,"
         "error_px,fail,ambiguous,support,candidates,fit_error,confidence,"
         "total_ms,scale_ms,decode_original,decode_equalized,decode_binary,"
         "decode_union,expected_original,expected_equalized,expected_binary,"
         "saturation_ratio,dark_ratio,illumination_cv,sharpness\n";
  out << std::fixed << std::setprecision(4);
  for (const Row& row : rows) {
    for (const VariantSpec& variant : variants) {
      const auto it = row.variants.find(variant.name);
      if (it == row.variants.end()) {
        continue;
      }
      const VariantMetrics& m = it->second;
      const double error =
          (m.module_px >= 0.0 && row.meta.gt_module_px > 0.0)
              ? std::abs(m.module_px - row.meta.gt_module_px)
              : -1.0;
      out << row.meta.file << "," << row.meta.category << "," << row.meta.split
          << "," << row.meta.condition << "," << row.meta.gt_module_px << ","
          << variant.name << "," << m.module_px << "," << error << ","
          << (m.fail ? 1 : 0) << "," << (m.ambiguous ? 1 : 0) << ","
          << m.support << "," << m.candidates << "," << m.fit_error << ","
          << m.confidence << "," << m.total_ms << "," << m.scale_ms << ","
          << (m.dec_original ? 1 : 0) << "," << (m.dec_equalized ? 1 : 0) << ","
          << (m.dec_binary ? 1 : 0) << ","
          << ((m.dec_original || m.dec_equalized || m.dec_binary) ? 1 : 0)
          << "," << (m.exp_original ? 1 : 0) << ","
          << (m.exp_equalized ? 1 : 0) << "," << (m.exp_binary ? 1 : 0) << ","
          << m.saturated_ratio << "," << m.dark_ratio << ","
          << m.illumination_cv << "," << m.sharpness << "\n";
    }
  }
}

void WriteMetricsJson(
    const fs::path& path, const std::string& run_id, const fs::path& dataset,
    const std::vector<VariantSpec>& variants, const std::vector<Row>& rows,
    const std::map<std::string, std::map<std::string, Summary>>& summaries) {
  cv::FileStorage fs(path.string(), cv::FileStorage::WRITE);
  fs << "run_id" << run_id;
  fs << "dataset" << dataset.string();
  fs << "image_count" << static_cast<int>(rows.size());
  fs << "variants" << "[";
  for (const VariantSpec& variant : variants) {
    fs << variant.name;
  }
  fs << "]";
  fs << "summary" << "{";
  for (const auto& group : summaries) {
    fs << group.first << "{";
    for (const auto& entry : group.second) {
      const Summary& s = entry.second;
      fs << entry.first << "{";
      fs << "n" << s.n;
      fs << "fail" << s.fail;
      fs << "ambiguous" << s.ambiguous;
      fs << "with_gt" << s.with_gt;
      fs << "error_mean" << MeanOf(s.errors);
      fs << "error_max" << MaxOf(s.errors);
      fs << "le_0_5" << s.le_0_5;
      fs << "le_1" << s.le_1;
      fs << "p50_ms" << Percentile(s.total_ms, 0.5);
      fs << "p95_ms" << Percentile(s.total_ms, 0.95);
      fs << "scale_p50_ms" << Percentile(s.scale_ms, 0.5);
      fs << "decode" << "{";
      fs << "original" << s.dec_original;
      fs << "equalized" << s.dec_equalized;
      fs << "binary" << s.dec_binary;
      fs << "union" << s.dec_union;
      fs << "}";
      fs << "expected" << "{";
      fs << "original" << s.exp_original;
      fs << "equalized" << s.exp_equalized;
      fs << "binary" << s.exp_binary;
      fs << "}";
      fs << "}";
    }
    fs << "}";
  }
  fs << "}";

  fs << "files" << "[";
  for (const Row& row : rows) {
    fs << "{";
    fs << "file" << row.meta.file;
    fs << "category" << row.meta.category;
    fs << "split" << row.meta.split;
    fs << "condition" << row.meta.condition;
    fs << "gt_module_px" << row.meta.gt_module_px;
    fs << "variants" << "{";
    for (const VariantSpec& variant : variants) {
      const auto it = row.variants.find(variant.name);
      if (it == row.variants.end()) {
        continue;
      }
      const VariantMetrics& m = it->second;
      fs << variant.name << "{";
      fs << "module_px" << m.module_px;
      fs << "fail" << (m.fail ? 1 : 0);
      fs << "ambiguous" << (m.ambiguous ? 1 : 0);
      fs << "support" << m.support;
      fs << "candidates" << m.candidates;
      fs << "fit_error" << m.fit_error;
      fs << "confidence" << m.confidence;
      fs << "total_ms" << m.total_ms;
      fs << "scale_ms" << m.scale_ms;
      fs << "decode" << "{";
      fs << "original" << (m.dec_original ? 1 : 0);
      fs << "equalized" << (m.dec_equalized ? 1 : 0);
      fs << "binary" << (m.dec_binary ? 1 : 0);
      fs << "union" << ((m.dec_original || m.dec_equalized || m.dec_binary) ? 1 : 0);
      fs << "}";
      fs << "quality" << "{";
      fs << "saturation_ratio" << m.saturated_ratio;
      fs << "dark_ratio" << m.dark_ratio;
      fs << "illumination_cv" << m.illumination_cv;
      fs << "sharpness" << m.sharpness;
      fs << "}";
      fs << "}";
    }
    fs << "}";
    fs << "}";
  }
  fs << "]";
}

void WriteRunMeta(const fs::path& path, const std::string& run_id,
                  const fs::path& dataset, const fs::path& manifest,
                  const std::vector<VariantSpec>& variants, std::size_t images,
                  bool save_outputs, bool save_all) {
  cv::FileStorage fs(path.string(), cv::FileStorage::WRITE);
  std::time_t now = std::time(nullptr);
  std::tm* local = std::localtime(&now);
  char buffer[32] = {0};
  if (local != nullptr) {
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", local);
  }
  fs << "run_id" << run_id;
  fs << "timestamp" << buffer;
  fs << "api_version" << qrscan::kApiVersion;
  fs << "git_commit" << QRSCAN_GIT_COMMIT;
  fs << "opencv_version" << CV_VERSION;
  fs << "dataset" << dataset.string();
  fs << "manifest" << manifest.string();
  fs << "image_count" << static_cast<int>(images);
  fs << "variants" << "[";
  for (const VariantSpec& variant : variants) {
    fs << variant.name;
  }
  fs << "]";
  fs << "save_outputs" << (save_outputs ? 1 : 0);
  fs << "save_all" << (save_all ? 1 : 0);
}

bool BuildVariants(const std::string& list,
                   std::vector<VariantSpec>* variants) {
  std::map<std::string, VariantSpec> known;
  known["scanline"] = {"scanline", EstimatorKind::kScanline,
                       CannyVariant::kNorm};
  known["canny_strict"] = {"canny_strict", EstimatorKind::kCanny,
                           CannyVariant::kStrict};
  known["canny_norm"] = {"canny_norm", EstimatorKind::kCanny,
                          CannyVariant::kNorm};
  known["canny_norm_round"] = {"canny_norm_round", EstimatorKind::kCanny,
                                CannyVariant::kNormRound};
  known["canny_relaxed"] = {"canny_relaxed", EstimatorKind::kCanny,
                             CannyVariant::kRelaxed};
  known["hybrid"] = {"hybrid", EstimatorKind::kHybrid, CannyVariant::kNorm};

  std::stringstream stream(list);
  std::string token;
  while (std::getline(stream, token, ',')) {
    token = Trim(token);
    const auto it = known.find(token);
    if (it == known.end()) {
      return false;
    }
    variants->push_back(it->second);
  }
  return !variants->empty();
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
    if (key == "--no-save") {
      options["no-save"] = "1";
      continue;
    }
    if (key == "--save-all") {
      options["save-all"] = "1";
      continue;
    }
    if (key.size() < 3 || key.rfind("--", 0) != 0 || i + 1 >= argc) {
      std::cerr << "Bad argument: " << key << "\n";
      PrintUsage();
      return 2;
    }
    options[key.substr(2)] = argv[++i];
  }

  const std::string run_id =
      options.count("run-id") != 0 ? options["run-id"] : NowRunId();
  const fs::path dataset =
      options.count("dataset") != 0
          ? fs::path(options["dataset"])
          : fs::path("layers/04_binarize/data/input/qr_samples");
  const fs::path out_dir =
      options.count("out") != 0
          ? fs::path(options["out"])
          : fs::path("layers/04_binarize/experiments/results") /
                ("run_" + run_id);

  BinarizeConfig base;
  if (options.count("config") != 0) {
    std::string error;
    if (!qrscan::binarize::LoadBinarizeConfig(options["config"], &base,
                                              &error)) {
      std::cerr << "Config error: " << error << "\n";
      return 2;
    }
  }

  std::vector<VariantSpec> variants;
  const std::string variant_list =
      options.count("variants") != 0
          ? options["variants"]
          : "scanline,canny_strict,canny_norm,canny_norm_round,canny_relaxed,hybrid";
  if (!BuildVariants(variant_list, &variants)) {
    std::cerr << "Bad variant list: " << variant_list << "\n";
    return 2;
  }

  const bool save_outputs = options.count("no-save") == 0;
  const bool save_all = options.count("save-all") != 0;

  const fs::path manifest = dataset / "manifest.csv";
  const std::map<std::string, Meta> meta_by_file = ReadManifest(manifest);
  if (meta_by_file.empty()) {
    std::cerr << "Cannot read manifest: " << manifest.string() << "\n";
    return 2;
  }

  std::vector<fs::path> images;
  for (const auto& entry : fs::directory_iterator(dataset)) {
    if (entry.is_regular_file() &&
        entry.path().extension() == ".png") {
      images.push_back(entry.path());
    }
  }
  std::sort(images.begin(), images.end());
  if (images.empty()) {
    std::cerr << "No PNG images in " << dataset.string() << "\n";
    return 2;
  }

  std::error_code ec;
  fs::create_directories(out_dir, ec);
  if (ec) {
    std::cerr << "Cannot create output directory: " << ec.message() << "\n";
    return 2;
  }

  const Binarizer binarizer;
  cv::QRCodeDetector detector;
  std::vector<Row> rows;

  for (const fs::path& image_path : images) {
    const std::string file = image_path.filename().string();
    const auto meta_it = meta_by_file.find(file);
    if (meta_it == meta_by_file.end()) {
      std::cerr << "Skipping unregistered image: " << file << "\n";
      continue;
    }
    cv::Mat gray;
    if (!ReadImageGray(image_path, &gray)) {
      std::cerr << "Cannot read: " << file << "\n";
      continue;
    }

    Row row;
    row.meta = meta_it->second;

    if (save_outputs) {
      SaveImage(out_dir / "input_gray" / (image_path.stem().string() + ".png"),
                gray);
    }

    for (const VariantSpec& variant : variants) {
      BinarizeConfig config = base;
      config.estimator = variant.estimator;
      config.canny_variant = variant.canny;

      std::vector<double> total_times;
      std::vector<double> scale_times;
      BinarizeResult result;
      for (int repeat = 0; repeat < 3; ++repeat) {
        result = binarizer.Process(gray, config);
        total_times.push_back(result.elapsed_ms);
        scale_times.push_back(result.scale.elapsed_ms);
      }
      std::sort(total_times.begin(), total_times.end());
      std::sort(scale_times.begin(), scale_times.end());

      VariantMetrics metrics;
      metrics.module_px =
          (result.status == Status::kOk && result.scale.module_px > 0.0)
              ? result.scale.module_px
              : -1.0;
      metrics.fail = metrics.module_px < 0.0;
      metrics.ambiguous = result.scale.ambiguous;
      metrics.support = result.scale.support;
      metrics.candidates = result.scale.candidates;
      metrics.fit_error = result.scale.fit_error;
      metrics.confidence = result.scale.confidence;
      metrics.total_ms = total_times[1];
      metrics.scale_ms = scale_times[1];
      metrics.saturated_ratio = result.quality.saturated_ratio;
      metrics.dark_ratio = result.quality.dark_ratio;
      metrics.illumination_cv = result.quality.illumination_cv;
      metrics.sharpness = result.quality.sharpness;

      const std::string original_text = DecodeOne(&detector, gray);
      const std::string equalized_text = DecodeOne(&detector, result.equalized);
      const std::string binary_text = DecodeOne(&detector, result.binary);
      metrics.dec_original = !original_text.empty();
      metrics.dec_equalized = !equalized_text.empty();
      metrics.dec_binary = !binary_text.empty();
      metrics.exp_original = original_text == row.meta.expected_text;
      metrics.exp_equalized = equalized_text == row.meta.expected_text;
      metrics.exp_binary = binary_text == row.meta.expected_text;

      if (save_outputs) {
        const std::string stem = image_path.stem().string();
        const fs::path variant_dir = out_dir / variant.name;
        const bool save_three = save_all || variant.name == "hybrid";
        if (save_three) {
          SaveImage(variant_dir / (stem + "_illumination.png"),
                    result.illumination);
          SaveImage(variant_dir / (stem + "_equalized.png"),
                    result.equalized);
        }
        SaveImage(variant_dir / (stem + "_binary.png"), result.binary);
        if (save_three) {
          SaveImage(out_dir / "montage" / ("hybrid_" + stem + ".png"),
                    MakeMontage(result));
        }
      }

      row.variants[variant.name] = metrics;
    }
    rows.push_back(row);
    std::cout << "processed " << file << " (" << row.variants.size()
              << " variants)\n";
  }

  std::map<std::string, std::map<std::string, Summary>> summaries;
  for (const Row& row : rows) {
    for (const auto& entry : row.variants) {
      AddToSummary(row.meta, entry.second, &summaries[GroupFor(row.meta)][entry.first]);
      AddToSummary(row.meta, entry.second, &summaries["all"][entry.first]);
    }
  }

  WriteCsv(out_dir / "per_image.csv", rows, variants);
  WriteMetricsJson(out_dir / "metrics.json", run_id, dataset, variants, rows,
                   summaries);
  WriteReport(out_dir / "report.md", run_id, dataset, variants, rows, summaries,
              save_outputs, save_all);
  WriteRunMeta(out_dir / "run_meta.json", run_id, dataset, manifest, variants,
               rows.size(), save_outputs, save_all);

  std::cout << "\nSummary (all):\n";
  for (const VariantSpec& variant : variants) {
    const Summary& s = summaries["all"][variant.name];
    std::cout << "  " << variant.name << ": fail=" << s.fail
              << " ambiguous=" << s.ambiguous << " err_mean="
              << Fixed(MeanOf(s.errors)) << " decode_union=" << s.dec_union
              << "/" << s.n << "\n";
  }
  std::cout << "Output: " << out_dir.string() << "\n";
  return 0;
}
