#pragma once

#include "file_audit.grpc.pb.h"
#include "block_chain.grpc.pb.h"
#include "mempool_manager.h"
#include <grpcpp/grpcpp.h>
#include <string>
#include <vector>
#include <memory>

/// Handles client submissions and gossips them out.
class FileAuditServiceImpl final
    : public common::FileAuditService::Service {
public:
  FileAuditServiceImpl(
    const std::vector<std::string>& peers,
    std::shared_ptr<MempoolManager> mempool);

  grpc::Status SubmitAudit(
      grpc::ServerContext* context,
      const common::FileAudit* request,
      common::FileAuditResponse* response) override;

private:
  std::vector<std::unique_ptr<blockchain::BlockChainService::Stub>> gossip_stubs_;
  std::shared_ptr<MempoolManager> mempool_;
};

/// Handles incoming gossip from peers.
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
};
