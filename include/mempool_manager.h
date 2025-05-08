#pragma once

#include "file_audit.grpc.pb.h"
#include <mutex>
#include <string>

/// Thread-safe appender for your mempool file.
class MempoolManager {
public:
  /// Construct with the file path (e.g. "../mempool.dat").
  explicit MempoolManager(std::string path);

  /// Append one audit (writes “req_id,file_id\n”) under lock.
  void Append(const common::FileAudit& audit);

private:
  std::mutex     mu_;
  std::string    path_;
};
