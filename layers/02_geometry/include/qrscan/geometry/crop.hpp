#pragma once

#include "qrscan/common/types.hpp"

namespace qrscan::geometry {
struct CropResult {
  RoiImage roi;                        // Inspect roi.status before using the crop.
  cv::Rect bounds;                     // Half-open rectangle in the original frame.
  cv::Matx33d view_to_original = cv::Matx33d::eye();
  bool clipped = false;                // True if requested padding exceeded the sensor area.
  std::string diagnostics;
};

// Inputs are clockwise convex symbol corners (image coordinates); no quiet zone in the quad.
// Native BGR pixels are copied and never resampled. Padding is a fraction per bounding-box side.
CropResult CropOriginal(const cv::Mat& original, const Quad& original_quad, double padding = 0.08);

// Pixel-center coordinates for cv::resize + integer letterbox padding. `content` is the
// actual resized source rectangle in the detector canvas, not the entire padded canvas.
CropResult CropFromDetector(const cv::Mat& original, const Quad& detector_quad,
                            cv::Rect content, double padding = 0.08);
}  // namespace qrscan::geometry
