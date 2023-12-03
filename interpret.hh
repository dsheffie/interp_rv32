#ifndef __INTERPRET_HH__
#define __INTERPRET_HH__

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <ostream>
#include <iostream>
#include <map>
#include <string>
#include <cassert>
#include <pthread.h>
#include <list>

#define MARGS 20

enum class mem_op_type { LB, LH, LW, LBU, LHU, SB, SH, SW};
enum class match_type {no_match, partial_match, sub_match, full_match};

struct store_txn {
  uint32_t addr;
  uint32_t sz;
  uint32_t data;
  store_txn(uint32_t addr, uint32_t sz, uint32_t data) : addr(addr), sz(sz), data(data) {}

  match_type should_forward(uint32_t ld_addr, uint32_t ld_sz) {
    uint32_t l = ld_addr + ld_sz;
    uint32_t s = addr + sz;
    if((ld_addr == addr) and (ld_sz == sz)) {
      return match_type::full_match;
    }
    else if(ld_addr >= addr and (l <= s)) {
      return match_type::sub_match;
    }
    else if((l <= addr) or (s <= ld_addr))
      return match_type::no_match;
    return match_type::partial_match;
  }
};

struct store_queue {
  uint32_t sz;
  uint8_t *mem;
  pthread_mutex_t * m_mtx;
  std::list<store_txn> q;
  uint32_t rx;
  store_queue(uint32_t lg_sz, uint8_t *mem, pthread_mutex_t *m_mtx, uint32_t rx) :
    sz(1U<<lg_sz), mem(mem), m_mtx(m_mtx), rx(rx) {
  }
  ~store_queue() {}

  void update_rng() {
    rx ^= rx << 13;
    rx ^= rx >> 17;
    rx ^= rx << 5;
  }
  
  void drain();
  void pop_one();
  void store(uint32_t ea, uint32_t sz, uint32_t data);
  int32_t load(uint32_t ea, uint32_t sz, bool u = false);

};

static const std::map<mem_op_type,uint32_t> sz_map = {
  {mem_op_type::SB,1},
  {mem_op_type::SH,2},
  {mem_op_type::SW,4},
  {mem_op_type::LB,1},
  {mem_op_type::LBU,1},  
  {mem_op_type::LH,2},
  {mem_op_type::LHU,2},  
  {mem_op_type::LW,4}  
};

/* ha - really sc_mem */
struct tso_mem {
  pthread_mutex_t *mtx;
  uint8_t *mem;
  uint32_t tid;
  store_queue *st;
  
  
  tso_mem(pthread_mutex_t *mtx, uint8_t *mem, uint32_t tid) :
    mtx(mtx), mem(mem), tid(tid) {
    st = new store_queue(4, mem, mtx, tid+1);
  }
  ~tso_mem() {
    delete st;
  }
  void store(mem_op_type ty, uint32_t ea, uint32_t data);
  int32_t load(mem_op_type ty, uint32_t ea);
  
#if 0
  void store(mem_op_type ty, uint32_t ea, uint32_t data) {
    pthread_mutex_lock(mtx);
    switch(ty)
      {
      case mem_op_type::SB:
	mem[ea] = *reinterpret_cast<uint8_t*>(&data);	
	break;
      case mem_op_type::SH:
	*(reinterpret_cast<uint16_t*>(mem + ea)) = *reinterpret_cast<uint16_t*>(&data);	
	break;
      case mem_op_type::SW:
	*(reinterpret_cast<uint32_t*>(mem + ea)) = *reinterpret_cast<uint32_t*>(&data);		
	break;
      default:
	assert(0);
      }
    pthread_mutex_unlock(mtx);    
  }

  int32_t load(mem_op_type ty, uint32_t ea) {
    int32_t r = 0;
    pthread_mutex_lock(mtx);
    switch(ty)
      {
      case mem_op_type::LB:
	r = static_cast<int32_t>(*(reinterpret_cast<int8_t*>(mem + ea)));
	break;
      case mem_op_type::LH:
	r = static_cast<int32_t>(*(reinterpret_cast<int16_t*>(mem + ea)));
	break;
      case mem_op_type::LW:
	r = *reinterpret_cast<int32_t*>(mem + ea);
	break;
      case mem_op_type::LBU: {
	uint32_t b = mem[ea];
	r = *reinterpret_cast<int32_t*>(&b);
	break;
      }
      case mem_op_type::LHU: {
	uint32_t b = *reinterpret_cast<uint16_t*>(mem + ea);
	r = *reinterpret_cast<int32_t*>(&b);
	break;
      }
      default:
	assert(0);
      }
    pthread_mutex_unlock(mtx);        
    return r;
  }
#endif  
};


