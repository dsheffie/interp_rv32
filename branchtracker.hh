#ifndef __branchtrackerhh__
#define __branchtrackerhh__

#include "globals.hh"
#include <unordered_map>
#include <unordered_set>
#include <map>

struct branch {
  uint64_t count = 0, tcount = 0, ntcount = 0;
  std::unordered_map<globalhist_t, uint64_t> t_patterns, nt_patterns;
  bool sanity_check() const {
    uint64_t t = 0, nt = 0;
    for(auto &p : t_patterns) {
      t += p.second;
    }
    for(auto &p : nt_patterns) {
      nt += p.second;
    }    
    assert(t == tcount);
    assert(nt == ntcount);
    return (t == tcount) and (nt == ntcount);
    
  }
  
};

class branchtracker {
private:
  std::map<uint32_t, branch*> bmap;
public:
  void update(uint32_t pc, bool taken);
  void report();
  ~branchtracker();
};


#endif
