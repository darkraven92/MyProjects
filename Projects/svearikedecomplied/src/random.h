#ifndef SVEA_RANDOM_H
#define SVEA_RANDOM_H
#include <stdint.h>
typedef struct { uint32_t state, calls; } SrRandom;
void sr_random_seed(SrRandom *rng,uint32_t seed);
/* Windows SVEA95.EXE random(n), including its n<=0 behavior. */
int32_t sr_random_next(SrRandom *rng,int32_t bound);
#endif
