#include "mempool_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

using google::protobuf::util::MessageToJsonString;
using google::protobuf::util::JsonStringToMessage;

// Constructor: just capture the path
MempoolManager::MempoolManager(std::string path)
    : path_(std::move(path)) {}

// Append one audit as JSON line
void MempoolManager::Append(const common::FileAudit& audit) {
  std::lock_guard<std::mutex> lk(mu_);
  std::ofstream out(path_, std::ios::app);
  if (!out) {
    std::cerr << "[MempoolManager] failed to open " << path_ << "\n";
    return;
  }
  std::string json;
  auto status = MessageToJsonString(audit, &json);
  if (!status.ok()) {
    std::cerr << "[MempoolManager] JSON serialization failed: "
              << status.ToString() << "\n";
    return;
  }
  out << json << "\n";
}

// Load all audits by parsing JSON lines
std::vector<common::FileAudit> MempoolManager::LoadAll() const {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<common::FileAudit> all;
  std::ifstream in(path_);
  if (!in) return all;

  std::string line;
  while (std::getline(in, line)) {
    common::FileAudit a;
    auto status = JsonStringToMessage(line, &a);
    if (!status.ok()) {
      std::cerr << "[MempoolManager] JSON parse error: "
                << status.ToString() << "\n";
      continue;
    }
    all.push_back(std::move(a));
  }
  return all;
}

// Remove a batch of req_ids by rewriting the file
void MempoolManager::RemoveBatch(const std::vector<std::string>& ids) {
  std::lock_guard<std::mutex> lk(mu_);
  std::unordered_set<std::string> to_remove(ids.begin(), ids.end());

  // Read everything
  std::vector<common::FileAudit> keep;
  {
    std::ifstream in(path_);
    std::string line;
    while (std::getline(in, line)) {
      common::FileAudit a;
      auto st = JsonStringToMessage(line, &a);
      if (!st.ok()) continue;
      if (!to_remove.count(a.req_id())) {
        keep.push_back(std::move(a));
      }
    }
  }

  // Rewrite
  std::ofstream out(path_, std::ios::trunc);
  if (!out) {
    std::cerr << "[MempoolManager] failed to reopen " << path_ << "\n";
    return;
  }
  for (auto& a : keep) {
    std::string json;
    auto status = MessageToJsonString(a, &json);
    if (!status.ok()) {
      std::cerr << "[MempoolManager] JSON serialization failed: "
                << status.ToString() << "\n";
      continue;
    }
    out << json << "\n";
  }
}
