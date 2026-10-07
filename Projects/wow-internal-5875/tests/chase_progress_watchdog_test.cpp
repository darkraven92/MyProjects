#include "../src/Bot/ChaseProgressWatchdog.h"
#include <cassert>

int main()
{
    using namespace Bot;
    ChaseProgressWatchdog watch({.meaningfulNetMovement=0.65f,
        .meaningfulGoalGain=0.75f,.suspectedStallTicks=8,.hardStallTicks=16});
    watch.Reset(0,0,0,10,0,0,10,0);
    // Command dispatch is not an observation. Sideways displacement is not
    // progress toward the locked target even though the player moved.
    assert(!watch.Update(0,0,0,10,0,0,10,7,true).progressed);
    auto lateral=watch.Update(0,2,0,10,0,0,10,8,true);
    assert(!lateral.progressed);
    assert(lateral.severity==MovementStallSeverity::Suspected);
    auto hard=watch.Update(0,2,0,10,0,0,10,16,true);
    assert(hard.severity==MovementStallSeverity::Hard);
    assert(watch.Update(1,2,0,10,0,0,9,17,true).progressed);
    assert(watch.Update(1,2,0,10,0,0,9,18,true).severity==MovementStallSeverity::None);
    watch.Suspend(1,2,0,10,0,0,9,1000); // navigation or water owner
    assert(watch.Update(1,2,0,10,0,0,9,1001,true).severity==MovementStallSeverity::None);
    return 0;
}
