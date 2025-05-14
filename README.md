# Audit-Vault

A C++17 blockchain-style audit framework for file access, featuring:

- **gRPC-based** client/server and peer-to-peer gossip protocols
- **RSA digital signatures** for end-to-end integrity and non-repudiation
- **Merkle-tree** block integrity and cryptographic hashing
- **Leader election** and **heartbeat** for high-availability consensus
- **On-demand block retrieval** (`GetBlock`) for new or recovering nodes
- Persistent **mempool**, **chain.json**, and per-block JSON files

---

## 📦 Features

1. **Submit & Gossip Audits**  
   Clients submit `FileAudit` requests over gRPC; servers persist them to a mempool and gossip to peers.

2. **Batch Block Proposal**  
   The leader periodically collects pending audits, forms a block, computes a Merkle root, and broadcasts a `ProposeBlock` message.

3. **Voting & Commit**  
   Peers verify each proposal (Merkle root, previous-hash, audit signatures), vote, and upon majority, commit the block (updating `chain.json`, pruning the mempool, and writing `blocks/block_<id>.json`).

4. **Leader Heartbeats**  
   Nodes exchange heartbeats every 2 s, tracking each other’s latest block IDs and mempool sizes, and marking peers dead after a 4 s timeout.

5. **Leader Election**  
   When no leader is known or the leader dies, a node triggers an election via `TriggerElection`/`NotifyLeadership` RPCs, comparing its own metrics (block ID, mempool size, address) against the candidate to vote.

6. **Block Synchronization**  
   Recovering nodes can request missing blocks (`GetBlock`) from peers, ensuring they catch up before accepting new proposals.

## 🛠 Prerequisites

- **C++17** compiler (e.g. `gcc` ≥ 9, `clang` ≥ 11)
- [CMake](https://cmake.org/) ≥ 3.15
- [gRPC](https://grpc.io/) & [Protocol Buffers](https://developers.google.com/protocol-buffers)
- [OpenSSL](https://www.openssl.org/) (for RSA signing/verification)
- [nlohmann/json](https://github.com/nlohmann/json) (header-only)

## File Structure

.
├── CMakeLists.txt
├── README.md
├── peers.json # List of peer addresses
├── leader.json # Initial leader, batch size & interval
├── keys/ # RSA keypair for clients
├── proto/ # .proto definitions
├── include/ # Public headers
├── src/ # Implementation (.cpp) files
├── blocks/ # Generated per-block JSON files
├── mempool.dat # Persisted mempool
└── chain.json # Persisted blockchain metadata

## Building

```bash
chmod +x starter.sh build.sh run.sh
./starter.sh
./build.sh
```

## Running

```bash
./run.sh
```

If you want to run the server with a different address run:

```bash
cd build
./node_server 0.0.0.0:<port_number>
```

For running the client:

```bash
cd build
./client
```

## Configuration

peer.json:

Put address of all the peer nodes in the array

leader.json:

The leader_addr field is redundant, you can use the batch size, batch_interval_s to determine when to trigger a block creation and proposal.
