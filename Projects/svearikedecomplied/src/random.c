#include "random.h"
void sr_random_seed(SrRandom *rng,uint32_t seed) {
    if(rng) { rng->state=seed; rng->calls=0; }
}
int32_t sr_random_next(SrRandom *rng,int32_t bound) {
    if(!rng) return 0;
    /* Directly reconstructed from SVEA95.EXE 0x5010d1 and 0x44e770.
       Each Lingo call combines TWO 15-bit CRT outputs, then applies modulo.
       Unsigned arithmetic explicitly preserves x86's 32-bit wraparound. */
    uint32_t value=0;
    for(int i=0;i<2;++i) {
        rng->state=rng->state*UINT32_C(214013)+UINT32_C(2531011);
        value=(value<<15)|((rng->state>>16)&UINT32_C(32767));
    }
    ++rng->calls;
    if(bound>0) value%=(uint32_t)bound;
    return (int32_t)(value+1);
}
