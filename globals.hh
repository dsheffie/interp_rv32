#ifndef __GLOBALSH__
#define __GLOBALSH__

#include <vector>

struct rvthr {
  int tid;
  int run;
  state_t *thr_state;
};

namespace globals {
  extern uint32_t tohost_addr;
  extern uint32_t fromhost_addr;
  extern int sysArgc;
  extern char **sysArgv;
  extern bool silent;
  extern bool log;
  extern std::map<std::string, uint32_t> symtab;
  extern std::vector<rvthr> threads;
};

#endif
