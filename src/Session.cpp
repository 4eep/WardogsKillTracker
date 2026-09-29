#include "Session.h"
#include "Utils.h"
#include <fstream>
#include <algorithm>
#include <Windows.h>
#include <stdexcept>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
RecordingSession::RecordingSession(std::filesystem::path r, int o)
    : root_(std::move(r)), offset_(o) {}
void RecordingSession::Start(std::chrono::steady_clock::time_point t) {
  std::scoped_lock l(mutex_);
  start_ = t.time_since_epoch().count() ? t : std::chrono::steady_clock::now();
  stop_ = start_;
  wallStart_ = LocalTimestamp("%Y-%m-%d %H:%M:%S");
  directory_ = root_ / LocalTimestamp("%Y-%m-%d_%H-%M-%S");
  for (int i = 1; exists(directory_); ++i)
    directory_ =
        root_ / (LocalTimestamp("%Y-%m-%d_%H-%M-%S") + "_" + std::to_string(i));
  std::filesystem::create_directories(directory_);
  kills_.clear();
  rowImages_.clear();
  copiedImages_ = 0;
  recordingFile_.clear();
  archiveDirectory_.clear();
  archived_ = archiveTextCopied_ = false;
  stableSince_ = {};
  recording_ = true;
  FlushKills();
}
void RecordingSession::Stop() {
  std::scoped_lock l(mutex_);
  if (!recording_)
    return;
  stop_ = std::chrono::steady_clock::now();
  recording_ = false;
  FlushKills();
}
KillEvent RecordingSession::RegisterKill(float c, uint64_t h) {
  std::scoped_lock l(mutex_);
  auto raw = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - start_) +
             std::chrono::milliseconds(offset_);
  KillEvent e{(uint32_t)kills_.size() + 1,
              std::max(raw, std::chrono::milliseconds(0)), c, h};
  kills_.push_back(e);
  FlushKills();
  return e;
}
std::chrono::milliseconds RecordingSession::Elapsed() const {
  std::scoped_lock l(mutex_);
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      (recording_ ? std::chrono::steady_clock::now() : stop_) - start_);
}
bool RecordingSession::IsRecording() const {
  std::scoped_lock l(mutex_);
  return recording_;
}
size_t RecordingSession::KillCount() const {
  std::scoped_lock l(mutex_);
  return kills_.size();
}
void RecordingSession::SetRecordingFile(std::filesystem::path p) {
  std::scoped_lock l(mutex_);
  if (archived_ || p == recordingFile_) return;
  const auto oldText = directory_ / (OutputStem() + ".txt");
  recordingFile_ = std::move(p);
  stableSince_ = {};
  FlushKills();
  std::error_code ec;
  if (oldText != directory_ / (OutputStem() + ".txt"))
    std::filesystem::remove(oldText, ec);

}
std::string RecordingSession::OutputStem() const {
  if (!recordingFile_.empty() && !recordingFile_.stem().empty())
    return recordingFile_.stem().string();
  return "kills";
}
std::filesystem::path RecordingSession::TextPath() const {
  std::scoped_lock l(mutex_);
  return directory_ / (OutputStem() + ".txt");
}
void RecordingSession::FlushKills() const {
  std::ofstream t(directory_ / (OutputStem() + ".txt"), std::ios::trunc);
  t.exceptions(std::ios::failbit | std::ios::badbit);
  for (const auto &e : kills_)
    t << "Kill " << FormatDuration(e.timestamp) << '\n';
  t.flush();
}
std::filesystem::path RecordingSession::RecordingFile() const {
  std::scoped_lock l(mutex_);
  return recordingFile_;
}
std::chrono::milliseconds RecordingSession::EventTimestamp() const {
  std::scoped_lock l(mutex_);
  return std::max(std::chrono::milliseconds(0),
      std::chrono::duration_cast<std::chrono::milliseconds>(
          (recording_ ? std::chrono::steady_clock::now() : stop_) - start_) +
          std::chrono::milliseconds(offset_));
}
std::filesystem::path RecordingSession::SaveKillRow(
    const cv::Mat &row, std::chrono::milliseconds timestamp) {
  std::scoped_lock l(mutex_);
  if (!recording_ || row.empty()) return {};
  cv::Mat visible;
  if (row.channels() == 4)
    cv::cvtColor(row, visible, cv::COLOR_BGRA2BGR);
  else
    visible = row;
  auto stem = FormatDuration(timestamp);
  std::replace(stem.begin(), stem.end(), ':', '-');
  std::filesystem::path path;
  for (int suffix = 1;; ++suffix) {
    path = directory_ /
        (stem + (suffix == 1 ? "" : "_" + std::to_string(suffix)) + ".png");
    if (!std::filesystem::exists(path)) break;
  }
  if (!cv::imwrite(path.string(), visible,
                   {cv::IMWRITE_PNG_COMPRESSION, 1}))
    throw std::runtime_error("Could not save kill-feed image: " + path.string());
  rowImages_.push_back(path.filename());
  return path;
}
bool RecordingSession::TryArchive() {
  std::scoped_lock l(mutex_);
  if (archived_) return true;
  if (recording_ || recordingFile_.empty()) return false;
  std::error_code ec;
  auto size = std::filesystem::file_size(recordingFile_, ec);
  if (ec || size == 0) return false;
  auto write = std::filesystem::last_write_time(recordingFile_, ec);
  if (ec) return false;
  auto now = std::chrono::steady_clock::now();
  if (stableSince_ == std::chrono::steady_clock::time_point{} ||
      size != observedSize_ || write != observedWrite_) {
    observedSize_ = size; observedWrite_ = write; stableSince_ = now;
    return false;
  }
  if (now - stableSince_ < std::chrono::seconds(2)) return false;
  // A recorder retaining write access prevents this exclusive open.
  HANDLE video = CreateFileW(recordingFile_.c_str(), GENERIC_READ, 0, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (video == INVALID_HANDLE_VALUE) return false;
  CloseHandle(video);
  if (archiveDirectory_.empty()) {
    auto parent = recordingFile_.parent_path();
    auto stem = recordingFile_.stem().wstring();
    for (int suffix = 0;; ++suffix) {
      auto candidate = parent / (stem + (suffix ? L" (" + std::to_wstring(suffix) + L")" : L""));
      if (std::filesystem::create_directory(candidate)) { archiveDirectory_ = candidate; break; }
    }
  }
  auto oldText = directory_ / (OutputStem() + ".txt");
  auto newText = archiveDirectory_ / (OutputStem() + ".txt");
  if (!archiveTextCopied_) {
    std::filesystem::copy_file(oldText, newText); // Fail instead of overwriting another file.
    archiveTextCopied_ = true;
  }
  while (copiedImages_ < rowImages_.size()) {
    const auto &name = rowImages_[copiedImages_];
    auto destinationImage = archiveDirectory_ / name;
    ec.clear();
    std::filesystem::copy_file(directory_ / name, destinationImage,
                               std::filesystem::copy_options::none, ec);
    if (ec) {
      std::error_code cleanup;
      std::filesystem::remove(destinationImage, cleanup);
      return false;
    }
    ++copiedImages_;
  }
  auto destination = archiveDirectory_ / recordingFile_.filename();
  ec.clear();
  std::filesystem::copy_file(recordingFile_, destination,
                             std::filesystem::copy_options::none, ec);
  if (ec || !std::filesystem::is_regular_file(destination)) {
    std::error_code cleanup;
    std::filesystem::remove(destination, cleanup);
    return false;
  }
  ec.clear();
  if (!std::filesystem::remove(recordingFile_, ec) || ec) {
    std::error_code cleanup;
    std::filesystem::remove(destination, cleanup);
    return false;
  }
  auto oldDirectory = directory_;
  recordingFile_ = destination;
  directory_ = archiveDirectory_;
  archived_ = true;
  std::filesystem::remove(oldText, ec);
  for (const auto &name : rowImages_) {
    ec.clear();
    std::filesystem::remove(oldDirectory / name, ec);
  }
  // Remove only the now-empty staging directory, never recursively.
  ec.clear(); std::filesystem::remove(oldDirectory, ec);
  return true;
}
