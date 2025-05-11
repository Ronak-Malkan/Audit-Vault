// src/server.cpp

#include "server.h"
#include "merkle_tree.h"                          // SHA256Hex, ComputeMerkleRoot
#include <google/protobuf/util/json_util.h>       // MessageToJsonString
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <iostream>
#include <unordered_set>
#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;

// Utility: Base64-decode into a byte vector
static std::vector<unsigned char> Base64Decode(const std::string& b64) {
  BIO* bmem  = BIO_new_mem_buf(b64.data(), (int)b64.size());
  BIO* b64f  = BIO_new(BIO_f_base64());
  BIO_set_flags(b64f, BIO_FLAGS_BASE64_NO_NL);
  bmem = BIO_push(b64f, bmem);
  std::vector<unsigned char> out(b64.size());
  int len = BIO_read(bmem, out.data(), (int)out.size());
  BIO_free_all(bmem);
  if (len < 0) return {};
  out.resize(len);
  return out;
}

// Utility: verify signature_b64 over data using PEM public key
static bool VerifySignature(
    const std::string& data,
    const std::string& signature_b64,
    const std::string& pubkey_pem)
{
  auto sig = Base64Decode(signature_b64);
  std::cout << "[VerifySignature] decoded signature (len="
            << sig.size() << ")\n";
  BIO* bio = BIO_new_mem_buf(pubkey_pem.data(), (int)pubkey_pem.size());
  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
  BIO_free(bio);
  std::cout << "[VerifySignature] decoded public key (len="
            << pubkey_pem << ")\n";
  std::cout << "Signature: " << signature_b64 << "\n";
  std::cout << "Data: " << data << "\n";
  if (!pkey) return false;

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  
  EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pkey);

  EVP_PKEY_CTX *pctx = EVP_MD_CTX_pkey_ctx(ctx);
  if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING) <= 0) {
    std::cerr << "Failed to set RSA padding\n";
  }

  EVP_DigestVerifyUpdate(ctx, data.data(), data.size());
  int rc = EVP_DigestVerifyFinal(ctx, sig.data(), sig.size());
  std::cout << "[VerifySignature] EVP_DigestVerifyFinal rc="
            << rc << "\n";
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);
  return rc == 1;
}

// -- FileAuditServiceImpl -------------------------------------------------

FileAuditServiceImpl::FileAuditServiceImpl(
    const std::vector<std::string>& peers,
    std::shared_ptr<MempoolManager> mempool)
  : mempool_(std::move(mempool))
{
  for (auto& addr : peers) {
    std::cout << "[FileAuditServiceImpl] gossip to peer="
              << addr << "\n";
    auto chan = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    gossip_stubs_.push_back(
      blockchain::BlockChainService::NewStub(chan));
  }
}

grpc::Status FileAuditServiceImpl::SubmitAudit(
    grpc::ServerContext* /*ctx*/,
    const common::FileAudit* request,
    fileaudit::FileAuditResponse* response)
{

  std::cout << "[SubmitAudit] client signature="
            << request->signature() << "\n";
  std::cout << "[SubmitAudit] client public key="
            << request->public_key() << "\n";
  std::cout << "[SubmitAudit] verifying client signature\n";

  // 1) Canonical JSON payload (sorted keys)
  ordered_json j;
  j["access_type"] = request->access_type();
  j["file_info"]   = {{"file_id",   request->file_info().file_id()},
                      {"file_name", request->file_info().file_name()}};
  j["req_id"]      = request->req_id();
  j["timestamp"]   = request->timestamp();
  j["user_info"]   = {{"user_id",   request->user_info().user_id()},
                      {"user_name", request->user_info().user_name()}};
  std::string payload = j.dump();
  std::cout << "[SubmitAudit] payload=" << payload << "\n";

  if (!VerifySignature(payload,
                       request->signature(),
                       request->public_key())) {
    return grpc::Status(
      grpc::StatusCode::INVALID_ARGUMENT,
      "Invalid client signature");
  }
  std::cout << "[SubmitAudit] verified client signature\n";

  // 2) Persist to mempool
  mempool_->Append(*request);

  // 3) Gossip to peers
  for (auto& stub : gossip_stubs_) {
    blockchain::WhisperResponse wr;
    grpc::ClientContext ctx2;
    auto st = stub->WhisperAuditRequest(&ctx2, *request, &wr);
    if (!st.ok()) {
      std::cerr << "[Gossip] to peer failed: "
                << st.error_message() << "\n";
    } else {
      std::cout << "[Gossip] to peer succeeded: "
                << wr.status() << "\n";
    }
  }

  // 4) Reply to client
  response->set_req_id(request->req_id());
  response->set_status("success");
  return grpc::Status::OK;
}

