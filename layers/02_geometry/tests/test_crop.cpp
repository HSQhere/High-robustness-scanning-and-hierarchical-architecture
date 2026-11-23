#include "qrscan/geometry/crop.hpp"
#include <cmath>
#include <iostream>
#include <limits>

using namespace qrscan;
using namespace qrscan::geometry;
#define CHECK(expr) do { if (!(expr)) { std::cerr << __LINE__ << ": " #expr << '\n'; return 1; } } while (false)

int main() {
  cv::Mat pixels(479, 767, CV_8UC3);
  cv::randu(pixels, 0, 255);
  Quad q;
  q.valid = true;
  q.points = {{{110, 96}, {210, 101}, {206, 206}, {100, 203}}};
  auto crop = CropOriginal(pixels, q, 0);
  CHECK(crop.roi.status == Status::kOk && crop.bounds == cv::Rect(100, 96, 111, 111));
  CHECK(crop.roi.original_size == pixels.size() && crop.roi.scale_from_original == 1 && !crop.clipped);
  CHECK(cv::norm(crop.roi.bgr, pixels(crop.bounds), cv::NORM_INF) == 0);
  CHECK(crop.view_to_original * cv::Vec3d(10, 0, 1) == cv::Vec3d(110, 96, 1));
  crop.roi.bgr.setTo(0);
  CHECK(cv::norm(pixels(crop.bounds), cv::NORM_INF) > 0);

  // Rounded resize with asymmetric padding. Independent forward pixel-center transform.
  cv::Rect content(17, 120, 603, 377);
  auto small = q;
  for (auto& p : small.points) {
    p.x = float((p.x + .5) * content.width / pixels.cols + content.x - .5);
    p.y = float((p.y + .5) * content.height / pixels.rows + content.y - .5);
  }
  auto mapped = CropFromDetector(pixels, small, content);
  CHECK(mapped.roi.status == Status::kOk);
  for (int i = 0; i < 4; ++i) CHECK(cv::norm(q.points[i] - mapped.roi.source_quad.points[i]) < .0001);
  CHECK(mapped.bounds == CropOriginal(pixels, q).bounds);
  auto shifted = q;
  for (auto& p : shifted.points) p -= cv::Point2f(100, 96);
  CHECK(CropOriginal(pixels, shifted).clipped);
  CHECK(CropOriginal(pixels, shifted).bounds.tl() == cv::Point(0, 0));
  CHECK(CropFromDetector(pixels, small, {}).roi.status == Status::kInvalidInput);
  CHECK(CropOriginal(pixels, {}).roi.status == Status::kNotFound);
  CHECK(CropOriginal({}, q).roi.status == Status::kInvalidInput);
  CHECK(CropOriginal(pixels, q, -0.1).roi.status == Status::kInvalidInput);
  CHECK(CropOriginal(pixels, q, std::numeric_limits<double>::quiet_NaN()).roi.status == Status::kInvalidInput);
  for (auto index : {0, 1, 2}) {
    Quad bad = q; std::swap(bad.points[index], bad.points[index+1]);
    CHECK(CropOriginal(pixels, bad).roi.status == Status::kInvalidInput);
  }
  auto bad = q; bad.points[1] = bad.points[0];
  CHECK(CropOriginal(pixels, bad).roi.status == Status::kInvalidInput);
  bad = q; bad.points[0].x = std::numeric_limits<float>::infinity();
  CHECK(CropOriginal(pixels, bad).roi.status == Status::kInvalidInput);
  bad = q; bad.points[0].x = -4;
  CHECK(CropOriginal(pixels, bad).roi.status == Status::kInvalidInput);
  return 0;
}
