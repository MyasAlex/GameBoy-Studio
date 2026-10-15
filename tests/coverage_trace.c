#include <stdint.h>
static uint8_t slots[65536];
static unsigned count,novel;
/* This translation unit is compiled without coverage instrumentation. */
void __sanitizer_cov_trace_pc(void){uintptr_t pc=(uintptr_t)__builtin_return_address(0);unsigned index=(unsigned)((pc>>4)^(pc>>16))&65535;if(!slots[index]){slots[index]=1;++count;novel=1;}}
void gb_cov_begin(void){novel=0;}
unsigned gb_cov_novel(void){return novel;}
unsigned gb_cov_count(void){return count;}
