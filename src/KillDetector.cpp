#include "KillDetector.h"
#include "Utils.h"
#include <algorithm>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
KillDetector::KillDetector(const Config &c) : config_(c) {
  templ_ = cv::imread(config_.Resolve(c.templatePath).string(),
                      cv::IMREAD_GRAYSCALE);
  if (templ_.empty())
    throw std::runtime_error("player template is missing or unreadable");
}
uint64_t KillDetector::DHash(const cv::Mat &src) {
  cv::Mat g, small;
  if (src.channels() == 1)
    g = src;
  else
    cv::cvtColor(src, g, src.channels() == 4 ? cv::COLOR_BGRA2GRAY : cv::COLOR_BGR2GRAY);
  cv::resize(g, small, {9, 8}, 0, 0, cv::INTER_AREA);
  uint64_t h = 0;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
      h = (h << 1) | (small.at<uchar>(y, x) > small.at<uchar>(y, x + 1));
  return h;
}
std::vector<DetectedRow> KillDetector::Detect(const cv::Mat &roi) const {
  cv::Mat g;
  if (roi.channels() == 4)
    cv::cvtColor(roi, g, cv::COLOR_BGRA2GRAY);
  else if (roi.channels() == 3)
    cv::cvtColor(roi, g, cv::COLOR_BGR2GRAY);
  else
    g = roi;
  if (g.cols < templ_.cols || g.rows < templ_.rows)
    return {};
  cv::Mat result;
  cv::matchTemplate(g, templ_, result, cv::TM_CCOEFF_NORMED);
  std::vector<cv::Point> pts;
  std::vector<float> conf;
  for (;;) {
    double mx;
    cv::Point p;
    cv::minMaxLoc(result, nullptr, &mx, nullptr, &p);
    if (mx < config_.threshold)
      break;
    pts.push_back(p);
    conf.push_back((float)mx);
    cv::rectangle(result,
                  {std::max(0, p.x - templ_.cols / 2),
                   std::max(0, p.y - templ_.rows / 2),
                   std::min(result.cols - std::max(0, p.x - templ_.cols / 2),
                            templ_.cols * 2),
                   std::min(result.rows - std::max(0, p.y - templ_.rows / 2),
                            templ_.rows * 2)},
                  cv::Scalar(-1), cv::FILLED);
  }
  std::vector<DetectedRow> rows;
  for (size_t i = 0; i < pts.size(); ++i) {
    // The killer name is at the left; a player name in the victim column is not a kill.
    if (pts[i].x > roi.cols * .20) continue;
    bool duplicate = false;
    for (auto &r : rows)
      if (std::abs(r.bounds.y - pts[i].y) < templ_.rows / 2) {
        duplicate = true;
        break;
      }
    if (duplicate)
      continue;
    int pad = std::max(4, roi.cols / 200);
    int x = std::max(0, pts[i].x - pad);
    int y = std::max(0, pts[i].y - templ_.rows / 3);
    int w = std::min(roi.cols - x, std::max(templ_.cols * 5, roi.cols * 3 / 4));
    int h = std::min(roi.rows - y, templ_.rows * 5 / 3);
    cv::Rect b{x, y, w, h};
    cv::Mat clean;
    if (roi.channels() == 4)
      cv::cvtColor(roi(b), clean, cv::COLOR_BGRA2GRAY);
    else if (roi.channels() == 3)
      cv::cvtColor(roi(b), clean, cv::COLOR_BGR2GRAY);
    else
      clean = roi(b).clone();
    cv::equalizeHist(clean, clean);
    cv::threshold(clean, clean, 170, 255, cv::THRESH_TOZERO);
    rows.push_back({b, conf[i], DHash(clean), false});
  }
  std::sort(rows.begin(), rows.end(),
            [](auto &a, auto &b) { return a.bounds.y < b.bounds.y; });
  return rows;
}
std::vector<DetectedRow> KillDetector::Inspect(const cv::Mat &r) const {
  return Detect(r);
}
std::vector<DetectedRow> KillDetector::Process(const cv::Mat &roi) {
  auto cur = Detect(roi);
  auto now = std::chrono::steady_clock::now();
  const auto memory = std::chrono::seconds(10);
  std::erase_if(recentlySeen_, [&](const SeenRow &row) {
    return now - row.lastSeen > memory;
  });
  if (!initialized_) {
    previous_ = cur;
    for (const auto &row : cur)
      recentlySeen_.push_back({row.hash, now});
    initialized_ = true;
    return {};
  }
  // A row can disappear for a few frames while the kill feed slides. Keep the
  // last stable state so its reappearance is not reported as another kill.
  if (cur.empty()) {
    if (++missingFrames_ > 10) previous_.clear();
    return {};
  }
  missingFrames_ = 0;
  std::vector<DetectedRow> fresh;
  std::vector<bool> used(previous_.size(), false);
  std::vector<bool> oldRows(cur.size(), false);
  int strongMatches = 0;
  for (size_t rowIndex = 0; rowIndex < cur.size(); ++rowIndex) {
    auto &r = cur[rowIndex];
    for (size_t i = 0; i < previous_.size(); ++i) {
      if (used[i]) continue;
      auto &p = previous_[i];
      int dy = std::abs(r.bounds.y - p.bounds.y);
      if (HammingDistance(r.hash, p.hash) <= std::max(config_.hashDistance, 20) &&
          (dy <= templ_.rows * 4 || r.bounds.y <= p.bounds.y)) {
        oldRows[rowIndex] = true;
        used[i] = true;
        ++strongMatches;
        break;
      }
    }
  }
  // During a coherent vertical slide the brightness animation can change the
  // hash substantially. If the number of rows did not grow, pair remaining
  // rows by their vertical order instead of treating every shifted row as new.
  if (cur.size() <= previous_.size() &&
      (strongMatches > 0 || cur.size() == previous_.size())) {
    for (size_t rowIndex = 0; rowIndex < cur.size(); ++rowIndex) {
      if (oldRows[rowIndex]) continue;
      size_t best = previous_.size();
      int bestDistance = templ_.rows * 4 + 1;
      for (size_t i = 0; i < previous_.size(); ++i) {
        if (used[i]) continue;
        int distance = std::abs(cur[rowIndex].bounds.y - previous_[i].bounds.y);
        if (distance < bestDistance) { best = i; bestDistance = distance; }
      }
      if (best != previous_.size()) {
        used[best] = true;
        oldRows[rowIndex] = true;
      }
    }
  }
  for (size_t rowIndex = 0; rowIndex < cur.size(); ++rowIndex) {
    auto &r = cur[rowIndex];
    if (!oldRows[rowIndex]) {
      auto seen = std::find_if(recentlySeen_.begin(), recentlySeen_.end(),
                               [&](const SeenRow &old) {
        return HammingDistance(r.hash, old.hash) <= config_.hashDistance;
      });
      if (seen != recentlySeen_.end()) {
        oldRows[rowIndex] = true;
        seen->lastSeen = now;
        seen->hash = r.hash;
      }
    }
    if (!oldRows[rowIndex]) {
      r.isNew = true;
      fresh.push_back(r);
    }
  }
  for (const auto &row : fresh)
    recentlySeen_.push_back({row.hash, now});
  previous_ = cur;
  if (!fresh.empty())
    lastKill_ = now;
  return fresh;
}
void KillDetector::Reset() {
  previous_.clear();
  recentlySeen_.clear();
  lastKill_ = {};
  missingFrames_ = 0;
  initialized_ = false;
}
