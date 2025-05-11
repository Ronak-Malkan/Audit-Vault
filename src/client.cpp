// src/client.cpp

#include "file_audit.grpc.pb.h"    // fileaudit::FileAuditService, FileAuditResponse
#include "common.grpc.pb.h"        // common::FileAudit
#include <grpcpp/grpcpp.h>

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <nlohmann/json.hpp>       // for ordered_json
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>

using ordered_json = nlohmann::ordered_json;

// Base64‐encode a byte buffer
static std::string Base64Encode(const unsigned char* buf, size_t len) {
  BIO* b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO* mem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, mem);
  BIO_write(b64, buf, (int)len);
  BIO_flush(b64);
  BUF_MEM* bptr;
  BIO_get_mem_ptr(b64, &bptr);
  std::string out(bptr->data, bptr->length);
  BIO_free_all(b64);
  return out;
}

// Load entire file into string
static std::string Slurp(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}

// Sign data with SHA256+RSA using a PEM private key
static std::vector<unsigned char> SignData(
    const std::string& data,
    const std::string& privkey_pem_path) 
{
  auto pem = Slurp(privkey_pem_path);
  BIO* bio = BIO_new_mem_buf(pem.data(), (int)pem.size());
  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (!pkey) {
    std::cerr << "ERROR loading private key\n";
    exit(1);
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey);
  EVP_DigestSignUpdate(ctx, data.data(), data.size());
  size_t sig_len = 0;
  EVP_DigestSignFinal(ctx, nullptr, &sig_len);
  std::vector<unsigned char> sig(sig_len);
  EVP_DigestSignFinal(ctx, sig.data(), &sig_len);
  sig.resize(sig_len);

  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);
  return sig;
}

int main() {
  // 1) Build your audit
  common::FileAudit req;
  req.set_req_id("smoke1");
  req.mutable_file_info()->set_file_id("file123");
  req.mutable_file_info()->set_file_name("important.docx");
  req.mutable_user_info()->set_user_id("user42");
  req.mutable_user_info()->set_user_name("alice");
  req.set_access_type(common::READ);
  int64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch()
               ).count();
  req.set_timestamp(ts);

  // 2) Canonical JSON with sorted keys
  ordered_json j;
  j["access_type"]       = req.access_type();
  j["file_info"]         = {{"file_id", req.file_info().file_id()},
                            {"file_name", req.file_info().file_name()}};
  j["req_id"]            = req.req_id();
  j["timestamp"]         = req.timestamp();
  j["user_info"]         = {{"user_id", req.user_info().user_id()},
                            {"user_name", req.user_info().user_name()}};

  std::string payload = j.dump(); 
  std::cout << "[client] payload = " << payload << "\n";

  // 3) Sign
  auto sig = SignData(payload, "../keys/client_private.pem");
  req.set_signature(Base64Encode(sig.data(), sig.size()));

  // 4) Attach public key
  req.set_public_key(Slurp("../keys/client_public.pem"));

  // 5) Send
  auto channel = grpc::CreateChannel(
      "0.0.0.0:50051", grpc::InsecureChannelCredentials());
  auto stub = fileaudit::FileAuditService::NewStub(channel);

  grpc::ClientContext ctx;
  fileaudit::FileAuditResponse resp;
  grpc::Status status = stub->SubmitAudit(&ctx, req, &resp);

  if (!status.ok()) {
    std::cerr << "RPC failed: " << status.error_message() << "\n";
    return 1;
  }
  std::cout << "Got response: req_id=" << resp.req_id()
            << ", status="  << resp.status()  << "\n";
  return 0;
}
