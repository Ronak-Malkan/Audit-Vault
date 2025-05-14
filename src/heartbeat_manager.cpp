#include "heartbeat_manager.h"
#include <iostream>
#include <google/protobuf/util/json_util.h>  // MessageToJsonString
#include <filesystem>
#include <fstream>
#include "block_chain.grpc.pb.h"

namespace fs = std::filesystem;

HeartbeatManager::HeartbeatManager(
    const std::vector<std::string>& peers,
    const std::string&              self_addr,
    ElectionState&                  state,
    std::shared_ptr<MempoolManager> mempool,
    ChainManager&                   chain,
    std::shared_ptr<HeartbeatTable> table)
  : self_addr_(self_addr)
  , state_(state)
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
    req.set_current_leader_address(state_.getLeader());
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
      state_.getLeader(),
      chain_.getLastID(),
      (int64_t)mempool_->LoadAll().size()
    );
    table_->sweep();

    syncMissingBlocks();

    std::this_thread::sleep_for(interval_);
  }
}

void HeartbeatManager::syncMissingBlocks() {
  // find the alive peer with the highest block id > ours
  auto entries   = table_->all();
  int64_t local  = chain_.getLastID();
  int64_t highest = local;
  std::string best;
  for (auto& e : entries) {
    if (e.alive &&
        e.from_address != self_addr_ &&
        e.latest_block_id > highest)
    {
      highest = e.latest_block_id;
      best    = e.from_address;
    }
  }
  if (!best.empty()) {
    fetchBlocksFromPeer(best, local+1, highest);
  }
}

void HeartbeatManager::fetchBlocksFromPeer(
    const std::string& peer,
    int64_t startId,
    int64_t endId)
{
  // look up which stub corresponds to `peer`
  auto it = std::find(peer_addrs_.begin(), peer_addrs_.end(), peer);
  if (it == peer_addrs_.end()) return;
  auto& stub = stubs_[std::distance(peer_addrs_.begin(), it)];

  std::cout << "[Sync] fetching blocks " << startId
            << "–" << endId << " from " << peer << "\n";

  for (int64_t id = startId; id <= endId; ++id) {
    grpc::ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(1));

    blockchain::GetBlockRequest  gb_req;
    blockchain::GetBlockResponse gb_resp;
    gb_req.set_id(id);

    auto status = stub->GetBlock(&ctx, gb_req, &gb_resp);
    if (!status.ok() || gb_resp.status() != "success") {
      std::cerr << "[Sync] failed to get block " << id
                << ": " << (status.ok()
                            ? gb_resp.error_message()
                            : status.error_message()) << "\n";
      return;
    }

    // commit into chain.json
    const auto& blk = gb_resp.block();
    BlockMeta meta { blk.id(),
                     blk.hash(),
                     blk.previous_hash(),
                     blk.merkle_root() };
    chain_.append(meta);

    // write full block JSON file
    try {
      fs::create_directories("../blocks");
      std::string json;
      google::protobuf::util::MessageToJsonString(blk, &json);
      std::ofstream out("../blocks/block_" + std::to_string(id) + ".json");
      out << json;
      std::cout << "[Sync] committed block " << id << "\n";
    } catch (std::exception& e) {
      std::cerr << "[Sync] error writing block file " << id
                << ": " << e.what() << "\n";
      return;
    }
  }
}