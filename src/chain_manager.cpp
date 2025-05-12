#include "chain_manager.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ChainManager::ChainManager(std::string path)
  : path_(std::move(path))
{
  loadFromDisk();
}

void ChainManager::loadFromDisk() {
  std::lock_guard<std::mutex> lk(mu_);
  blocks_.clear();
  std::ifstream in(path_);
  if (!in) return;  // no file yet

  json j;
  try {
    in >> j;
    if (!j.is_array()) {
      std::cerr << "[ChainManager] ERROR: chain.json not an array\n";
      return;
    }
    for (auto& el : j) {
      BlockMeta m;
      m.id            = el.value("id", 0LL);
      m.hash          = el.value("hash", std::string());
      m.previous_hash = el.value("previous_hash", std::string());
      m.merkle_root   = el.value("merkle_root", std::string());
      blocks_.push_back(std::move(m));
    }
  } catch (const std::exception& e) {
    std::cerr << "[ChainManager] ERROR parsing chain.json: "
              << e.what() << "\n";
  }
}

void ChainManager::writeToDisk() const {
  std::lock_guard<std::mutex> lk(mu_);
  json j = json::array();
  for (auto const& m : blocks_) {
    j.push_back({
      {"id",             m.id},
      {"hash",           m.hash},
      {"previous_hash",  m.previous_hash},
      {"merkle_root",    m.merkle_root}
    });
  }
  std::ofstream out(path_, std::ios::trunc);
  if (!out) {
    std::cerr << "[ChainManager] ERROR opening " << path_ << "\n";
    return;
  }
  out << j.dump(2) << "\n";
}

int64_t ChainManager::getLastID() const {
  std::lock_guard<std::mutex> lk(mu_);
  return blocks_.empty() ? 0 : blocks_.back().id;
}

std::string ChainManager::getLastHash() const {
  std::lock_guard<std::mutex> lk(mu_);
  return blocks_.empty() ? "" : blocks_.back().hash;
}

std::string ChainManager::getLastMerkleRoot() const {
  std::lock_guard<std::mutex> lk(mu_);
  return blocks_.empty() ? "" : blocks_.back().merkle_root;
}

std::vector<BlockMeta> ChainManager::getAll() const {
  std::lock_guard<std::mutex> lk(mu_);
  return blocks_;
}

void ChainManager::append(const BlockMeta& meta) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    blocks_.push_back(meta);
  }
  writeToDisk();
}
