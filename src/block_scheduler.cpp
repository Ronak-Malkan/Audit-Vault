// src/block_scheduler.cpp

#include "block_scheduler.h"
#include "merkle_tree.h"                    // SHA256Hex, ComputeMerkleRoot
#include <nlohmann/json.hpp>                // ordered_json
#include <chrono>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;
using ordered_json = nlohmann::ordered_json;

static constexpr auto kPeerRpcTimeoutMs = 200;

BlockScheduler::BlockScheduler(
    std::shared_ptr<MempoolManager> mempool,
    ChainManager&                    chain,
    StubList&                        stubs,
    const LeaderConfig&              cfg,
    std::function<bool()>            isLeaderFn
)
  : mempool_(std::move(mempool))
  , chain_(chain)
  , stubs_(stubs)
  , cfg_(cfg)
  , isLeaderFn_(std::move(isLeaderFn))
{}

BlockScheduler::~BlockScheduler() {
  stop();
}

void BlockScheduler::start() {
  if (running_.exchange(true)) return;  // already running
  thr_ = std::thread(&BlockScheduler::loop, this);
}

void BlockScheduler::stop() {
  running_ = false;
  if (thr_.joinable()) thr_.join();
}

void BlockScheduler::loop() {
  using namespace std::chrono;
  while (running_) {
    auto t0 = steady_clock::now();

    // Wait until enough audits or timeout
    while (running_) {
      auto audits = mempool_->LoadAll();
      if ((int)audits.size() >= cfg_.getBatchSize()) break;
      if (steady_clock::now() - t0 
          >= seconds(cfg_.getBatchIntervalSec()))
        break;
      std::this_thread::sleep_for(milliseconds(100));
    }
    if (!running_) break;

    auto pending = mempool_->LoadAll();
    std::cout << "[Scheduler] woke up: " 
              << pending.size() << " audits pending\n";

    if (pending.empty()) {
      std::cout << "[Scheduler] no audits pending, skipping block creation\n";
      continue;
    }

    if (isLeaderFn_()) {
      std::cout << "[Scheduler] I am leader, creating block\n";
      createAndBroadcastBlock(std::move(pending));
    } else {
      std::cout << "[Scheduler] not leader, skipping\n";
    }
  }
}

void BlockScheduler::createAndBroadcastBlock(
    std::vector<common::FileAudit> pending
) {
  // 1) Sort pending by (timestamp, req_id)
   std::sort(pending.begin(), pending.end(),
     [](auto& a, auto& b){
       if (a.timestamp() != b.timestamp())
         return a.timestamp() < b.timestamp();
       return a.req_id() < b.req_id();
     }
   );

  // 2) Build Merkle root
  std::vector<std::string> leaf_hashes;
  leaf_hashes.reserve(pending.size());
  for (auto& a : pending) {
    ordered_json j;
    j["access_type"] = a.access_type();
    j["file_info"]   = {
      {"file_id",   a.file_info().file_id()},
      {"file_name", a.file_info().file_name()}
    };
    j["req_id"]      = a.req_id();
    j["timestamp"]   = a.timestamp();
    j["user_info"]   = {
      {"user_id",   a.user_info().user_id()},
      {"user_name", a.user_info().user_name()}
    };
    leaf_hashes.push_back(SHA256Hex(j.dump()));
   }
  
  auto merkle = ComputeMerkleRoot(leaf_hashes);

  // 3) Fill Block proto
  blockchain::Block block;
  int64_t id = chain_.getLastID() + 1;
  block.set_id(id);
  block.set_previous_hash(chain_.getLastHash());
  block.set_merkle_root(merkle);

  for (auto& a : pending) {
    *block.add_audits() = a;
  }

  // 4) Compute block_hash by concatenating:
//    id + previous_hash + merkle_root + JSON(audit1)+JSON(audit2)+…
  std::string audits_concat;
  audits_concat.reserve(pending.size() * 128);  // pre-reserve for efficiency
  for (auto& a : pending) {
    ordered_json j2;
    j2["access_type"] = a.access_type();
    j2["file_info"]   = {
      {"file_id",   a.file_info().file_id()},
      {"file_name", a.file_info().file_name()}
    };
    j2["req_id"]      = a.req_id();
    j2["timestamp"]   = a.timestamp();
    j2["user_info"]   = {
      {"user_id",   a.user_info().user_id()},
      {"user_name", a.user_info().user_name()}
    };
    // exactly the same compact, sorted JSON string your Python code uses:
    audits_concat += j2.dump();
  }
  // build exactly: "<id><previous_hash><merkle_root><audits_json…>"
  std::string header = std::to_string(id)
                    + block.previous_hash()
                    + merkle
                    + audits_concat;
  block.set_hash(SHA256Hex(header));


  // 6) Send ProposeBlock to all peers
  bool all_yes = true;
  for (auto& stub : stubs_) {
    grpc::ClientContext ctx;
    // enforce a deadline on this RPC
    ctx.set_deadline(
      std::chrono::system_clock::now() +
      std::chrono::milliseconds(kPeerRpcTimeoutMs)
    );
    blockchain::BlockVoteResponse vote_resp;
    auto status = stub->ProposeBlock(&ctx, block, &vote_resp);
    if (!vote_resp.vote()) {
      all_yes = false;
      std::cerr << "[Scheduler] proposal rejected: "
                << (status.ok() ? vote_resp.error_message() : status.error_message())
                << "\n";
      break;
    }
  }
  if (!all_yes) return;

  //CommitBlock RPC
  for (auto& stub : stubs_) {
    grpc::ClientContext ctx;
    // enforce a deadline on this RPC too
    ctx.set_deadline(
       std::chrono::system_clock::now() +
      std::chrono::milliseconds(kPeerRpcTimeoutMs)
    );
    blockchain::BlockCommitResponse commit_resp;
    auto status = stub->CommitBlock(&ctx, block, &commit_resp);
    if (!status.ok() || commit_resp.status() != "success") {
      std::cerr << "[Scheduler] commit failed: "
                << (status.ok()
                    ? commit_resp.error_message()
                    : status.error_message())
                << "\n";
    }
  }

  // 7) Locally commit: update chain.json + prune mempool
  {
    BlockMeta meta {
      id,
      block.hash(),
      block.previous_hash(),
      block.merkle_root()
    };
    chain_.append(meta);
  }
  std::vector<std::string> ids;
  ids.reserve(pending.size());
  for (auto& a : pending) ids.push_back(a.req_id());
  mempool_->RemoveBatch(ids);

  // 8) Dump full block JSON to file
  fs::create_directories("../blocks");
  std::string block_json;
  google::protobuf::util::MessageToJsonString(block, &block_json);
  std::ofstream bout("../blocks/block_" + std::to_string(id) + ".json");
  if (!bout) {
    std::cerr << "[Scheduler] failed to write block file\n";
  } else {
    bout << block_json;
    std::cout << "[Scheduler] wrote ../blocks/block_"
              << id << ".json\n";
  }

  std::cout << "[Scheduler] committed block " << id
            << " (" << pending.size() << " audits)\n";
}
