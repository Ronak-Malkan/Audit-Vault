#pragma once
#include "heartbeat_table.h"
#include "mempool_manager.h"
#include "chain_manager.h"
#include "election_state.h"
#include <grpcpp/grpcpp.h>
#include "block_chain.grpc.pb.h"
#include <atomic>
#include <thread>
#include <chrono>

/// Sends heartbeats periodically to peers.
class HeartbeatManager {
public:
  HeartbeatManager(
    const std::vector<std::string>& peers,
    const std::string&              self_addr,
    ElectionState&                  state,
    std::shared_ptr<MempoolManager> mempool,
    ChainManager&                   chain,
    std::shared_ptr<HeartbeatTable> table);

  ~HeartbeatManager();
  void start();
  void stop();


private:
  void loop();
  void syncMissingBlocks();
  void fetchBlocksFromPeer(const std::string& peer,
                           int64_t startId,
                           int64_t endId);

  std::vector<std::unique_ptr<blockchain::BlockChainService::Stub>> stubs_;
  std::vector<std::string> peer_addrs_;
  std::string              self_addr_;
  ElectionState&           state_;
  std::shared_ptr<MempoolManager> mempool_;
  ChainManager&                   chain_;
  std::shared_ptr<HeartbeatTable> table_;

  std::atomic<bool>        running_{false};
  std::thread              thr_;
  std::chrono::seconds interval_{10};
};
