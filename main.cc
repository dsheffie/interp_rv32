#include <cstdio>
#include <iostream>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <cxxabi.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cassert>
#include <map>
#include <fstream>
#include <boost/program_options.hpp>

#include "loadelf.hh"
#include "helper.hh"
#include "disassemble.hh"
#include "interpret.hh"
#include "saveState.hh"
#include "globals.hh"

extern const char* githash;
uint32_t globals::tohost_addr = 0;
uint32_t globals::fromhost_addr = 0;
bool globals::log = false;
std::map<std::string, uint32_t> globals::symtab;
char **globals::sysArgv = nullptr;
int globals::sysArgc = 0;
bool globals::silent = true;
std::vector<rvthr> globals::threads;

static state_t *s = nullptr;


void *thr_worker(void *x) {
  rvthr *t = reinterpret_cast<rvthr*>(x);
  volatile int *run = reinterpret_cast<volatile int*>(&(t->run));
  while(*run == 0) {}
  pthread_mutex_lock(t->thr_state->mm->mtx);
  printf("thread %d started\n", t->tid);
  pthread_mutex_unlock(t->thr_state->mm->mtx);  
  runRiscv(t->thr_state);
  pthread_mutex_lock(t->thr_state->mm->mtx);
  printf("thread %d done\n", t->tid);
  pthread_mutex_unlock(t->thr_state->mm->mtx);  
  return nullptr;
}


static int buildArgcArgv(const char *filename, const std::string &sysArgs, char **&argv){
  int cnt = 0;
  std::vector<std::string> args;
  char **largs = 0;
  args.push_back(std::string(filename));

  char *ptr = nullptr;
  char *c_str = strdup(sysArgs.c_str());
  if(sysArgs.size() != 0)
    ptr = strtok(c_str, " ");

  while(ptr && (cnt<MARGS)) {
    args.push_back(std::string(ptr));
    ptr = strtok(nullptr, " ");
    cnt++;
  }
  largs = new char*[args.size()];
  for(size_t i = 0; i < args.size(); i++) {
    const std::string & s = args[i];
    size_t l = strlen(s.c_str());
    largs[i] = new char[l+1];
    memset(largs[i],0,sizeof(char)*(l+1));
    memcpy(largs[i],s.c_str(),sizeof(char)*l);
  }
  argv = largs;
  free(c_str);
  return (int)args.size();
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options; 
  size_t pgSize = getpagesize();
  std::string sysArgs, filename;
  uint64_t maxinsns = ~(0UL), dumpIcnt = ~(0UL);
  bool hash = false;
  pthread_mutex_t mtx;
  int nthr = 2;
  pthread_t *threads;
  try {
    po::options_description desc("Options");
    desc.add_options() 
      ("help", "Print help messages") 
      ("args,a", po::value<std::string>(&sysArgs), "arguments to rv32 binary") 
      ("hash,h", po::value<bool>(&hash)->default_value(false), "hash memory at end of execution")
      ("file,f", po::value<std::string>(&filename), "rv32 binary")
      ("maxicnt,m", po::value<uint64_t>(&maxinsns)->default_value(~(0UL)), "max instructions to execute")
      ("dumpicnt", po::value<uint64_t>(&dumpIcnt)->default_value(~(0UL)), "dump after n instructions")
      ("silent,s", po::value<bool>(&globals::silent)->default_value(true), "no interpret messages")
      ("nthr,n", po::value<int>(&nthr)->default_value(1), "num threads")
      ("log,l", po::value<bool>(&globals::log)->default_value(false), "log instructions")
      ; 
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 
  }
  catch(po::error &e) {
    std::cerr << KRED << "command-line error : " << e.what() << KNRM << "\n";
    return -1;
  }

  
  if(not(globals::silent)) {
    std::cerr << KGRN
	      << "RV32 INTERP : built "
	      << __DATE__ << " " << __TIME__
	      << ",pid="<< getpid() << "\n"
	      << "git hash=" << githash
	      << KNRM << "\n";
  }
  
  if(filename.size()==0) {
    std::cerr << argv[0] << ": no file\n";
    return -1;
  }

  /* Build argc and argv */
  globals::sysArgc = buildArgcArgv(filename.c_str(),sysArgs,globals::sysArgv);

  int rc = posix_memalign((void**)&s, pgSize, pgSize); 
  initState(s);
  s->maxicnt = maxinsns;
#ifdef __linux__
  void* mempt = mmap(nullptr, 1UL<<32, PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
#else
  void* mempt = mmap(nullptr, 1UL<<32, PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);
#endif
  assert(mempt != reinterpret_cast<void*>(-1));
  assert(madvise(mempt, 1UL<<32, MADV_DONTNEED)==0);
  s->mem = reinterpret_cast<uint8_t*>(mempt);
  
  if(s->mem == nullptr) {
    std::cerr << "INTERP : couldn't allocate backing memory!\n";
    exit(-1);
  }
  pthread_mutex_init(&mtx, nullptr);  
  
  load_elf(filename.c_str(), s);
  initCapstone();

  threads = new pthread_t[nthr];

  globals::threads.resize(nthr);
  for(int i = 0; i < nthr; i++) {
    globals::threads[i].tid = i;
    globals::threads[i].run = (i == 0);
    globals::threads[i].thr_state = new state_t;
    globals::threads[i].thr_state->mem = s->mem;
    globals::threads[i].thr_state->mm = new tso_mem(&mtx, s->mem, i+1);
    globals::threads[i].thr_state->pc = s->pc;
    for(int j = 0; j < 32; j++) {
      globals::threads[i].thr_state->gpr[j] = s->gpr[j];
    }
    globals::threads[i].thr_state->tid = i;
    globals::threads[i].thr_state->maxicnt = s->maxicnt;
  }
  for(int i = 0; i < nthr; i++) {
    pthread_create(&threads[i], nullptr, thr_worker, &globals::threads[i]);
  }

  for(int i = 0; i < nthr; i++) {
    pthread_join(threads[i], nullptr);
  }
  


  
  munmap(mempt, 1UL<<32);
  if(globals::sysArgv) {
    for(int i = 0; i < globals::sysArgc; i++) {
      delete [] globals::sysArgv[i];
    }
    delete [] globals::sysArgv;
  }
  free(s);
  stopCapstone();
  pthread_mutex_destroy(&mtx);
  return 0;
}


