#include "CenterKillDetector.h"
#include "IconTemplate.h"
#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

CenterKillDetector::CenterKillDetector(const std::filesystem::path &path,
                                       double threshold)
    : threshold_(threshold) {
  template_ = LoadIconTemplate(path);
  if (template_.empty())
    throw std::runtime_error("Missing icon template: " + path.string());
}

CenterKillDetector::CenterKillDetector(const Config &config)
    : CenterKillDetector(config.Resolve(config.skullTemplatePath),
                         config.skullThreshold) {
  regionHeight_ = config.centerRegion.height;
}

SkullDetectionResult CenterKillDetector::Detect(const cv::Mat &roi) const {
  SkullDetectionResult out;
  if (roi.empty()) return out;
  cv::Mat gray;
  if (roi.channels() == 4) cv::cvtColor(roi, gray, cv::COLOR_BGRA2GRAY);
  else if (roi.channels() == 3) cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
  else gray = roi;
  const double scale = roi.rows / (regionHeight_ * 1080.0);
  std::vector<std::pair<cv::Rect, float>> candidates;
  for (int height = 16; height <= 20; height += 2) {
    cv::Mat templ;
    const int h = std::max(8, static_cast<int>(std::lround(height * scale)));
    const int w = std::max(8, static_cast<int>(std::lround(
        h * static_cast<double>(template_.cols) / template_.rows)));
    cv::resize(template_, templ, {w, h}, 0, 0, cv::INTER_AREA);
    if (gray.cols < w || gray.rows < h) continue;
    cv::Mat scores;
    cv::matchTemplate(gray, templ, scores, cv::TM_CCOEFF_NORMED);
    for (;;) {
      double score = 0;
      cv::Point point;
      cv::minMaxLoc(scores, nullptr, &score, nullptr, &point);
      if (!std::isfinite(score) || score < threshold_) break;
      candidates.push_back({{point.x, point.y, w, h}, static_cast<float>(score)});
      cv::Rect suppress(point.x - w / 2, point.y - h / 2, w * 2, h * 2);
      scores(suppress & cv::Rect(0, 0, scores.cols, scores.rows)).setTo(-1);
    }
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });
  for (const auto &[bounds, score] : candidates) {
    bool duplicate = false;
    for (const auto &existing : out.matches) {
      if ((existing & bounds).area() > std::min(existing.area(), bounds.area()) / 4) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;
    out.matches.push_back(bounds);
    out.confidence = std::max(out.confidence, score);
  }
  out.visible = static_cast<int>(out.matches.size());
  return out;
}

SkullDetectionResult CenterKillDetector::Process(const cv::Mat &roi) {
  auto result = Detect(roi);
  if (!initialized_) {
    previousCount_ = result.visible;
    candidateCount_ = result.visible;
    candidateFrames_ = 1;
    initialized_ = true;
    return result;
  }
  if (result.visible == candidateCount_) {
    ++candidateFrames_;
  } else {
    candidateCount_ = result.visible;
    candidateFrames_ = 1;
  }
  // A single noisy frame is not a kill confirmation. The HUD icon must be
  // present in two consecutive captures before it can confirm a feed row.
  if (candidateFrames_ >= 2 && candidateCount_ != previousCount_) {
    if (candidateCount_ > previousCount_)
      result.newKills = candidateCount_ - previousCount_;
    previousCount_ = candidateCount_;
  }
  return result;
}

SkullDetectionResult CenterKillDetector::Inspect(const cv::Mat &roi) const {
  return Detect(roi);
}

void CenterKillDetector::Reset() {
  previousCount_ = 0;
  candidateCount_ = 0;
  candidateFrames_ = 0;
  initialized_ = false;
}
