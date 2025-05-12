#pragma once

#include "common.grpc.pb.h"        // common::FileAudit
#include "block_chain.grpc.pb.h"   // blockchain::Block, BlockVoteResponse, BlockCommitResponse
#include "chain_manager.h"
#include "leader_config.h"
#include "mempool_manager.h"
#include "merkle_tree.h"

#include <grpcpp/grpcpp.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

/// Periodically triggers block proposal when thresholds are met.
class BlockScheduler {
public:
  using StubList =
    std::vector<std::unique_ptr<blockchain::BlockChainService::Stub>>;

  BlockScheduler(
    std::shared_ptr<MempoolManager> mempool,
    ChainManager&                    chain,
    StubList&                        stubs,
    const LeaderConfig&              cfg,
    std::function<bool()>            isLeaderFn
  );

  ~BlockScheduler();

  /// Launches the background scheduling thread.
  void start();

  /// Stops the scheduler (and joins the thread).
  void stop();

private:
  void loop();
  void createAndBroadcastBlock(std::vector<common::FileAudit> pending);

  std::shared_ptr<MempoolManager> mempool_;
  ChainManager&                   chain_;
  StubList&                       stubs_;
  const LeaderConfig&             cfg_;
  std::function<bool()>           isLeaderFn_;

  std::thread                     thr_;
  std::atomic<bool>               running_{false};
};
