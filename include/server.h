#pragma once

#include "common.grpc.pb.h"          // common::FileAudit, etc.
#include "file_audit.grpc.pb.h"     // fileaudit::FileAuditService, FileAuditResponse
#include "block_chain.grpc.pb.h"    // blockchain::BlockChainService, etc.
#include "mempool_manager.h"
#include "chain_manager.h"
#include "heartbeat_table.h"
#include "election_state.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <vector>

/// Handles client submissions and gossips them out.
class FileAuditServiceImpl final
    : public fileaudit::FileAuditService::Service {
public:
  FileAuditServiceImpl(
    const std::vector<std::string>& peers,
    std::shared_ptr<MempoolManager> mempool);

  std::vector<std::unique_ptr<blockchain::BlockChainService::Stub>>& getGossipStubs();

  // Note: response is in the fileaudit namespace now
  grpc::Status SubmitAudit(
      grpc::ServerContext* context,
      const common::FileAudit* request,
      fileaudit::FileAuditResponse* response) override;

private:
  std::vector<std::unique_ptr<blockchain::BlockChainService::Stub>> gossip_stubs_;
  std::shared_ptr<MempoolManager> mempool_;
};

/// Handles incoming gossip & block proposals.
class BlockChainServiceImpl final : public blockchain::BlockChainService::Service {
public:
  BlockChainServiceImpl(
      std::shared_ptr<MempoolManager> mempool,
      ChainManager& chain,
      std::shared_ptr<HeartbeatTable> hb_table,
      ElectionState& election_state,
      std::string self_addr);

  grpc::Status WhisperAuditRequest(
      grpc::ServerContext* context,
      const common::FileAudit* request,
      blockchain::WhisperResponse* response) override;

  grpc::Status ProposeBlock(
      grpc::ServerContext* context,
      const blockchain::Block* request,
      blockchain::BlockVoteResponse* response) override;

  grpc::Status CommitBlock(
      grpc::ServerContext* context,
      const blockchain::Block* request,
      blockchain::BlockCommitResponse* response) override;

  grpc::Status GetBlock(
      grpc::ServerContext* context,
      const blockchain::GetBlockRequest* request,
      blockchain::GetBlockResponse* response) override;

  grpc::Status SendHeartbeat(
      grpc::ServerContext* context,
      const blockchain::HeartbeatRequest* request,
      blockchain::HeartbeatResponse* response) override;

  grpc::Status TriggerElection(
      grpc::ServerContext* context,
      const blockchain::TriggerElectionRequest* request,
      blockchain::TriggerElectionResponse* response) override;

  grpc::Status NotifyLeadership(
      grpc::ServerContext* context,
      const blockchain::NotifyLeadershipRequest* request,
      blockchain::NotifyLeadershipResponse* response) override;

  // grpc::Status VoteOnBlock(
  //     grpc::ServerContext* context,
  //     const blockchain::Vote* request,
  //     blockchain::BlockVoteResponse* response) override;

private:
  std::shared_ptr<MempoolManager> mempool_;
  ChainManager&                   chain_;
  std::shared_ptr<HeartbeatTable> hb_table_;
  ElectionState&                  state_;
  std::string                     self_addr_;
};
