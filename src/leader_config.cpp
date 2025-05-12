#include "leader_config.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

LeaderConfig::LeaderConfig(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("Cannot open leader.json: " + path);
  }

  json j;
  try {
    in >> j;
  } catch (const std::exception& e) {
    throw std::runtime_error(
      std::string("Error parsing leader.json: ") + e.what());
  }

  // Required fields
  if (!j.contains("leader_addr")      ||
      !j.contains("batch_size")       ||
      !j.contains("batch_interval_s")) {
    throw std::runtime_error(
      "leader.json missing one of [leader_addr,batch_size,batch_interval_s]");
  }

  leader_addr_       = j.at("leader_addr").get<std::string>();
  batch_size_        = j.at("batch_size").get<int>();
  batch_interval_s_  = j.at("batch_interval_s").get<int>();
}
