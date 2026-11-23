#include "qrscan/geometry/crop.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>

namespace qrscan::geometry {
namespace {
CropResult Failure(Status status, const char* reason) {
  CropResult out;
  out.roi.status = status;
  out.diagnostics = reason;
  return out;
}
}  // namespace

CropResult CropOriginal(const cv::Mat& original, const Quad& quad, double padding) {
  if (original.empty() || original.dims != 2 || original.type() != CV_8UC3)
    return Failure(Status::kInvalidInput, "Expected a nonempty CV_8UC3 frame");
  if (!std::isfinite(padding) || padding < 0 || padding > 1)
    return Failure(Status::kInvalidInput, "Padding must be in [0,1]");
  if (!quad.valid) return Failure(Status::kNotFound, "No ROI proposal");

  double x0 = original.cols, x1 = -1, y0 = original.rows, y1 = -1;
  for (const auto& pt : quad.points) {
    const double x = pt.x, y = pt.y;
    if (!std::isfinite(x) || !std::isfinite(y) || x < -0.5 || x > original.cols - 0.5 ||
        y < -0.5 || y > original.rows - 0.5)
      return Failure(Status::kInvalidInput, "Symbol corner outside source");
    x0 = std::min(x0, x); x1 = std::max(x1, x);
    y0 = std::min(y0, y); y1 = std::max(y1, y);
  }
  try {
    if (!cv::isContourConvex(quad.points) || cv::contourArea(quad.points, true) <= 0)
      return Failure(Status::kInvalidInput, "Expected a clockwise convex symbol quad");
    const double dx = padding * (x1 - x0), dy = padding * (y1 - y0);
    // Include both extreme pixel centers; clamp in double before narrowing.
    const double l = std::floor(x0 - dx), r = std::ceil(x1 + dx) + 1;
    const double t = std::floor(y0 - dy), b = std::ceil(y1 + dy) + 1;
    const int x = static_cast<int>(std::max(0., l)), y = static_cast<int>(std::max(0., t));
    const int right = static_cast<int>(std::min<double>(original.cols, r));
    const int bottom = static_cast<int>(std::min<double>(original.rows, b));
    CropResult out;
    out.bounds = {x, y, right - x, bottom - y};
    out.roi.bgr = original(out.bounds).clone();
    out.roi.original_size = original.size();
    out.roi.source_quad = quad;
    out.view_to_original = {1, 0, double(x), 0, 1, double(y), 0, 0, 1};
    out.clipped = l < 0 || t < 0 || r > original.cols || b > original.rows;
    return out;
  } catch (const cv::Exception& e) {
    return Failure(Status::kInternalError, e.what());
  }
}

CropResult CropFromDetector(const cv::Mat& original, const Quad& detector_quad,
                            cv::Rect content, double padding) {
  if (content.x < 0 || content.y < 0 || content.width <= 0 || content.height <= 0)
    return Failure(Status::kInvalidInput, "Invalid resized content rectangle");
  const double sx = double(original.cols) / content.width, sy = double(original.rows) / content.height;
  Quad mapped = detector_quad;
  for (auto& p : mapped.points) {
    p.x = static_cast<float>((double(p.x) - content.x + 0.5) * sx - 0.5);
    p.y = static_cast<float>((double(p.y) - content.y + 0.5) * sy - 0.5);
  }
  return CropOriginal(original, mapped, padding);
}
}  // namespace qrscan::geometry
