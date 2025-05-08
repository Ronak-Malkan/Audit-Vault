#pragma once

#include <string>
#include <vector>

// Reads a JSON array of strings from `path`
// and returns a vector of peer endpoints ("host:port").
std::vector<std::string> LoadPeers(const std::string& path);
