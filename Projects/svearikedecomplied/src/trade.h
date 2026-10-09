#ifndef SVEA_TRADE_H
#define SVEA_TRADE_H
/* Translation of SVEA.DIR MovieScript 2, fixrandomPrice.
 * trigger_roll is random(4); direction_roll is random(3) outside [4,8],
 * random(2) inside. Rolls are ONE based. Return 0 for invalid inputs.
 * This pure step accepts recorded/test rolls; economy.c supplies Windows RNG draws. */
int sr_trade_price_step(int index, int trigger_roll, int direction_roll);
#endif
