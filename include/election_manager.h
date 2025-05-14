#pragma once

#include "heartbeat_table.h"
#include "election_state.h"
#include "chain_manager.h"
#include "mempool_manager.h"
#include "block_chain.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <chrono>

/// Periodically checks heartbeats & triggers elections.
class ElectionManager {
public:
  ElectionManager(
    const std::vector<std::string>& peers,
    const std::string& self_addr,
    std::shared_ptr<HeartbeatTable> hb_table,
    ElectionState&                 state,
    std::shared_ptr<MempoolManager> mempool,
    ChainManager&                   chain
  );
  ~ElectionManager();

  /// Launch background election thread.
  void start();

  /// Stop thread.
  void stop();

private:
  void loop();

  std::vector<std::unique_ptr<blockchain::BlockChainService::Stub>> stubs_;
  std::vector<std::string> peer_addrs_;
  std::string              self_addr_;
  std::shared_ptr<HeartbeatTable> hb_table_;
  ElectionState&           state_;
  std::shared_ptr<MempoolManager> mempool_;
  ChainManager&            chain_;

  std::atomic<bool>        running_{false};
  std::thread              thr_;
  std::chrono::seconds     interval_{2};
};
