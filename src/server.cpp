#include "server.h"
#include <iostream>

// FileAuditServiceImpl

FileAuditServiceImpl::FileAuditServiceImpl(
    const std::vector<std::string>& peers,
    std::shared_ptr<MempoolManager> mempool)
  : mempool_(std::move(mempool))
{
  for (auto& addr : peers) {
    auto chan = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    gossip_stubs_.push_back(
      blockchain::BlockChainService::NewStub(chan));
  }
}

grpc::Status FileAuditServiceImpl::SubmitAudit(
    grpc::ServerContext* /*ctx*/,
    const common::FileAudit* req,
    common::FileAuditResponse* resp)
{
  // 1) persist
  mempool_->Append(*req);

  // 2) gossip
  for (auto& stub : gossip_stubs_) {
    blockchain::WhisperResponse wr;
    grpc::ClientContext ctx;
    auto status = stub->WhisperAuditRequest(&ctx, *req, &wr);
    if (!status.ok()) {
      std::cerr << "[Gossip] to peer failed: "
                << status.error_message() << "\n";
    }
    else {
        std::cout << "[Gossip] to peer success\n";
    }
  }

  // 3) reply
  resp->set_req_id(req->req_id());
  resp->set_status("success");
  return grpc::Status::OK;
}

// BlockChainServiceImpl

BlockChainServiceImpl::BlockChainServiceImpl(
    std::shared_ptr<MempoolManager> mempool)
  : mempool_(std::move(mempool))
{}

grpc::Status BlockChainServiceImpl::WhisperAuditRequest(
    grpc::ServerContext* /*ctx*/,
    const common::FileAudit* req,
    blockchain::WhisperResponse* resp)
{
  mempool_->Append(*req);
  std::cout << "[WhisperAuditRequest] req_id="
                << req->req_id() << ", file_id="
                << req->file_id() << std::endl;
  resp->set_status("success");
  return grpc::Status::OK;
}

grpc::Status BlockChainServiceImpl::ProposeBlock(
    grpc::ServerContext* /*ctx*/,
    const blockchain::BlockProposal* request,
    blockchain::BlockVoteResponse* response) {
  std::cout << "[ProposeBlock] number="
            << request->block().block_number() << std::endl;
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status BlockChainServiceImpl::VoteOnBlock(
    grpc::ServerContext* /*ctx*/,
    const blockchain::Vote* request,
    blockchain::BlockVoteResponse* response) {
  std::cout << "[VoteOnBlock] block_id="
            << request->block_id() << std::endl;
  response->set_success(true);
  return grpc::Status::OK;
}
