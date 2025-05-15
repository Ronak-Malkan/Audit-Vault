#include "merkle_tree.h"
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/coded_stream.h>

static std::string toHex(const unsigned char* buf, size_t len) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (size_t i = 0; i < len; i++)
    out << std::setw(2) << (int)buf[i];
  return out.str();
}

std::string SHA256Hex(const std::string& data) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(data.data()),
         data.size(), hash);
  return toHex(hash, SHA256_DIGEST_LENGTH);
}

/// New: deterministic Serialize
std::string DeterministicSerialize(const google::protobuf::Message& msg) {
  std::string out;
  google::protobuf::io::StringOutputStream sos(&out);
  google::protobuf::io::CodedOutputStream cos(&sos);
  cos.SetSerializationDeterministic(true);
  msg.SerializeWithCachedSizes(&cos);
  cos.Trim();
  return out;
}

std::string ComputeMerkleRoot(const std::vector<std::string>& leaf_hashes) {
  if (leaf_hashes.empty()) return "";
  std::vector<std::string> level = leaf_hashes;
  while (level.size() > 1) {
    std::vector<std::string> next;
    for (size_t i = 0; i < level.size(); i += 2) {
      const std::string& left  = level[i];
      const std::string& right = (i+1 < level.size() ? level[i+1] : left);
      next.push_back(SHA256Hex(left + right));
    }
    level.swap(next);
  }
  return level[0];
}