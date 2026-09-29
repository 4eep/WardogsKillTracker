#pragma once
#include <filesystem>
#include <string>
struct Region {
  double x = 0, y = .42, width = .30, height = .14;
};
struct Config {
  int monitorIndex = 0, fps = 30;
  Region region;
  Region centerRegion{.41, .64, .18, .05};
  std::filesystem::path templatePath = L"assets/player_template.png",
                        skullTemplatePath = L"assets/Killfeed_KillConfirmed.png";
  double threshold = .85, skullThreshold = .70;
  int hashDistance = 10, fusionWindowMs = 1200;
  bool debugPreview = false, centerDetection = true, hotkey = true,
       watchDirectory = true;
  std::filesystem::path recordingDirectory = L"D:\\Videos\\NVIDIA",
                        outputDirectory = L"output";
  int confirmationTimeoutMs = 5000, timestampOffsetMs = 0;
  std::filesystem::path baseDirectory;
  static Config Load(const std::filesystem::path &);
  std::filesystem::path Resolve(const std::filesystem::path &) const;
};
