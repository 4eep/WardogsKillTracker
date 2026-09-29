#pragma once
#include "CenterKillDetector.h"
#include "Config.h"
#include "KillDetector.h"
#include "RecordingMonitor.h"
#include "ScreenCapture.h"
#include "Session.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <set>
#include <deque>
class Application {
public:
  explicit Application(Config);
  int Run();
  void RequestStop();
  int Calibrate(bool createTemplate);
  int TestImage(const std::filesystem::path &);
  int TestFolder(const std::filesystem::path &);
  static void ListMonitors();

private:
  void OnHotkey();
  void OnVideo(const std::filesystem::path &);
  void StartSession(std::chrono::steady_clock::time_point);
  void StopSession();
  void ProcessArchives();
  void CaptureLoop(std::stop_token);
  void DrawDebug(cv::Mat &, const std::vector<DetectedRow> &) const;
  struct PendingKillImage {
    cv::Mat image;
    uint64_t hash{};
    float confidence{};
    std::chrono::steady_clock::time_point detectedAt{};
  };
  struct PendingConfirmation {
    float confidence{};
    std::chrono::steady_clock::time_point detectedAt{};
  };
  Config config_;
  std::unique_ptr<DxgiScreenCapture> capture_;
  std::unique_ptr<KillDetector> detector_;
  std::unique_ptr<CenterKillDetector> centerDetector_;
  std::shared_ptr<RecordingSession> session_;
  struct PendingArchive {
    std::shared_ptr<RecordingSession> session;
    std::chrono::steady_clock::time_point deadline;
  };
  std::vector<PendingArchive> pendingArchives_;
  std::set<std::filesystem::path> archivedVideos_;
  std::chrono::system_clock::time_point sessionWallStart_{};
  std::unique_ptr<RecordingMonitor> monitor_;
  std::jthread captureThread_;
  std::atomic<RecordingState> state_ = RecordingState::Idle;
  std::atomic_bool stopping_ = false;
  std::recursive_mutex sessionMutex_;
  std::chrono::steady_clock::time_point ignoreVideoEventsUntil_{};
  std::deque<PendingKillImage> pendingKillImages_;
  std::deque<PendingConfirmation> pendingConfirmations_;
};
