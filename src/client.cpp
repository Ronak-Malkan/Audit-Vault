// src/client.cpp
#include "file_audit.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <iostream>

int main() {
  // 1) Create a channel to the server
  auto channel = grpc::CreateChannel(
      "127.0.0.1:50051", grpc::InsecureChannelCredentials());
  std::unique_ptr<common::FileAuditService::Stub> stub =
      common::FileAuditService::NewStub(channel);

  // 2) Build a request
  common::FileAudit req;
  req.set_req_id("smoke1");
  // (other fields can stay default/empty)

  // 3) Call SubmitAudit
  grpc::ClientContext ctx;
  common::FileAuditResponse resp;
  grpc::Status status = stub->SubmitAudit(&ctx, req, &resp);

  // 4) Check result
  if (!status.ok()) {
    std::cerr << "RPC failed: " << status.error_message() << std::endl;
    return 1;
  }
  std::cout << "Got response: req_id=" << resp.req_id()
            << ", status="  << resp.status()  << std::endl;
  return 0;
}
