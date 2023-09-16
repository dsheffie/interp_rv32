#include <list>
#include "interpret.hh"

#ifndef __LOAD_ELF_H__
#define __LOAD_ELF_H__

void load_elf(const char* fn, state_t *ms);
void load_binary(uint32_t addr, const char* fn, state_t *ms);

#endif 