struct state_t{
  uint32_t pc;
  uint32_t last_pc;
  int32_t gpr[32];
  uint8_t *mem;
  tso_mem *mm;
  uint8_t brk;
  uint8_t bad_addr;
  uint32_t epc;
  uint64_t maxicnt;
  uint64_t icnt;
  int tid;
};



void handle_syscall(state_t *s, uint64_t tohost);

static inline int32_t checksum_gprs(const state_t *s) {
  int32_t h = 0;
  for(int i = 0; i < 32; i++) {
    h ^= s->gpr[i];
  }
  return h;
}


struct utype_t {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t imm : 20;
};

struct branch_t {
  uint32_t opcode : 7;
  uint32_t imm11 : 1; //8
  uint32_t imm4_1 : 4; //12
  uint32_t sel: 3; //15
  uint32_t rs1 : 5; //20
  uint32_t rs2 : 5; //25
  uint32_t imm10_5 : 6; //31
  uint32_t imm12 : 1; //32
};

struct jal_t {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t imm19_12 : 8;
  uint32_t imm11 : 1;
  uint32_t imm10_1 : 10;
  uint32_t imm20 : 1;
};

struct jalr_t {
  uint32_t opcode : 7;
  uint32_t rd : 5; //12
  uint32_t mbz : 3; //15
  uint32_t rs1 : 5; //20
  uint32_t imm11_0 : 12; //32
};

struct rtype_t {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t sel: 3;
  uint32_t rs1 : 5;
  uint32_t rs2 : 5;
  uint32_t special : 7;
};

struct itype_t {
  uint32_t opcode : 7;
  uint32_t rd : 5;
  uint32_t sel : 3;
  uint32_t rs1 : 5;
  uint32_t imm : 12;
};

struct store_t {
  uint32_t opcode : 7;
  uint32_t imm4_0 : 5; //12
  uint32_t sel : 3; //15
  uint32_t rs1 : 5; //20
  uint32_t rs2 : 5; //25
  uint32_t imm11_5 : 7; //32
};

struct load_t {
  uint32_t opcode : 7;
  uint32_t rd : 5; //12
  uint32_t sel : 3; //15
  uint32_t rs1 : 5; //20
  uint32_t imm11_0 : 12; //32
};

union riscv_t {
  rtype_t r;
  itype_t i;
  utype_t u;
  jal_t j;
  jalr_t jj;
  branch_t b;
  store_t s;
  load_t l;
  uint32_t raw;
  riscv_t(uint32_t x) : raw(x) {}
};

void initState(state_t *s);
void runRiscv(state_t *s);

/* stolen from libgloss-htif : syscall.h */
#define SYS_getcwd 17
#define SYS_fcntl 25
#define SYS_mkdirat 34
#define SYS_unlinkat 35
#define SYS_linkat 37
#define SYS_renameat 38
#define SYS_ftruncate 46
#define SYS_faccessat 48
#define SYS_chdir 49
#define SYS_open   55
#define SYS_openat 56
#define SYS_close 57
#define SYS_lseek 62
#define SYS_read 63
#define SYS_write 64
#define SYS_pread 67
#define SYS_pwrite 68
#define SYS_stat 78
#define SYS_fstatat 79
#define SYS_fstat 80
#define SYS_exit 93
#define SYS_gettimeofday 94
#define SYS_lstat 1039
#define SYS_getmainvars 2011

std::ostream &operator<<(std::ostream &out, const state_t & s);

struct stat32_t {
  uint16_t st_dev;
  uint16_t st_ino;
  uint32_t st_mode;
  uint16_t st_nlink;
  uint16_t st_uid;
  uint16_t st_gid;
  uint16_t st_rdev;
  uint32_t st_size;
  uint32_t _st_atime;
  uint32_t st_spare1;
  uint32_t _st_mtime;
  uint32_t st_spare2;
  uint32_t _st_ctime;
  uint32_t st_spare3;
  uint32_t st_blksize;
  uint32_t st_blocks;
  uint32_t st_spare4[2];
};


#endif