// -- BlockChainServiceImpl ------------------------------------------------

static bool VerifyClientSignatureStub(const common::FileAudit&) { return true; }
static bool VerifyProposerSignatureStub(const blockchain::BlockProposal&) { return true; }

BlockChainServiceImpl::BlockChainServiceImpl(
    std::shared_ptr<MempoolManager> mempool)
  : mempool_(std::move(mempool))
  , last_block_hash_("")  // no blocks yet
{}

grpc::Status BlockChainServiceImpl::WhisperAuditRequest(
    grpc::ServerContext* /*ctx*/,
    const common::FileAudit* request,
    blockchain::WhisperResponse* response)
{
  std::cout << "[WhisperAuditRequest] audit request received\n";
  std::cout << "  req_id:    " << request->req_id() << "\n";
  std::cout << "  file_info: id=" << request->file_info().file_id()
            << ", name=" << request->file_info().file_name() << "\n";
  std::cout << "  user_info: id=" << request->user_info().user_id()
            << ", name=" << request->user_info().user_name() << "\n";
  std::cout << "  access:    " << request->access_type()
            << ", timestamp=" << request->timestamp() << "\n";
  std::cout << "  signature: (len=" << request->signature().size() << ")\n";
  std::cout << "  public_key: (len=" << request->public_key().size() << ")\n";

  ordered_json j;
  j["access_type"] = request->access_type();
  j["file_info"]   = {{"file_id",   request->file_info().file_id()},
                      {"file_name", request->file_info().file_name()}};
  j["req_id"]      = request->req_id();
  j["timestamp"]   = request->timestamp();
  j["user_info"]   = {{"user_id",   request->user_info().user_id()},
                      {"user_name", request->user_info().user_name()}};
  std::string payload2 = j.dump();
  std::cout << "[SubmitAudit] payload=" << payload2 << "\n";


  if (!VerifySignature(payload2,
                       request->signature(),
                       request->public_key()))
  {
    std::cerr << "[WhisperAuditRequest] invalid signature for req_id="
              << request->req_id() << "\n";
    return grpc::Status(
      grpc::StatusCode::INVALID_ARGUMENT,
      "Invalid signature in gossiped audit");
  } else {
    std::cout << "[WhisperAuditRequest] verified signature for req_id="
              << request->req_id() << "\n";
  }

  // 2) Persist to mempool
  mempool_->Append(*request);

  // 3) Log that it's added
  std::cout << "[WhisperAuditRequest] audit added to mempool\n";

  // 4) Ack
  response->set_status("success");
  return grpc::Status::OK;
}

grpc::Status BlockChainServiceImpl::ProposeBlock(
    grpc::ServerContext* /*ctx*/,
    const blockchain::BlockProposal* /*request*/,
    blockchain::BlockVoteResponse* response)
{
  // For now, stubbed—actual verify+prune logic lives elsewhere
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status BlockChainServiceImpl::VoteOnBlock(
    grpc::ServerContext* /*ctx*/,
    const blockchain::Vote* request,
    blockchain::BlockVoteResponse* response)
{
  std::cout << "[VoteOnBlock] block_id="
            << request->block_id() << std::endl;
  response->set_success(true);
  return grpc::Status::OK;
}
