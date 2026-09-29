#include "Config.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
using json = nlohmann::json;
Config Config::Load(const std::filesystem::path &p) {
  Config c;
  c.baseDirectory = p.parent_path();
  if (!std::filesystem::exists(p))
    throw std::runtime_error("config.json not found beside executable");
  std::ifstream f(p);
  json j;
  f >> j;
  auto a = j.value("capture", json::object());
  c.monitorIndex = a.value("monitorIndex", 0);
  c.fps = std::clamp(a.value("fps", 30), 5, 120);
  auto r = a.value("killFeedRegion", json::object());
  c.region = {r.value("x", 0.), r.value("y", .42), r.value("width", .30),
              r.value("height", .14)};
  auto d = j.value("detection", json::object());
  c.templatePath = d.value("templatePath", "assets/player_template.png");
  c.threshold = d.value("templateThreshold", .85);
  c.hashDistance = d.value("hashDistanceThreshold", 10);
  c.debugPreview = d.value("debugPreview", false);
  auto center = d.value("centerKillConfirmation", json::object());
  c.centerDetection = center.value("enabled", true);
  c.skullTemplatePath =
      center.value("templatePath", "assets/Killfeed_KillConfirmed.png");
  c.skullThreshold = center.value("templateThreshold", .70);
  c.fusionWindowMs = center.value("fusionWindowMs", 1200);
  auto cr = center.value("region", json::object());
  c.centerRegion = {cr.value("x", .41), cr.value("y", .64),
                    cr.value("width", .18), cr.value("height", .05)};
  auto n = j.value("nvidia", json::object());
  c.hotkey = n.value("hotkeyDetection", true);
  c.recordingDirectory = n.value("recordingDirectory", "D:\\Videos\\NVIDIA");
  c.watchDirectory = n.value("watchRecordingDirectory", true);
  c.confirmationTimeoutMs = n.value("recordingConfirmationTimeoutMs", 5000);
  c.outputDirectory = j.value("outputDirectory", "output");
  c.timestampOffsetMs = j.value("timestampOffsetMs", 0);
  auto valid = [](const Region &q) {
    return q.x >= 0 && q.y >= 0 && q.width > 0 && q.height > 0 &&
           q.x + q.width <= 1 && q.y + q.height <= 1;
  };
  if (!valid(c.region) || !valid(c.centerRegion))
    throw std::runtime_error("capture regions must be normalized inside 0..1");
  return c;
}
std::filesystem::path Config::Resolve(const std::filesystem::path &p) const {
  return p.is_absolute() ? p : baseDirectory / p;
}
