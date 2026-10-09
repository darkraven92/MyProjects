#include "trade.h"
int sr_trade_price_step(int index, int trigger_roll, int direction_roll) {
    if (index < 1 || index > 11 || trigger_roll < 1 || trigger_roll > 4) return 0;
    if (trigger_roll == 1) {
        int max = (index > 8 || index < 4) ? 3 : 2;
        if (direction_roll < 1 || direction_roll > max) return 0;
        if (index > 8) index += direction_roll == 1 ? 1 : -1;
        else if (index < 4) index += direction_roll == 1 ? -1 : 1;
        else index += direction_roll == 1 ? -1 : 1;
    }
    if (index < 1) index = 1;
    if (index > 11) index = 11;
    return index;
}
