#include "branchtracker.hh"
#include "globals.hh"

#include <iostream>

void branchtracker::update(uint32_t pc, bool taken) {
  ++dyn_branches;
  auto it = bmap.find(pc);
  if(it == bmap.end()) {
    bmap[pc] = new branch();
  }
  branch *b = bmap.at(pc);
  b->count++;
  
  if(taken) {
    b->t_patterns[globals::H]++;
    b->tcount++;
  }
  else {
    b->nt_patterns[globals::H]++;
    b->ntcount++;
  }
  //b->sanity_check();

}

branchtracker::~branchtracker() {
  for(auto &p : bmap) {
    delete p.second;
  }
}

void branchtracker::report() {
  std::cout << bmap.size() << " static branches\n";
  double acc = 0.0;
  for(auto pp : bmap) {
    const branch *b = pp.second;
    uint64_t b_dyn_t = 0, b_dyn_nt = 0;
    
    double br_prel = 0.0;
    if(b->t_patterns.empty())
      continue;



    for(auto &p : b->t_patterns) {
      const globalhist_t &pat = p.first;
      double t_count = static_cast<double>(p.second);
      b_dyn_t += p.second;
      double nt_count = 0.0;
      assert(p.second != 0);


      auto it = b->nt_patterns.find(pat);
      if(it != b->nt_patterns.end()) {
	assert(it->first == pat);
	nt_count = static_cast<double>(it->second);
	b_dyn_nt += it->second;
      }
      double tc = t_count + nt_count;
      double pr = t_count / tc;
      double el = 2 * std::min(pr, 1.0 - pr);
      
      double prel = (t_count + nt_count)*el;
      
      br_prel += prel;
    }
    
#if 0    
    std::cout << std::hex
	      << pp.first
	      << " : "
	      << std::dec
	      << b->count
	      << " "
	      << br_prel / static_cast<double>(b->count)
	      << "\n";
#endif
    acc += br_prel;
  }
  acc /= static_cast<double>(dyn_branches);
  std::cout << "acc = " << acc << "\n";
}
