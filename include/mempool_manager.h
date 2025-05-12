#pragma once

#include "common.pb.h"     
#include <mutex>
#include <string>
#include <vector>
#include <google/protobuf/util/json_util.h> 

/// Thread-safe manager for the mempool file.
class MempoolManager {
public:
  /// Construct with the file path (e.g. "../mempool.dat").
  explicit MempoolManager(std::string path);

  /// Append one audit (writes “req_id,file_id\n”) under lock.
  void Append(const common::FileAudit& audit);

  /// Load *all* previously appended audits from disk.
  /// If the file doesn’t exist or is empty, returns an empty vector.
  std::vector<common::FileAudit> LoadAll() const;

  /// Remove every audit whose req_id is in `ids`, rewriting the file.
  void RemoveBatch(const std::vector<std::string>& ids);

private:
  mutable std::mutex mu_;
  std::string        path_;
};
