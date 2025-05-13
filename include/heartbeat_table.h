#pragma once
#include <string>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <vector>
#include <iostream>

/// One row in the heartbeat table.
struct HeartbeatEntry {
  std::string from_address;
  std::string leader_address;
  int64_t     latest_block_id;
  int64_t     mem_pool_size;
  std::chrono::steady_clock::time_point last_seen;
  bool        alive = true;
};

/// Stores the freshest heartbeat from each peer, marking them dead after timeout.
class HeartbeatTable {
public:
  /// timeout_sec: how long before we mark a peer dead.
  explicit HeartbeatTable(int timeout_sec = 4)
    : timeout_(std::chrono::seconds(timeout_sec)) {}

  /// Called whenever we get a heartbeat from `from`.  
  void update(const std::string& from,
              const std::string& leader,
              int64_t latest_block_id,
              int64_t mem_pool_size) {
    std::lock_guard<std::mutex> lk(mu_);
    auto& e = table_[from];
    e.from_address     = from;
    e.leader_address   = leader;
    e.latest_block_id  = latest_block_id;
    e.mem_pool_size    = mem_pool_size;
    e.last_seen        = std::chrono::steady_clock::now();
    e.alive            = true;
  }

  /// Remove peers that have not been heard from in `timeout_`.
  void sweep() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& [k,e] : table_) {
      if (now - e.last_seen > timeout_ && e.alive) {
        std::cout << "[HeartbeatTable] marking " << k
                  << " as dead (timeout)\n";
        e.alive = false;
      }
    }
  }

  /// Snapshot of all entries.
  std::vector<HeartbeatEntry> all() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<HeartbeatEntry> out;
    out.reserve(table_.size());
    for (auto const& kv : table_) {
      out.push_back(kv.second);
    }
    return out;
  }

private:
  mutable std::mutex mu_;
  std::unordered_map<std::string,HeartbeatEntry> table_;
  std::chrono::seconds timeout_;
};
