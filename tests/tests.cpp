#include "CenterKillDetector.h"
#include "Config.h"
#include "IconTemplate.h"
#include "KillDetector.h"
#include "Session.h"
#include "Utils.h"
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <fstream>
#include <opencv2/imgproc.hpp>
#include <thread>

#ifndef TEST_ASSET_DIR
#define TEST_ASSET_DIR "assets"
#endif

int main() {
  using namespace std::chrono_literals;
  assert(FormatDuration(804287ms) == "00:13:24:28");

  Config config;
  config.baseDirectory = std::filesystem::path(TEST_ASSET_DIR).parent_path();
  CenterKillDetector detector(config);
  cv::Mat blank(54, 346, CV_8UC1, cv::Scalar(30));
  auto withIcons = [&](int count) {
    auto frame = blank.clone();
    auto icon = LoadIconTemplate(std::filesystem::path(TEST_ASSET_DIR) /
                                 "Killfeed_KillConfirmed.png");
    const int height = 18;
    const int width = height * icon.cols / icon.rows;
    cv::resize(icon, icon, {width, height}, 0, 0, cv::INTER_AREA);
    for (int i = 0; i < count; ++i)
      icon.copyTo(frame(cv::Rect(40 + i * (width + 15), 17, width, height)));
    return frame;
  };
  detector.Process(blank);
  assert(detector.Process(withIcons(4)).newKills == 0);
  assert(detector.Process(withIcons(4)).newKills == 4);
  assert(detector.Process(blank).newKills == 0);
  assert(detector.Process(withIcons(4)).newKills == 0);
  assert(detector.Process(blank).newKills == 0);
  assert(detector.Process(blank).newKills == 0);
  assert(detector.Process(withIcons(4)).newKills == 0);
  assert(detector.Process(withIcons(4)).newKills == 4);

  KillDetector feed(config);
  auto player = cv::imread((std::filesystem::path(TEST_ASSET_DIR) /
                            "player_template.png").string(),
                           cv::IMREAD_GRAYSCALE);
  auto feedFrame = [&](int shift) {
    cv::Mat frame(151, 576, CV_8UC1, cv::Scalar(20));
    for (int i = 0; i < 2; ++i) {
      int y = 55 + i * 40 + shift;
      player.copyTo(frame(cv::Rect(20, y, player.cols, player.rows)));
      cv::putText(frame, i ? "Victim B" : "Victim A", {170, y + 18},
                  cv::FONT_HERSHEY_SIMPLEX, .5, cv::Scalar(230), 1);
    }
    return frame;
  };
  feed.Process(cv::Mat(151, 576, CV_8UC1, cv::Scalar(20)));
  assert(feed.Process(feedFrame(0)).size() == 2);
  assert(feed.Process(feedFrame(-12)).empty());
  assert(feed.Process(cv::Mat(151, 576, CV_8UC1, cv::Scalar(20))).empty());
  assert(feed.Process(feedFrame(-12)).empty());
  // A feed animation may hide the row for considerably more than ten frames.
  // Reappearing at another vertical position must not emit the same kill again.
  for (int i = 0; i < 30; ++i)
    assert(feed.Process(cv::Mat(151, 576, CV_8UC1, cv::Scalar(20))).empty());
  assert(feed.Process(feedFrame(-24)).empty());

  auto root = std::filesystem::temp_directory_path() /
      ("wardogs-test-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  RecordingSession session(root / "staging", 0);
  session.Start();
  session.RegisterKill(1, 0);
  session.RegisterKill(1, 0);
  cv::Mat killImage(24, 576, CV_8UC4, cv::Scalar(20, 40, 60, 0));
  auto image = session.SaveKillRow(killImage, 804287ms);
  assert(std::filesystem::exists(image));
  session.Stop();

  auto videoDir = root / "videos";
  std::filesystem::create_directories(videoDir);
  auto video = videoDir / "clip.mp4";
  { std::ofstream stream(video, std::ios::binary); stream << "video"; }
  session.SetRecordingFile(video);
  HANDLE writer = CreateFileW(video.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  assert(writer != INVALID_HANDLE_VALUE);
  assert(!session.TryArchive());
  CloseHandle(writer);
  assert(!session.TryArchive());
  std::this_thread::sleep_for(2100ms);
  assert(session.TryArchive());
  assert(std::filesystem::exists(session.Directory() / "clip.mp4"));
  assert(std::filesystem::exists(session.Directory() / "clip.txt"));
  assert(std::filesystem::exists(session.Directory() / "00-13-24-28.png"));
  std::ifstream text(session.Directory() / "clip.txt");
  std::string line;
  std::getline(text, line);
  assert(line.starts_with("Kill "));
  text.close();
  std::filesystem::remove_all(root);
}
