#include "random.h"
#include "projector_rng_vectors.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    SrRandom rng;
    for(unsigned i=0;i<sizeof rng_vectors/sizeof rng_vectors[0];++i) {
        sr_random_seed(&rng,rng_vectors[i].seed);
        CHECK((uint32_t)sr_random_next(&rng,rng_vectors[i].bound)==rng_vectors[i].value);
        CHECK(rng.state==rng_vectors[i].state && rng.calls==1);
    }
    sr_random_seed(&rng,1);
    for(int i=1;i<=10000;++i) {
        int value=sr_random_next(&rng,15);
        CHECK(value>=1 && value<=15 && rng.calls==(uint32_t)i);
    }
    CHECK(sr_random_next(0,15)==0);
    puts("60 golden vectors match native execution of the original Windows RNG instructions.");
    return 0;
}
