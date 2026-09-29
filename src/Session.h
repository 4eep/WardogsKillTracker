#pragma once
#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
struct KillEvent {
  uint32_t index{};
  std::chrono::milliseconds timestamp{};
  float confidence{};
  uint64_t rowHash{};
};
class RecordingSession {
public:
  RecordingSession(std::filesystem::path root, int offsetMs);
  void Start(std::chrono::steady_clock::time_point = {});
  void Stop();
  KillEvent RegisterKill(float, uint64_t);
  std::chrono::milliseconds Elapsed() const;
  bool IsRecording() const;
  size_t KillCount() const;
  void SetRecordingFile(std::filesystem::path);
  std::filesystem::path Directory() const { return directory_; }
  std::filesystem::path TextPath() const;
  std::filesystem::path RecordingFile() const;
  std::chrono::milliseconds EventTimestamp() const;
  std::filesystem::path SaveKillRow(const cv::Mat &,
                                    std::chrono::milliseconds);
  // Non-blocking: wait for a stable, closed video, then move the recording pair.
  bool TryArchive();

private:
  void FlushKills() const;
  std::string OutputStem() const;
  mutable std::mutex mutex_;
  std::filesystem::path root_, directory_, recordingFile_;
  std::filesystem::path archiveDirectory_;
  uintmax_t observedSize_ = 0;
  std::filesystem::file_time_type observedWrite_{};
  std::chrono::steady_clock::time_point stableSince_{};
  bool archived_ = false, archiveTextCopied_ = false;
  int offset_;
  bool recording_ = false;
  std::chrono::steady_clock::time_point start_, stop_;
  std::string wallStart_;
  std::vector<KillEvent> kills_;
  std::vector<std::filesystem::path> rowImages_;
  size_t copiedImages_ = 0;
};
