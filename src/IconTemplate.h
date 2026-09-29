#pragma once
#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
inline cv::Mat LoadIconTemplate(const std::filesystem::path &path) {
  auto image = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
  if (image.empty()) return {};
  cv::Mat gray;
  if (image.channels() == 4) {
    cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    cv::Mat alpha;
    cv::extractChannel(image, alpha, 3);
    cv::multiply(gray, alpha, gray, 1.0 / 255.0);
  } else if (image.channels() == 3) cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
  else gray = image;
  return gray;
}
