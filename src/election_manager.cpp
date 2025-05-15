#include "election_manager.h"
#include <iostream>

ElectionManager::ElectionManager(
    const std::vector<std::string>& peers,
    const std::string& self_addr,
    std::shared_ptr<HeartbeatTable> hb_table,
    ElectionState& state,
    std::shared_ptr<MempoolManager> mempool,
    ChainManager& chain
)
  : self_addr_(self_addr)
  , hb_table_(std::move(hb_table))
  , state_(state)
  , mempool_(std::move(mempool))
  , chain_(chain)
{
  for (auto& addr : peers) {
    auto chan = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    stubs_.push_back(
      blockchain::BlockChainService::NewStub(chan)
    );
    peer_addrs_.push_back(addr);
  }
}

ElectionManager::~ElectionManager() {
  stop();
}

void ElectionManager::start() {
  if (running_.exchange(true)) return;
  thr_ = std::thread(&ElectionManager::loop, this);
}

void ElectionManager::stop() {
  running_ = false;
  if (thr_.joinable()) thr_.join();
}

void ElectionManager::loop() {
  std::this_thread::sleep_for(std::chrono::seconds(30));
  while (running_) {
    // 1) sweep stale heartbeats
    hb_table_->sweep();

    auto leader = state_.getLeader();
    bool needElection =
      leader.empty() ||
      [&] {
        for (auto& e : hb_table_->all())
          if (e.from_address == leader && !e.alive)
            return true;
        return false;
      }();

    if (needElection) {
      std::cout << "[ElectionManager] triggering election\n";

      // 2) vote for self
      int votes = 1;
      const int majority = (int)peer_addrs_.size()/2 + 1;

      // 3) ask others
      for (size_t i = 0; i < stubs_.size(); ++i) {
        grpc::ClientContext ctx;
        ctx.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(1));
        blockchain::TriggerElectionRequest req;
        req.set_term(0);  // unused
        req.set_address(self_addr_);
        blockchain::TriggerElectionResponse resp;

        auto status = stubs_[i]->TriggerElection(&ctx, req, &resp);
        if (status.ok() && resp.vote()) {
          votes++;
          std::cout << "[ElectionManager] got vote from " << peer_addrs_[i]
                    << "\n";
        } else {
          std::cout << "[ElectionManager] no vote from " << peer_addrs_[i]
                    << ": " << status.error_message() << "\n";
        }
      }

      // 4) if won majority
      if (votes >= majority) {
        state_.setLeader(self_addr_);
        std::cout << "[ElectionManager] I won election, leader=" << self_addr_ << "\n";

        // 5) notify all peers
        for (size_t i = 0; i < stubs_.size(); ++i) {
          grpc::ClientContext ctx;
          ctx.set_deadline(std::chrono::system_clock::now() +
                           std::chrono::seconds(1));
          blockchain::NotifyLeadershipRequest req2;
          req2.set_address(self_addr_);
          blockchain::NotifyLeadershipResponse resp2;
          stubs_[i]->NotifyLeadership(&ctx, req2, &resp2);
        }
      } else {
        std::cout << "[ElectionManager] lost election (" << votes
                  << "/" << peer_addrs_.size() << ")\n";
      }
    }

    std::this_thread::sleep_for(interval_);
  }
}
