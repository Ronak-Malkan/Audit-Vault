#include "config_loader.h"
#include "mempool_manager.h"
#include "server.h"
#include <grpcpp/grpcpp.h>
#include <iostream>

int main(int argc, char** argv) {
  // Load peers (exec in build/)
  auto peers = LoadPeers("../peers.json");
  std::cout << "Loaded peers:\n";
  for (auto& p : peers) std::cout << "  - " << p << "\n";

  // Shared mempool manager
  auto mempool = std::make_shared<MempoolManager>("../mempool.dat");

  // 2a) Reload existing audits and report
    auto pending = mempool->LoadAll();
    std::cout << "Recovered " << pending.size() 
            << " audits from mempool:\n";
    for (auto& a : pending) {
    std::cout << "  • req_id=" << a.req_id()
                << ", file_id=" << a.file_info().file_id()
                << "\n";
    }


  // Services
  FileAuditServiceImpl  file_svc(peers,   mempool);
  BlockChainServiceImpl block_svc(mempool);

  // Server
  std::string addr = "0.0.0.0:50051";
  if (argc > 1) {
    addr = argv[1];
  }
  grpc::ServerBuilder builder;
  builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&file_svc);
  builder.RegisterService(&block_svc);

  auto server = builder.BuildAndStart();
  std::cout << "Server listening on " << addr << std::endl;
  server->Wait();
  return 0;
}
