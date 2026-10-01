#include "../src/Navigation/IncrementalTileInitializationPolicy.h"

#include <cassert>
#include <cstddef>

using Navigation::IncrementalTileInitializationPolicy;

int main()
{
    IncrementalTileInitializationPolicy cursor;
    assert(!cursor.Active());
    std::size_t index = 999;
    assert(!cursor.Next(index));

    cursor.Begin(3);
    assert(cursor.Active() && cursor.Total() == 3);
    assert(cursor.Processed() == 0 && !cursor.Complete());
    cursor.BeginStep();
    assert(cursor.Next(index) && index == 0);
    assert(cursor.Processed() == 1 && !cursor.Complete());
    assert(cursor.Next(index) && index == 1);
    assert(!cursor.Next(index) && cursor.Processed() == 2);
    cursor.BeginStep();
    assert(cursor.Next(index) && index == 2);
    assert(cursor.Complete());
    assert(!cursor.Next(index) && cursor.Processed() == 3);
    cursor.MarkReady();
    assert(cursor.CurrentState() == IncrementalTileInitializationPolicy::State::Ready);
    assert(!cursor.Next(index));

    cursor.Begin(2);
    cursor.BeginStep();
    assert(cursor.Next(index) && index == 0);
    cursor.Fail();
    assert(cursor.CurrentState() == IncrementalTileInitializationPolicy::State::Failed);
    assert(!cursor.Next(index));

    // A new destination/tier gets a fresh episode; an abandoned episode
    // cannot leak a stale tile index into the replacement.
    cursor.Begin(9);
    cursor.BeginStep();
    assert(cursor.Next(index) && index == 0);
    cursor.Cancel();
    assert(!cursor.Active() && !cursor.Next(index));
    assert(cursor.CurrentState() == IncrementalTileInitializationPolicy::State::Cancelled);
    cursor.Begin(2);
    cursor.BeginStep();
    assert(cursor.Next(index) && index == 0);
    assert(cursor.Total() == 2);

    // Empty tile sets complete without ever processing a tile. The provider
    // rejects this as initialization_failed before attempting a Detour query.
    cursor.Begin(0);
    assert(cursor.Complete() && !cursor.Next(index));
    cursor.MarkReady();
    assert(cursor.CurrentState() == IncrementalTileInitializationPolicy::State::Ready);

    // The full-map case requires 352 distinct updates: no Step can consume
    // more than two tiles, duplicate a tile, or skip ahead on its own.
    cursor.Begin(704);
    for (std::size_t step = 0; step < 352; ++step)
    {
        cursor.BeginStep();
        for (std::size_t offset = 0;
             offset < IncrementalTileInitializationPolicy::MaxTilesPerStep;
             ++offset)
        {
            assert(cursor.Next(index));
            assert(index == step * 2 + offset);
        }
        assert(!cursor.Next(index));
        assert(cursor.Processed() == (step + 1) * 2);
        if (step == 0)
            assert(!cursor.Complete());
    }
    assert(cursor.Complete() && !cursor.Next(index));
}
