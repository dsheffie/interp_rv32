#include "interpret.hh"


void tso_mem::store(mem_op_type ty, uint32_t ea, uint32_t data) {
  ///if(tid == 1) {
  //std::cout << "thread " << tid << " store to " << std::hex << ea << std::dec << "\n";
  //}
  st->store(ea, sz_map.at(ty), data);
}
int32_t tso_mem::load(mem_op_type ty, uint32_t ea) {
  //if(tid == 1) {
  //std::cout << "thread " << tid << " load to " << std::hex << ea << std::dec << "\n";
  //}  
  return st->load(ea, sz_map.at(ty));
}

void store_queue::drain() {
    pthread_mutex_lock(m_mtx);
    while(not(q.empty())) {
      store_txn t = q.front();
      q.pop_front();
      switch(t.sz)
	{
	case 1:
	  mem[t.addr] = *reinterpret_cast<uint8_t*>(&t.data);		  
	  break;
	case 2:
	  *(reinterpret_cast<uint16_t*>(mem + t.addr)) = *reinterpret_cast<uint16_t*>(&t.data);		  
	  break;
	case 4:
	  *(reinterpret_cast<uint32_t*>(mem + t.addr)) = *reinterpret_cast<uint32_t*>(&t.data);			  
	  break;
	default:
	  assert(0);
	}
    }
    pthread_mutex_unlock(m_mtx);      
  }

void store_queue::pop_one() {
  if(not(q.empty())) {
    pthread_mutex_lock(m_mtx);
    store_txn t = q.front();
    q.pop_front();
    switch(t.sz)
      {
      case 1:
	mem[t.addr] = *reinterpret_cast<uint8_t*>(&t.data);		  
	break;
      case 2:
	*(reinterpret_cast<uint16_t*>(mem + t.addr)) = *reinterpret_cast<uint16_t*>(&t.data);		  
	break;
      case 4:
	*(reinterpret_cast<uint32_t*>(mem + t.addr)) = *reinterpret_cast<uint32_t*>(&t.data);			  
	break;
      default:
	  assert(0);
      }
    pthread_mutex_unlock(m_mtx);      
  }
}

void store_queue::store(uint32_t ea, uint32_t sz, uint32_t data) {
    /* if data in store buffer, drain one entry */
    //place store at end of the store buffer
  update_rng();  
  if(rx&1) {
    pop_one();
  }
  q.emplace_back(ea,sz,data);
}

int32_t store_queue::load(uint32_t ea, uint32_t sz, bool u) {
  //check store buffer - from youngest to oldest
  bool must_drain = false;  
  update_rng();
  if(rx & 1) {
    pop_one();
  }  
  for(store_txn &txn : q) {
    match_type m = txn.should_forward(ea, sz);
    //hit in the store buffer
    if(m == match_type::full_match) {
      return *reinterpret_cast<int32_t*>(&txn.data);
    }
    else if(m == match_type::sub_match) {
      std::cout << "sub match\n";
      std::cout << "st.ea = "<< std::hex << txn.addr << "\n";
      std::cout << "st.sz = "<< std::hex << txn.sz << "\n";
      std::cout << "ld.ea = "<< std::hex << ea << "\n";
      std::cout << "ld.sz = "<< std::hex << sz << "\n";	
      must_drain = true;
      break;
    }
    else if(m == match_type::partial_match) {
      std::cout << "partial match\n";
      std::cout << "st.ea = "<< std::hex << txn.addr << "\n";
      std::cout << "st.sz = "<< std::hex << txn.sz << "\n";
      std::cout << "ld.ea = "<< std::hex << ea << "\n";
      std::cout << "ld.sz = "<< std::hex << sz << "\n";
      must_drain = true;
      assert(txn.sz > sz);	
      break;
    }
  }
    
  if(must_drain) {
    drain();
  }
  int32_t l = 0;
  //didn't find value in store buffer, go to memory
  pthread_mutex_lock(m_mtx);
  switch(sz)
    {
    case 1: {
      if(u) {
	uint32_t b = mem[ea];	  
	l = *reinterpret_cast<int32_t*>(&b);
      }
      else {
	l = static_cast<int32_t>(*(reinterpret_cast<int8_t*>(mem + ea)));
      }
      break;
    }
    case 2: {
      if(u) {
	uint32_t b = *reinterpret_cast<uint16_t*>(mem + ea);
	l = *reinterpret_cast<int32_t*>(&b);
      }
      else {
	l = static_cast<int32_t>(*(reinterpret_cast<int16_t*>(mem + ea)));
      }
      break;
    }
    case 4:
      l = *reinterpret_cast<int32_t*>(mem + ea);
      break;
    default:
      assert(0);
    }
  pthread_mutex_unlock(m_mtx);
  return l;
}
