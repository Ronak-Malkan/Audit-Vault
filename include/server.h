#pragma once

#include "common.grpc.pb.h"          // common::FileAudit, etc.
#include "file_audit.grpc.pb.h"     // fileaudit::FileAuditService, FileAuditResponse
#include "block_chain.grpc.pb.h"    // blockchain::BlockChainService, etc.
#include "mempool_manager.h"
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
class BlockChainServiceImpl final
    : public blockchain::BlockChainService::Service {
public:
  explicit BlockChainServiceImpl(std::shared_ptr<MempoolManager> mempool);

  grpc::Status WhisperAuditRequest(
      grpc::ServerContext* context,
      const common::FileAudit* request,
      blockchain::WhisperResponse* response) override;

  grpc::Status ProposeBlock(
      grpc::ServerContext* context,
      const blockchain::BlockProposal* request,
      blockchain::BlockVoteResponse* response) override;

  grpc::Status VoteOnBlock(
      grpc::ServerContext* context,
      const blockchain::Vote* request,
      blockchain::BlockVoteResponse* response) override;

private:
  std::shared_ptr<MempoolManager> mempool_;
  std::string                     last_block_hash_;
};
