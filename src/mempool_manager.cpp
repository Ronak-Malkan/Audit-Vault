#include "mempool_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

MempoolManager::MempoolManager(std::string path)
    : path_(std::move(path)) {}

void MempoolManager::Append(const common::FileAudit& audit) {
  std::lock_guard<std::mutex> lk(mu_);
  std::ofstream out(path_, std::ios::app);
  if (!out) {
    std::cerr << "[MempoolManager] failed to open " << path_ << "\n";
    return;
  }
  out << audit.req_id() << ","
      << audit.file_info().file_id() << "\n";
}

std::vector<common::FileAudit> MempoolManager::LoadAll() const {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<common::FileAudit> all;
  std::ifstream in(path_);
  if (!in) return all;

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::string req_id, file_id;
    if (!std::getline(ss, req_id, ',')) continue;
    std::getline(ss, file_id);  // may be empty
    common::FileAudit a;
    a.set_req_id(req_id);
    a.mutable_file_info()->set_file_id(file_id);
    all.push_back(std::move(a));
  }
  return all;
}

void MempoolManager::RemoveBatch(const std::vector<std::string>& ids) {
  std::lock_guard<std::mutex> lk(mu_);
  std::unordered_set<std::string> to_remove(ids.begin(), ids.end());

  // Read all entries
  std::vector<common::FileAudit> all;
  {
    std::ifstream in(path_);
    std::string line;
    while (std::getline(in, line)) {
      std::istringstream ss(line);
      std::string req_id, file_id;
      if (!std::getline(ss, req_id, ',')) continue;
      std::getline(ss, file_id);
      common::FileAudit a;
      a.set_req_id(req_id);
      a.mutable_file_info()->set_file_id(file_id);
      all.push_back(std::move(a));
    }
  }

  // Rewrite without removed
  std::ofstream out(path_, std::ios::trunc);
  if (!out) {
    std::cerr << "[MempoolManager] failed to reopen " << path_ << "\n";
    return;
  }
  for (auto& a : all) {
    if (to_remove.count(a.req_id()) == 0) {
      out << a.req_id() << "," << a.file_info().file_id() << "\n";
    }
  }
}
