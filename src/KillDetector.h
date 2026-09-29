#pragma once
#include "Config.h"
#include <chrono>
#include <cstdint>
#include <opencv2/core.hpp>
#include <vector>
struct DetectedRow {
  cv::Rect bounds;
  float confidence{};
  uint64_t hash{};
  bool isNew{};
};
class KillDetector {
public:
  explicit KillDetector(const Config &);
  std::vector<DetectedRow> Process(const cv::Mat &roi);
  std::vector<DetectedRow> Inspect(const cv::Mat &roi) const;
  void Reset();
  static uint64_t DHash(const cv::Mat &);

private:
  struct SeenRow {
    uint64_t hash{};
    std::chrono::steady_clock::time_point lastSeen;
  };
  std::vector<DetectedRow> Detect(const cv::Mat &) const;
  Config config_;
  cv::Mat templ_;
  std::vector<DetectedRow> previous_;
  std::vector<SeenRow> recentlySeen_;
  std::chrono::steady_clock::time_point lastKill_{};
  int missingFrames_ = 0;
  bool initialized_ = false;
};
