#ifndef __FAC_HH__
#define __FAC_HH__

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <cassert>

class fully_assoc_cache {
private:
  static const uint32_t MASK = ~15U;
  struct entry {
    entry *next;
    entry *prev;
    uint32_t addr;
  };
  size_t lines;
  entry *entries;
  entry *freelist;  
  entry *lrulist;

  uint64_t hits, accesses;
  
  void print(entry *p) {
    std::cout << std::hex << p->addr << std::dec << "\n";
    std::cout << "next = " << p->next << "\n";
    std::cout << "prev = " << p->prev << "\n";
    if(p->next) {
      print(p->next);
    }
  }
  
public:
  fully_assoc_cache(size_t lines) :
    lines(lines), entries(nullptr),
    freelist(nullptr), lrulist(nullptr),
    hits(0), accesses(0) {
    entries = new entry[lines];
    memset(entries,0,sizeof(entry)*lines);
    
    entry *c = freelist = &entries[0];
    for(size_t i = 1; i < lines; i++) {
      c->next = &entries[i];
      entries[i].prev = c;
      c = &entries[i];
    }
  }
  ~fully_assoc_cache() {
    delete [] entries;
  }
  uint64_t get_hits() const {
    return hits;
  }
  uint64_t get_accesses() const {
    return accesses;
  }
  uint64_t get_size() const {
    return lines * ((~MASK)+1);
  }
  void access(uint32_t ea) {
    bool found = false;
    entry *p = lrulist, *l = nullptr;
    ++accesses;
    ea &= MASK;

    while(p) {
      if(p->addr == ea) {
	found = true;
	break;
      }
      l = p;
      p = p->next;
    }

    //std::cout << "found " << found << " ea " << std::hex << ea << std::dec << "\n";
    
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
      ++hits;
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
  }
  

};


#endif
