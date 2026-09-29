#pragma once
#include "Config.h"
#include <filesystem>
#include <opencv2/core.hpp>
#include <vector>

struct SkullDetectionResult {
  int visible{};
  int newKills{};
  float confidence{};
  std::vector<cv::Rect> matches;
};

class CenterKillDetector {
public:
  CenterKillDetector(const std::filesystem::path &, double);
  explicit CenterKillDetector(const Config &);
  SkullDetectionResult Process(const cv::Mat &);
  SkullDetectionResult Inspect(const cv::Mat &) const;
  void Reset();

private:
  SkullDetectionResult Detect(const cv::Mat &) const;
  cv::Mat template_;
  double threshold_ = .70;
  double regionHeight_ = .05;
  int previousCount_ = 0;
  int candidateCount_ = 0;
  int candidateFrames_ = 0;
  bool initialized_ = false;
};
