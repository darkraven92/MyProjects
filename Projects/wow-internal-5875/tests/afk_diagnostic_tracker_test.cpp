#include "../src/Bot/AfkDiagnosticTracker.h"
#include "../src/Objects/PlayerFlagsCandidate.h"

#include <cassert>
#include <cstdint>

int main()
{
    using Bot::AfkDiagnosticEvent;
    using Bot::AfkDiagnosticTracker;

    static_assert(!Objects::DecodeAfkCandidate(0x00000000));
    static_assert(Objects::DecodeAfkCandidate(0x00000002));
    static_assert(!Objects::DecodeAfkCandidate(0x00000004));
    static_assert(Objects::DecodeAfkCandidate(0x80000006));

    AfkDiagnosticTracker unknown;
    assert(unknown.Observe(false, false, 10).event == AfkDiagnosticEvent::None);
    assert(!unknown.EverKnown());
    assert(unknown.Stale());

    AfkDiagnosticTracker baseline;
    assert(baseline.Observe(true, false, 20).event == AfkDiagnosticEvent::None);
    assert(baseline.EverKnown());
    assert(!baseline.CandidateAfk());
    assert(!baseline.Stale());
    assert(baseline.Observe(true, false, 30).event == AfkDiagnosticEvent::None);
    const auto entered = baseline.Observe(true, true, 40);
    assert(entered.event == AfkDiagnosticEvent::Entered);
    assert(!entered.initialObservation);
    assert(entered.observedDurationMs == 0);
    assert(baseline.EnteredAtMs() == 40);
    assert(baseline.Observe(true, true, 55).event == AfkDiagnosticEvent::None);
    assert(baseline.ObservedDurationMs() == 15);
    assert(baseline.Observe(true, true, 54).event == AfkDiagnosticEvent::None);
    assert(baseline.ObservedDurationMs() == 15); // rollback cannot double count
    assert(baseline.Observe(true, true, 60).event == AfkDiagnosticEvent::None);
    assert(baseline.ObservedDurationMs() == 20);
    const auto cleared = baseline.Observe(true, false, 70);
    assert(cleared.event == AfkDiagnosticEvent::Cleared);
    assert(!cleared.initialObservation);
    assert(cleared.observedDurationMs == 20); // last positive observation
    assert(!baseline.CandidateAfk());
    assert(baseline.ObservedDurationMs() == 0);

    AfkDiagnosticTracker initiallyAfk;
    const auto initial = initiallyAfk.Observe(true, true, 100);
    assert(initial.event == AfkDiagnosticEvent::Entered);
    assert(initial.initialObservation);
    assert(initiallyAfk.Observe(true, true, 120).event == AfkDiagnosticEvent::None);
    assert(initiallyAfk.ObservedDurationMs() == 20);
    assert(initiallyAfk.Observe(false, false, 150).event == AfkDiagnosticEvent::None);
    assert(initiallyAfk.Stale());
    assert(initiallyAfk.EverKnown());
    assert(initiallyAfk.CandidateAfk());
    assert(initiallyAfk.ObservedDurationMs() == 20); // unknown gap is not counted
    assert(initiallyAfk.LastObservedAgeMs(150) == 30);
    assert(initiallyAfk.Observe(true, true, 200).event == AfkDiagnosticEvent::None);
    assert(!initiallyAfk.Stale());
    assert(initiallyAfk.ObservedDurationMs() == 20);
    assert(initiallyAfk.Observe(true, true, 210).event == AfkDiagnosticEvent::None);
    assert(initiallyAfk.ObservedDurationMs() == 30);
    const auto gapCleared = initiallyAfk.Observe(true, false, 220);
    assert(gapCleared.event == AfkDiagnosticEvent::Cleared);
    assert(gapCleared.observedDurationMs == 30);

    AfkDiagnosticTracker resumed;
    assert(resumed.Observe(true, false, 10).event == AfkDiagnosticEvent::None);
    assert(resumed.Observe(false, false, 20).event == AfkDiagnosticEvent::None);
    const auto afterGap = resumed.Observe(true, true, 30);
    assert(afterGap.event == AfkDiagnosticEvent::Entered);
    assert(afterGap.initialObservation);
    assert(resumed.LastObservedAgeMs(29) == 0);

    return 0;
}
