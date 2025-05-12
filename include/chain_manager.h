#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

/// Minimal metadata for each block.
struct BlockMeta {
  int64_t    id;
  std::string hash;
  std::string previous_hash;
  std::string merkle_root;
};

/// Manages chain.json on disk + in-memory view.
class ChainManager {
public:
  /// Construct the manager over the given file path.
  explicit ChainManager(std::string path);

  /// Latest block ID (0 if none).
  int64_t getLastID() const;

  /// Latest block hash ("" if none).
  std::string getLastHash() const;

  /// Latest block merkle_root ("" if none).
  std::string getLastMerkleRoot() const;

  /// All blocks in chain order.
  std::vector<BlockMeta> getAll() const;

  /// Append a new block (and rewrite file).
  void append(const BlockMeta& meta);

private:
  void loadFromDisk();
  void writeToDisk() const;

  std::string         path_;
  mutable std::mutex  mu_;
  std::vector<BlockMeta> blocks_;
};
