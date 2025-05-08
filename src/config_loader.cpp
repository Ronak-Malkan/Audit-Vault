#include "config_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

// Trim whitespace and JSON punctuation
static std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (std::isspace((unsigned char)s[b]) || s[b]=='\"' || s[b]=='[' || s[b]==']')) ++b;
    while (e > b && (std::isspace((unsigned char)s[e-1]) || s[e-1]=='\"' || s[e-1]=='[' || s[e-1]==']')) --e;
    return s.substr(b, e-b);
}

std::vector<std::string> LoadPeers(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Unable to open peers file: " + path);
    }

    std::ostringstream buf;
    buf << in.rdbuf();
    std::string content = buf.str();

    // Remove newlines for simpler parsing
    content.erase(std::remove(content.begin(), content.end(), '\n'), content.end());

    std::vector<std::string> peers;
    std::stringstream ss(content);
    std::string token;
    while (std::getline(ss, token, ',')) {
        auto p = trim(token);
        if (!p.empty()) {
            peers.push_back(p);
        }
    }
    return peers;
}
