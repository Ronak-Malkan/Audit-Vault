#pragma once

#include <string>

/// Loads leader.json { leader_addr, batch_size, batch_interval_s }.
class LeaderConfig {
public:
  /// Throws std::runtime_error on parse error or missing fields.
  explicit LeaderConfig(const std::string& path);

  /// Address (host:port) of the leader.
  const std::string& getLeaderAddr() const { return leader_addr_; }

  /// Number of audits before forcing a block.
  int getBatchSize() const { return batch_size_; }

  /// Seconds to wait before forcing a block.
  int getBatchIntervalSec() const { return batch_interval_s_; }

private:
  std::string leader_addr_;
  int         batch_size_;
  int         batch_interval_s_;
};
