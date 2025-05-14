#include "config_loader.h"
#include "mempool_manager.h"
#include "server.h"
#include "chain_manager.h"
#include "leader_config.h"
#include "block_scheduler.h"
#include "heartbeat_manager.h"
#include "election_state.h"
#include "election_manager.h"
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

  // Leader config & chain state
  LeaderConfig cfg("../leader.json");
  ChainManager chain("../chain.json");

  auto hb_table = std::make_shared<HeartbeatTable>(15);

  ElectionState election_state;    

  
  std::string addr = "0.0.0.0:50051";
  if (argc > 1) {
    addr = argv[1];
  }

  // Services
  FileAuditServiceImpl  file_svc(peers,   mempool);
  BlockChainServiceImpl block_svc(mempool, chain, hb_table, election_state, addr);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&file_svc);
  builder.RegisterService(&block_svc);

  auto server = builder.BuildAndStart();
  std::cout << "Server listening on " << addr << std::endl;

  // Block‐proposal scheduler
  BlockScheduler scheduler(
    mempool,
    chain,
    file_svc.getGossipStubs(),
    cfg,
    [&]{ return election_state.getLeader() == addr; }
  );
  scheduler.start();

  HeartbeatManager hb_mgr(
    peers, addr, election_state, mempool, chain, hb_table
  );
  hb_mgr.start();

  ElectionManager election_mgr(
    peers, addr, hb_table, election_state, mempool, chain
  );
  election_mgr.start();

  server->Wait();
  scheduler.stop();
  hb_mgr.stop();
  election_mgr.stop();

  return 0;
}
