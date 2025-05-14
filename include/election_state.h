#pragma once
#include <cstdint>
#include <string>

/// Holds the current election term, which peer we've voted for, and who the leader is.
struct ElectionState {
  // The current term number; starts at 0.
  int64_t       current_term    = 0;

  // The address (host:port) of the peer we voted for in this term. Empty = none.
  std::string   voted_for       = "";

  // The address of the current leader as we know it. Empty = unknown.
  std::string   current_leader  = "";

  // Getters / setters
  void       setTerm(int64_t term)                { current_term    = term; }
  int64_t    getTerm() const                       { return current_term; }

  void       setVotedFor(const std::string& addr)  { voted_for       = addr; }
  const std::string& getVotedFor() const            { return voted_for; }

  void       setLeader(const std::string& addr)    { current_leader  = addr; }
  const std::string& getLeader() const              { return current_leader; }
};
