#include "branchtracker.hh"
#include "globals.hh"

#include <iostream>

void branchtracker::update(uint32_t pc, bool taken) {
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
  uint64_t dyn_branches = 0;
  double acc = 0.0;
  for(auto pp : bmap) {
    const branch *b = pp.second;
    uint64_t b_dyn_instances = 0;
    uint64_t b_dyn_t = 0, b_dyn_nt = 0;
    
    double br_prel = 0.0;
    if(b->t_patterns.empty())
      continue;

    {
      uint64_t t_count = 0, nt_count = 0;
      for(auto &p : b->t_patterns) {
	t_count += p.second;
      }
      for(auto &p : b->nt_patterns) {
	nt_count += p.second;
      }    
      assert(t_count == b->tcount);
      assert(nt_count == b->ntcount);
    }


    for(auto &p : b->t_patterns) {
      const globalhist_t &pat = p.first;
      double t_count = static_cast<double>(p.second);
      dyn_branches += p.second;
      b_dyn_t += p.second;
      b_dyn_instances += p.second;
      double nt_count = 0.0;
      assert(p.second != 0);


      auto it = b->nt_patterns.find(pat);
      if(it != b->nt_patterns.end()) {
	assert(it->first == pat);
	nt_count = static_cast<double>(it->second);
	dyn_branches += it->second;
	b_dyn_instances += it->second;
	b_dyn_nt += it->second;
      }
      assert(b_dyn_instances <= b->count);
      
      double tc = t_count + nt_count;
      double pr = t_count / tc;
      double el = 2 * std::min(pr, 1.0 - pr);
      double prel = pr*el;
      std::cout << std::hex << pat.to_ulong() << std::dec << "\n";
      std::cout << "tc = " << t_count << "\n";
      std::cout << "ntc = " << nt_count << "\n";
      std::cout << "sum = " << (t_count + nt_count) << "\n";
      
      //std::cout << "pr = " << pr << "\n";
      //std::cout << "el = " << el << "\n";
      //std::cout << "prel = " << prel << "\n";
      br_prel += prel;
    }
    
    if(b->count != b_dyn_instances) {
      std::cout << "b->count = " << b->count << "\n";
      std::cout << "b_dyn_nt = " << b_dyn_nt << "\n";
      std::cout << "b_dyn_t  = " << b_dyn_t << "\n";

      std::cout << "b_dyn_nt = " << (b_dyn_nt == b->ntcount) << "\n";
      std::cout << "b_dyn_t  = " << (b_dyn_t == b->tcount) << "\n";

      std::cout << "b_dyn_instances = " << b_dyn_instances << "\n";
    }
    assert(b->count == b_dyn_instances);
    assert(b_dyn_instances);
    std::cout << std::hex
	      << pp.first
	      << " : "
	      << std::dec
	      << b_dyn_instances
	      << " "
	      << br_prel / static_cast<double>(b_dyn_instances)
	      << "\n";
    acc += br_prel;
  }
  acc /= static_cast<double>(dyn_branches);
  std::cout << "acc = " << acc << "\n";
}
