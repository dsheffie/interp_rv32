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
std::map<std::string, uint32_t> globals::symtab;
char **globals::sysArgv = nullptr;
int globals::sysArgc = 0;
bool globals::enClockFuncts = false;
bool globals::isMipsEL = false;
uint64_t globals::icountMIPS = 500;
bool globals::silent = true;
std::map<uint32_t, uint64_t> globals::execHisto;

static state_t *s =0;

template<typename X, typename Y>
static inline void dump_histo(const std::string &fname,
			      const std::map<X,Y> &histo) {
  if(histo.empty())
    return;
  
  std::vector<std::pair<X,Y>> sorted_by_cnt;
  for(auto &p : histo) {
    sorted_by_cnt.emplace_back(p.second, p.first);
  }
  std::ofstream out(fname);
  std::sort(sorted_by_cnt.begin(), sorted_by_cnt.end());
  for(auto it = sorted_by_cnt.rbegin(), E = sorted_by_cnt.rend(); it != E; ++it) {
    uint32_t pc = it->second;
    uint32_t r_inst = *reinterpret_cast<uint32_t*>(s->mem+pc);
    r_inst = bswap<false>(r_inst);	
    auto s = getAsmString(r_inst, it->second);
    out << std::hex << it->second << ":"
  	      << s << ","
  	      << std::dec << it->first << "\n";
  }
  out.close();
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
  std::string dumpname;
  int64_t dumpIcnt = -1L;
  size_t pgSize = getpagesize();
  std::string sysArgs, filename;
  uint64_t maxinsns = ~(0UL);
  bool hash = false, isDump = false;

  try {
    po::options_description desc("Options");
    desc.add_options() 
      ("help", "Print help messages") 
      ("args,a", po::value<std::string>(&sysArgs), "arguments to mips binary") 
      ("clock,c", po::value<bool>(&globals::enClockFuncts)->default_value(false), "enable wall-clock")
      ("hash,h", po::value<bool>(&hash)->default_value(false), "hash memory at end of execution")
      ("file,f", po::value<std::string>(&filename), "mips binary")
      ("isdump,d", po::value<bool>(&isDump)->default_value(false), "is a dump")
      ("dumpicnt", po::value<int64_t>(&dumpIcnt)->default_value(-1L), "dump after n instructions")
      ("dumpname", po::value<std::string>(&dumpname), "dump file name")
      ("maxicnt,m", po::value<uint64_t>(&maxinsns)->default_value(~(0UL)), "max instructions to execute")
      ("silent,s", po::value<bool>(&globals::silent)->default_value(true), "no interpret messages")
      ("icountMIPS", po::value<uint64_t>(&globals::icountMIPS)->default_value(500), "millions of of instructions per second for time calculation")
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
	      << "MIPS INTERP : built "
	      << __DATE__ << " " << __TIME__
	      << ",pid="<< getpid() << "\n"
	      << "git hash=" << githash
	      << KNRM << "\n";
  }
  
  if(filename.size()==0) {
    std::cerr << "INTERP : no file\n";
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
  
  if(isDump) {
    loadState(*s,filename);
  }
  else {
    load_elf(filename.c_str(), s);
  }
  
  initCapstone();

  double runtime = timestamp();
  
  if(dumpIcnt != -1L) {
    if(dumpname.size() == 0) {
      dumpname = filename;
    }
    while(s->brk==0 and (s->icnt < s->maxicnt)) {
      if( (s->icnt >= dumpIcnt) ) {
	std::stringstream ss;
	ss << dumpname << s->icnt << ".bin";
	if(not(globals::silent)) {
	  std::cout << "dumping at icnt " << s->icnt << "\n";
	}
	dumpState(*s, ss.str());
	s->brk = 1;
      }
      execRiscv(s);
    }
  }
  else {
    while(s->brk==0 and (s->icnt < s->maxicnt)) {
      execRiscv(s);
      if(s->bad_addr) {
	std::cout << "bad address generated for pc "
		  << std::hex
		  << s->epc
		  << std::dec
		  << "!\n";
	break;
      }
    }
  }

  runtime = timestamp()-runtime;
  dump_histo("exec.txt", globals::execHisto);
  
  if(hash) {
    std::fflush(nullptr);
    /* std::cerr << *s << "\n"; */
    std::cerr << "crc32=" << std::hex
	      << crc32(s->mem, 1UL<<32)<<std::dec
	      << "\n";
  }  


  if(not(globals::silent)) {
    std::cerr << KGRN << "INTERP: "
	      << runtime << " sec, "
	      << s->icnt << " ins executed, "
	      << std::round((s->icnt/runtime)*1e-6) << " megains / sec "
	      << KNRM  << "\n";
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

  return 0;
}


