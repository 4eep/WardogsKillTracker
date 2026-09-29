#include "Application.h"
#include "Logger.h"
#include "Utils.h"
#include <algorithm>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
namespace {
cv::Rect ToRect(const Region &r, const cv::Mat &f) {
  cv::Rect q{(int)(f.cols * r.x), (int)(f.rows * r.y), (int)(f.cols * r.width),
             (int)(f.rows * r.height)};
  return q & cv::Rect(0, 0, f.cols, f.rows);
}
} // namespace
Application::Application(Config c) : config_(std::move(c)) {}
void Application::ListMonitors() {
  auto v = DxgiScreenCapture::ListMonitors();
  for (size_t i = 0; i < v.size(); ++i)
    std::cout << '[' << i << "] " << v[i].rect.right - v[i].rect.left << 'x'
              << v[i].rect.bottom - v[i].rect.top << " @ (" << v[i].rect.left
              << ',' << v[i].rect.top << ')' << (v[i].primary ? " PRIMARY" : "")
              << "\n";
}
int Application::Run() {
  std::cout << "========================================\n       WARDOGS KILL "
               "TRACKER\n========================================\n\nChecking "
               "configuration...\n[OK] Config\n";
  try {
    capture_ = std::make_unique<DxgiScreenCapture>(config_.monitorIndex);
    auto s = capture_->Size();
    std::cout << "[OK] Monitor " << config_.monitorIndex << " - " << s.cx << 'x'
              << s.cy << "\n";
  } catch (const std::exception &e) {
    Logger::Get().Error(e.what());
    return 2;
  }
  try {
    detector_ = std::make_unique<KillDetector>(config_);
    std::cout << "[OK] Player template\n";
  } catch (const std::exception &e) {
    Logger::Get().Error(std::string(e.what()) + ". Check assets and template paths.");
    return 3;
  }
  if (config_.centerDetection) {
    try {
      centerDetector_ = std::make_unique<CenterKillDetector>(
          config_);
      std::cout << "[OK] Center kill confirmation template\n";
    } catch (const std::exception &e) {
      Logger::Get().Warn(
          std::string(e.what()) +
          "; center confirmation disabled. Check HUD template paths.");
    }
  }
  session_ = std::make_shared<RecordingSession>(
      config_.Resolve(config_.outputDirectory), config_.timestampOffsetMs);
  monitor_ = std::make_unique<RecordingMonitor>(
      config_.Resolve(config_.recordingDirectory), config_.watchDirectory,
      [this] { OnHotkey(); }, [this](auto &p) { OnVideo(p); });
  if (config_.hotkey && !monitor_->InstallHook())
    Logger::Get().Warn("Alt+F9 keyboard hook could not be installed");
  else
    std::cout << "[OK] Alt+F9 monitor\nReady. WAITING FOR NVIDIA RECORDING\n";
  captureThread_ = std::jthread([this](std::stop_token s) { CaptureLoop(s); });
  monitor_->RunMessageLoop(captureThread_.get_stop_token());
  if (captureThread_.joinable()) captureThread_.join();
  StopSession();
  // Allow the final NVIDIA close/rename notification to arrive during shutdown.
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    ProcessArchives();
    { std::scoped_lock l(sessionMutex_); if (pendingArchives_.empty()) break; }
    Sleep(100);
  }
  monitor_->Stop();
  for (const auto &pending : pendingArchives_)
    Logger::Get().Warn("Video not ready for moving; event TXT kept at " + pending.session->TextPath().string());
  return 0;
}
void Application::RequestStop() {
  if (stopping_.exchange(true))
    return;
  if (captureThread_.joinable())
    captureThread_.request_stop();
}
void Application::OnHotkey() {
  Logger::Get().Info("NVIDIA Alt+F9 detected");
  auto expected = RecordingState::Idle;
  if (state_.compare_exchange_strong(expected, RecordingState::Starting))
    StartSession(std::chrono::steady_clock::now());
  else if (expected == RecordingState::Recording) {
    state_ = RecordingState::Stopping;
    StopSession();
  }
}
void Application::StartSession(std::chrono::steady_clock::time_point t) {
  std::scoped_lock l(sessionMutex_);
  if (session_->IsRecording())
    return;
  session_ = std::make_shared<RecordingSession>(
      config_.Resolve(config_.outputDirectory), config_.timestampOffsetMs);
  sessionWallStart_ = std::chrono::system_clock::now();
  detector_->Reset();
  if (centerDetector_)
    centerDetector_->Reset();
  pendingKillImages_.clear();
  pendingConfirmations_.clear();
  session_->Start(t);
  state_ = RecordingState::Recording;
  Logger::Get().Info("Recording started; session=" +
                     session_->Directory().string());
}
void Application::StopSession() {
  std::scoped_lock l(sessionMutex_);
  if (!session_ || !session_->IsRecording()) {
    state_ = RecordingState::Idle;
    return;
  }
  auto d = session_->Elapsed();
  auto k = session_->KillCount();
  auto dir = session_->Directory();
  session_->Stop();
  pendingArchives_.push_back({session_, std::chrono::steady_clock::now() + std::chrono::minutes(2)});
  ignoreVideoEventsUntil_ =
      std::chrono::steady_clock::now() +
      std::chrono::milliseconds(config_.confirmationTimeoutMs);
  state_ = RecordingState::Idle;
  Logger::Get().Info("Recording stopped. Duration: " + FormatDuration(d) +
                     " | Kills: " + std::to_string(k));
  std::cout << "Event TXT saved: " << session_->TextPath().string()
            << "\nWaiting for video to close before organizing files."
            << "\nWAITING FOR NVIDIA RECORDING\n";
}
void Application::OnVideo(const std::filesystem::path &p) {
  std::scoped_lock l(sessionMutex_);
  if (archivedVideos_.contains(p.lexically_normal())) return;
  std::error_code fileError;
  if (!std::filesystem::is_regular_file(p, fileError)) return;
  for (const auto &pending : pendingArchives_)
    if (pending.session->RecordingFile() == p) return;
  bool predatesCurrent = false;
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &data)) {
    ULARGE_INTEGER ticks{};
    ticks.LowPart = data.ftCreationTime.dwLowDateTime;
    ticks.HighPart = data.ftCreationTime.dwHighDateTime;
    auto created = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::duration<int64_t, std::ratio<1, 10000000>>(
                (int64_t)ticks.QuadPart - 116444736000000000LL)));
    predatesCurrent = created < sessionWallStart_;
  }
  // A video may become visible only after Stop, even if the next session has begun.
  for (auto it = pendingArchives_.rbegin(); it != pendingArchives_.rend(); ++it)
    if (it->session->RecordingFile().empty() && (!session_->IsRecording() || predatesCurrent)) {
      it->session->SetRecordingFile(p);
      Logger::Get().Info("Stopped recording video found; queued for organizing: " + p.string());
      return;
    }
  Logger::Get().Info("NVIDIA file detected: " + p.string());
  if (!session_->IsRecording()) {
    detector_->Reset();
    if (centerDetector_)
      centerDetector_->Reset();
    pendingKillImages_.clear();
    pendingConfirmations_.clear();
    session_ = std::make_shared<RecordingSession>(
        config_.Resolve(config_.outputDirectory), config_.timestampOffsetMs);
    sessionWallStart_ = std::chrono::system_clock::now();
    session_->Start(std::chrono::steady_clock::now());
    state_ = RecordingState::Recording;
    Logger::Get().Info("Recording started from directory watcher");
  }
  session_->SetRecordingFile(p);
}
void Application::ProcessArchives() {
  std::scoped_lock l(sessionMutex_);
  for (auto it = pendingArchives_.begin(); it != pendingArchives_.end();) {
    try {
      if (it->session->TryArchive()) {
        archivedVideos_.insert(it->session->RecordingFile().lexically_normal());
        Logger::Get().Info("Recording organized: " + it->session->Directory().string());
        it = pendingArchives_.erase(it);
        continue;
      }
    } catch (const std::exception &e) {
      Logger::Get().Warn(std::string("Could not organize recording; original files retained: ") + e.what());
      it = pendingArchives_.erase(it);
      continue;
    }
    if (std::chrono::steady_clock::now() >= it->deadline) {
      Logger::Get().Warn("Timed out waiting for video; TXT retained: " + it->session->TextPath().string());
      it = pendingArchives_.erase(it);
    } else ++it;
  }
}
void Application::DrawDebug(cv::Mat &m,
                            const std::vector<DetectedRow> &r) const {
  for (auto &x : r) {
    cv::rectangle(m, x.bounds,
                  x.isNew ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 180, 255), 2);
    cv::putText(m, cv::format("%.3f", x.confidence),
                x.bounds.tl() + cv::Point(0, -3), cv::FONT_HERSHEY_SIMPLEX, .45,
                {0, 255, 0}, 1);
  }
}
void Application::CaptureLoop(std::stop_token stop) {
  auto period = std::chrono::microseconds(1000000 / config_.fps);
  auto next = std::chrono::steady_clock::now();
  auto lastDisplay = next;
  auto lastArchiveCheck = next;
  while (!stop.stop_requested()) {
    if (std::chrono::steady_clock::now() - lastArchiveCheck >= std::chrono::milliseconds(250)) {
      ProcessArchives();
      lastArchiveCheck = std::chrono::steady_clock::now();
    }
    if (state_ != RecordingState::Recording) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      next = std::chrono::steady_clock::now();
      continue;
    }
    cv::Mat frame;
    if (capture_->Capture(frame) && !frame.empty()) {
      // Keep detector state and registration in the same recording session.
      std::scoped_lock frameLock(sessionMutex_);
      if (!session_->IsRecording()) continue;
      auto feedR = ToRect(config_.region, frame);
      auto feedRoi = frame(feedR);
      auto fresh = detector_->Process(feedRoi);
      float confidence = fresh.empty() ? 0.f : fresh.front().confidence;
      SkullDetectionResult skull;
      if (centerDetector_) {
        auto centerR = ToRect(config_.centerRegion, frame);
        auto centerRoi = frame(centerR);
        skull = centerDetector_->Process(centerRoi);
        confidence = std::max(confidence, skull.confidence);
        if (config_.debugPreview) {
          auto v = centerRoi.clone();
          for (auto &r : skull.matches)
            cv::rectangle(v, r, {255, 255, 0}, 2);
          cv::imshow("WARDOGS Center Kill Debug", v);
        }
      }
      auto now = std::chrono::steady_clock::now();
      auto fusionWindow = std::chrono::milliseconds(config_.fusionWindowMs);
      while (!pendingKillImages_.empty() &&
             now - pendingKillImages_.front().detectedAt > fusionWindow)
        pendingKillImages_.pop_front();
      while (!pendingConfirmations_.empty() &&
             now - pendingConfirmations_.front().detectedAt > fusionWindow)
        pendingConfirmations_.pop_front();
      for (const auto &row : fresh) {
        auto bounds = cv::Rect(0, row.bounds.y, feedRoi.cols,
                               row.bounds.height) &
                      cv::Rect(0, 0, feedRoi.cols, feedRoi.rows);
        if (!bounds.empty())
          pendingKillImages_.push_back(
              {feedRoi(bounds).clone(), row.hash, row.confidence, now});
      }
      for (int i = 0; i < skull.newKills; ++i)
        pendingConfirmations_.push_back({skull.confidence, now});

      while (!pendingKillImages_.empty() &&
             (!centerDetector_ || !pendingConfirmations_.empty())) {
        auto row = std::move(pendingKillImages_.front());
        pendingKillImages_.pop_front();
        float confirmationConfidence = 0.f;
        if (centerDetector_) {
          confirmationConfidence = pendingConfirmations_.front().confidence;
          pendingConfirmations_.pop_front();
        }
        auto event = session_->RegisterKill(
            std::max(row.confidence, confirmationConfidence), row.hash);
        Logger::Get().Kill("#" + std::to_string(event.index) + " - " +
                           FormatDuration(event.timestamp) +
                           " confidence=" + cv::format("%.3f", event.confidence));
        if (!row.image.empty()) session_->SaveKillRow(row.image, event.timestamp);
      }
      if (config_.debugPreview) {
        auto v = feedRoi.clone();
        DrawDebug(v, detector_->Inspect(feedRoi));
        cv::imshow("WARDOGS Kill Detector Debug", v);
        cv::waitKey(1);
      }
      if (std::chrono::steady_clock::now() - lastDisplay >=
          std::chrono::milliseconds(250)) {
        std::scoped_lock l(sessionMutex_);
        if (session_->IsRecording())
          std::cout << "\rRecording: " << FormatDuration(session_->Elapsed())
                    << " | Kills: " << session_->KillCount() << "          "
                    << std::flush;
        lastDisplay = std::chrono::steady_clock::now();
      }
    }
    next += period;
    std::this_thread::sleep_until(next);
    if (next < std::chrono::steady_clock::now() - period)
      next = std::chrono::steady_clock::now();
  }
  cv::destroyAllWindows();
}
int Application::Calibrate(bool create) {
  capture_ = std::make_unique<DxgiScreenCapture>(config_.monitorIndex);
  cv::Mat f;
  for (int i = 0; i < 20 && !capture_->Capture(f); ++i)
    Sleep(50);
  if (f.empty())
    return 2;
  auto dbg = config_.baseDirectory / "debug";
  std::filesystem::create_directories(dbg);
  auto feed = f(ToRect(config_.region, f)),
       center = f(ToRect(config_.centerRegion, f));
  cv::imwrite((dbg / "fullscreen.png").string(), f);
  cv::imwrite(
      (dbg / (create ? "template_source.png" : "killfeed_roi.png")).string(),
      feed);
  cv::imwrite((dbg / (create ? "skull_template_source.png" : "center_roi.png"))
                  .string(),
              center);
  if (create) {
    std::cout << "Saved template_source.png and skull_template_source.png. "
                 "Crop your player name to assets/player_template.png and one skull to "
                 "assets/Killfeed_KillConfirmed.png\n";
    return 0;
  }
  detector_ = std::make_unique<KillDetector>(config_);
  auto rows = detector_->Inspect(feed);
  auto out = feed.clone();
  DrawDebug(out, rows);
  cv::imwrite((dbg / "detection_result.png").string(), out);
  if (std::filesystem::exists(config_.Resolve(config_.skullTemplatePath))) {
    centerDetector_ = std::make_unique<CenterKillDetector>(
        config_);
    auto s = centerDetector_->Inspect(center);
    auto co = center.clone();
    for (auto &r : s.matches)
      cv::rectangle(co, r, {0, 255, 0}, 2);
    cv::imwrite((dbg / "center_detection_result.png").string(), co);
    std::cout << "Skulls: " << s.visible << " confidence=" << s.confidence
              << "\n";
  }
  for (auto &x : rows)
    std::cout << "Player match: X=" << x.bounds.x << " Y=" << x.bounds.y
              << " confidence=" << x.confidence << "\n";
  return 0;
}
int Application::TestImage(const std::filesystem::path &p) {
  auto f = cv::imread(p.string(), cv::IMREAD_UNCHANGED);
  if (f.empty())
    return 2;
  detector_ = std::make_unique<KillDetector>(config_);
  auto roi = f(ToRect(config_.region, f));
  auto rows = detector_->Inspect(roi);
  auto out = roi.clone();
  DrawDebug(out, rows);
  std::filesystem::create_directories(config_.baseDirectory / "debug");
  cv::imwrite((config_.baseDirectory / "debug/test_image_result.png").string(),
              out);
  auto center = f(ToRect(config_.centerRegion, f));
  cv::imwrite((config_.baseDirectory / "debug/test_center_roi.png").string(),
              center);
  std::cout << "Center HUD saved to debug/test_center_roi.png\n";
  if (std::filesystem::exists(config_.Resolve(config_.skullTemplatePath))) {
    centerDetector_ = std::make_unique<CenterKillDetector>(
        config_);
    auto skulls = centerDetector_->Inspect(center);
    auto centerOut = center.clone();
    for (auto &match : skulls.matches)
      cv::rectangle(centerOut, match, {0, 255, 0}, 2);
    cv::imwrite(
        (config_.baseDirectory / "debug/test_center_result.png").string(),
        centerOut);
    std::cout << "Skulls=" << skulls.visible
              << " confidence=" << skulls.confidence << '\n';
  }
  for (auto &x : rows)
    std::cout << x.bounds.x << ',' << x.bounds.y
              << " confidence=" << x.confidence << " hash=" << x.hash << '\n';
  return 0;
}
int Application::TestFolder(const std::filesystem::path &p) {
  detector_ = std::make_unique<KillDetector>(config_);
  std::vector<std::filesystem::path> v;
  for (auto &e : std::filesystem::directory_iterator(p))
    if (e.is_regular_file())
      v.push_back(e.path());
  std::sort(v.begin(), v.end());
  CenterKillDetector center(config_);
  int total = 0;
  for (auto &x : v) {
    auto f = cv::imread(x.string(), cv::IMREAD_UNCHANGED);
    if (f.empty()) continue;
    auto fresh = detector_->Process(f(ToRect(config_.region, f)));
    auto hud = center.Process(f(ToRect(config_.centerRegion, f)));
    total += std::min(static_cast<int>(fresh.size()), hud.newKills);
    std::cout << x.filename().string() << ": feed=" << fresh.size()
              << " center=" << hud.newKills << '\n';
  }
  std::cout << "Kill total=" << total << '\n';
  return 0;
}
