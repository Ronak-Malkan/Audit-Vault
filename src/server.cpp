#include "server.h"
#include "merkle_tree.h"                          // SHA256Hex, ComputeMerkleRoot
#include <google/protobuf/util/json_util.h>      // MessageToJsonString
#include <unordered_set>
#include <iostream>

// -- FileAuditServiceImpl -------------------------------------------------

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
    } else {
      std::cout << "[Gossip] to peer success\n";
    }
  }

  // 3) reply
  resp->set_req_id(req->req_id());
  resp->set_status("success");
  return grpc::Status::OK;
}

// -- BlockChainServiceImpl ------------------------------------------------

// Helper stubs (you’ll replace with real RSA checks later):
static bool VerifyClientSignature(const common::FileAudit& a) {
  (void)a;
  return true;
}
static bool VerifyProposerSignature(const blockchain::BlockProposal& p) {
  (void)p;
  return true;
}

BlockChainServiceImpl::BlockChainServiceImpl(
    std::shared_ptr<MempoolManager> mempool)
  : mempool_(std::move(mempool))
  , last_block_hash_("")  // no blocks yet
{}

grpc::Status BlockChainServiceImpl::WhisperAuditRequest(
    grpc::ServerContext* /*ctx*/,
    const common::FileAudit* req,
    blockchain::WhisperResponse* resp)
{
  mempool_->Append(*req);
  std::cout << "[WhisperAuditRequest] audit added to mempool\n";
  resp->set_status("success");
  return grpc::Status::OK;
}

grpc::Status BlockChainServiceImpl::ProposeBlock(
    grpc::ServerContext* /*ctx*/,
    const blockchain::BlockProposal* req,
    blockchain::BlockVoteResponse* resp)
{
  const auto& blk = req->block();

  // 1) Verify Merkle root
  std::vector<std::string> leafs;
  for (const auto& a : blk.audits()) {
    std::string json;
    auto st = google::protobuf::util::MessageToJsonString(a, &json);
    if (!st.ok()) {
      std::cerr << "[ProposeBlock] JSON serialization failed for " 
                << a.req_id() << ": " << st.ToString() << "\n";
    }
    leafs.push_back(SHA256Hex(json));
  }
  if (ComputeMerkleRoot(leafs) != blk.merkle_root()) {
    resp->set_success(false);
    resp->set_message("bad merkle_root");
    return grpc::Status::OK;
  }

  // 2) Verify prev_hash
  if (blk.previous_block_hash() != last_block_hash_) {
    resp->set_success(false);
    resp->set_message("bad previous_block_hash");
    return grpc::Status::OK;
  }

  // 3) Verify client signatures
  for (const auto& a : blk.audits()) {
    if (!VerifyClientSignature(a)) {
      resp->set_success(false);
      resp->set_message("invalid client signature for " + a.req_id());
      return grpc::Status::OK;
    }
  }

  // 4) Verify proposer signature
  if (!VerifyProposerSignature(*req)) {
    resp->set_success(false);
    resp->set_message("invalid proposer signature");
    return grpc::Status::OK;
  }

  // 5) All good → prune mempool
  std::vector<std::string> ids;
  for (const auto& a : blk.audits()) {
    ids.push_back(a.req_id());
  }
  mempool_->RemoveBatch(ids);
  std::cout << "[ProposeBlock] pruned " << ids.size() << " audits\n";

  // 6) Update head
  last_block_hash_ = blk.block_hash();

  resp->set_success(true);
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
