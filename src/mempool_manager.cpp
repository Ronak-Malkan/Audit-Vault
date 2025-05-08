#include "mempool_manager.h"
#include <fstream>
#include <iostream>

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
