#include "heartbeat_manager.h"
#include <iostream>

HeartbeatManager::HeartbeatManager(
    const std::vector<std::string>& peers,
    const std::string& self_addr,
    const LeaderConfig& cfg,
    std::shared_ptr<MempoolManager> mempool,
    ChainManager& chain,
    std::shared_ptr<HeartbeatTable> table)
  : self_addr_(self_addr)
  , cfg_(cfg)
  , mempool_(std::move(mempool))
  , chain_(chain)
  , table_(std::move(table))
{
  for (auto& addr : peers) {
    auto chan = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    stubs_.push_back(blockchain::BlockChainService::NewStub(chan));
    peer_addrs_.push_back(addr);
  }
}

HeartbeatManager::~HeartbeatManager() {
  stop();
}

void HeartbeatManager::start() {
  if (running_.exchange(true)) return;
  thr_ = std::thread(&HeartbeatManager::loop, this);
}

void HeartbeatManager::stop() {
  running_ = false;
  if (thr_.joinable()) thr_.join();
}

void HeartbeatManager::loop() {
  while (running_) {
    // Build request
    blockchain::HeartbeatRequest req;
    req.set_from_address(self_addr_);
    req.set_current_leader_address(cfg_.getLeaderAddr());
    req.set_latest_block_id(chain_.getLastID());
    req.set_mem_pool_size((int64_t)mempool_->LoadAll().size());

    // Send to each peer
    for (size_t i = 0; i < stubs_.size(); ++i) {
      auto& stub = stubs_[i];
      const auto& peer = peer_addrs_[i];

      grpc::ClientContext ctx;
      ctx.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(1));  // 1s timeout
      blockchain::HeartbeatResponse resp;
      auto status = stub->SendHeartbeat(&ctx, req, &resp);
      if (!status.ok()) {
        std::cerr << "[Heartbeat] to " << peer
                  << " failed: " << status.error_message() << "\n";
      }
    }

    // Also record our own heartbeat locally:
    table_->update(
      self_addr_,
      cfg_.getLeaderAddr(),
      chain_.getLastID(),
      (int64_t)mempool_->LoadAll().size()
    );

    table_->sweep();

    std::this_thread::sleep_for(interval_);
  }
}
