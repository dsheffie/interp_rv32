#ifndef __NAC_HH__
#define __NAC_HH__

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <cassert>

class nway_cache {
private:
  static const uint32_t CL_LEN = 4;
  static const uint32_t MASK = ~((1U<<CL_LEN)-1);
  struct entry {
    entry *next;
    entry *prev;
    uint32_t addr;
  };

  struct way {
    size_t ways;    
    entry *entries;
    entry *freelist;  
    entry *lrulist;
    
    way(size_t ways) : entries(nullptr), freelist(nullptr), lrulist(nullptr), ways(ways) {
      entries = new entry[ways];
      memset(entries,0,sizeof(entry)*ways);
      
      entry *c = freelist = &entries[0];
      for(size_t i = 1; i < ways; i++) {
	c->next = &entries[i];
	entries[i].prev = c;
	c = &entries[i];
      }
    }
    ~way() {
      delete [] entries;
    }
    bool access(uint32_t ea) {    
      bool found = false;
      entry *p = lrulist, *l = nullptr;
      while(p) {
	if(p->addr == ea) {
	  found = true;
	  break;
	}
	l = p;
	p = p->next;
      }
      if(not(found)) {
	if(freelist != nullptr) {
	  l = freelist;
	  l->addr = ea;
	  freelist = freelist->next;
	  l->prev = nullptr;
	  l->next = nullptr;
	  if(lrulist == nullptr) {
	    lrulist = l;
	  }
	  else {
	    lrulist->prev = l;
	    l->next = lrulist;
	    lrulist = l;
	  }
	}
	else {
	  assert(l);
	  assert(l->next == nullptr);	
	  /* remove this node */
	  l->prev->next = nullptr;
	  l->prev = nullptr;
	  lrulist->prev = l;
	  l->next = lrulist;
	  lrulist = l;
	  l->addr = ea;
	}
      }
      else {
	if(p != lrulist) {
	  if(p->next) {
	    p->next->prev = p->prev;
	  }
	  if(p->prev) {
	    p->prev->next = p->next;
	  }
	  p->prev = nullptr;
	  p->next = lrulist;
	  lrulist->prev = p;
	  lrulist = p;
	  //print(lrulist);
	}
      }
      return found;
    }
    
    
  };
  
  size_t nways,lg2_lines;
  way **ways;

  uint64_t hits, accesses;
  
public:
  nway_cache(size_t nways, size_t lg2_lines) :
    nways(nways), lg2_lines(lg2_lines), hits(0), accesses(0) {
    ways = new way*[(1UL<<lg2_lines)];
    for(size_t l = 0; l < (1UL<<lg2_lines); l++) {
      ways[l] = new way(nways);
    }
  }
  ~nway_cache() {
    delete [] ways;
  }
  uint64_t get_hits() const {
    return hits;
  }
  uint64_t get_accesses() const {
    return accesses;
  }
  uint64_t get_size() const {
    return (1UL<<lg2_lines) * nways * ((~MASK)+1);
  }
  void access(uint32_t ea) {
    ea &= MASK;
    uint32_t idx = (ea >> CL_LEN) & ((1U<<lg2_lines)-1);
    accesses++;
    bool h = ways[idx]->access(ea);
    if(h) {
      hits++;
    }
  }
};


#endif
