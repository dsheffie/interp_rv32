#ifndef __INTERPRET_HH__
#define __INTERPRET_HH__

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <ostream>
#include <map>
#include <string>

#define MARGS 20

struct state_t{
  uint32_t pc;
  uint32_t last_pc;
  int32_t gpr[32];
  uint8_t *mem;
  uint8_t brk;
  uint8_t bad_addr;
  uint32_t csr[64];
  uint64_t maxicnt;
  uint64_t icnt;
};

enum class riscv_csr {
  fflags,
  frm,
  fcsr,
  cycle,
  time,
  instret,
  cycleh,
  timeh,
  instreth,
  sstatus,
  sie,
  stvec,
  scounteren,
  senvcfg,
  sscratch,
  sepc,
  scause,
  stval,
  sip,
  satp,
  scontext,
  mvendorid,
  marchid,
  mimpid,
  mhartid,
  mconfigptr,
  mstatus,
  misa,
  medeleg,
  mideleg,
  mie,
  mtvec,
  mcounteren,
  mscratch,
  mepc,
  mcause
};

static const std::map<uint32_t, riscv_csr> csr_enum_map = {
  {0x001, riscv_csr::fflags},
  {0x002, riscv_csr::frm},
  {0x003, riscv_csr::fcsr},    
  {0xc00, riscv_csr::cycle},
  {0xc01, riscv_csr::time},
  {0xc02, riscv_csr::instret},
  {0xc80, riscv_csr::cycleh},  
  {0xc81, riscv_csr::timeh},
  {0xc82, riscv_csr::instreth},      
  {0x100, riscv_csr::sstatus},
  {0x104, riscv_csr::sie},
  {0x105, riscv_csr::stvec},
  {0x106, riscv_csr::scounteren},
  {0x10a, riscv_csr::senvcfg},
  {0x140, riscv_csr::sscratch},
  {0x141, riscv_csr::sepc},
  {0x142, riscv_csr::scause},
  {0x143, riscv_csr::stval},
  {0x144, riscv_csr::sip},
  {0x180, riscv_csr::satp},
  {0x5a8, riscv_csr::scontext},
  {0xf11, riscv_csr::mvendorid},
  {0xf12, riscv_csr::marchid},
  {0xf13, riscv_csr::mimpid},
  {0xf14, riscv_csr::mhartid},
  {0xf15, riscv_csr::mconfigptr},
  {0x300, riscv_csr::mstatus},
  {0x301, riscv_csr::misa},  
  {0x302, riscv_csr::medeleg},
  {0x303, riscv_csr::mideleg},
  {0x304, riscv_csr::mie},
  {0x305, riscv_csr::mtvec},
  {0x306, riscv_csr::mcounteren},      
  {0x340, riscv_csr::mscratch},    
};

inline static int get_csr_idx(riscv_csr e)
{
  switch(e)
    {
    case riscv_csr::fflags:
      return 0;
    case riscv_csr::frm:
      return 1;
    case riscv_csr::fcsr:
      return 2;
    case riscv_csr::cycle:
      return 3;
    case riscv_csr::time:
      return 4;
    case riscv_csr::instret:
      return 5;
    case riscv_csr::cycleh:
      return 6;
    case riscv_csr::timeh:
      return 7;
    case riscv_csr::instreth:
      return 8;
    case riscv_csr::sstatus:
      return 9;
    case riscv_csr::sie:
      return 10;
    case riscv_csr::stvec:
      return 11;
    case riscv_csr::scounteren:
      return 12;
    case riscv_csr::senvcfg:
      return 13;
    case riscv_csr::sscratch:
      return 14;
    case riscv_csr::sepc:
      return 15;
    case riscv_csr::scause:
      return 16;
    case riscv_csr::stval:
      return 17;
    case riscv_csr::sip:
      return 18;
    case riscv_csr::satp:
      return 19;
    case riscv_csr::scontext:
      return 20;
    case riscv_csr::mvendorid:
      return 21;
    case riscv_csr::marchid:
      return 22;
    case riscv_csr::mimpid:
      return 23;
    case riscv_csr::mhartid:
      return 24;
    case riscv_csr::mconfigptr:
      return 25;
    case riscv_csr::mstatus:
      return 26;
    case riscv_csr::misa:
      return 27;
    case riscv_csr::medeleg:
      return 28;
    case riscv_csr::mideleg:
      return 29;
    case riscv_csr::mie:
      return 30;
    case riscv_csr::mtvec:      
      return 31;
    case riscv_csr::mcounteren:
      return 32;
    case riscv_csr::mscratch:
      return 33;
    default:
      break;
    }
  assert(false);
  return -1;
}


uint32_t va2pa(state_t *s, uint32_t va, bool &fault);


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
void runRiscv(state_t *s, uint64_t dumpIcnt);

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
