#pragma once
#include <string>
#include <vector>

/// Compute the SHA-256 hash of a byte string, returning a hex digest.
std::string SHA256Hex(const std::string& data);

/// Given a list of leaf hashes (hex strings), build the Merkle tree
/// (duplicating the last leaf if odd) and return the root (hex).
std::string ComputeMerkleRoot(const std::vector<std::string>& leaf_hashes);