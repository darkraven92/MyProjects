#include "../src/Bot/DisconnectDiagnosticPolicy.h"

#include <cassert>

int main()
{
    using Bot::DisconnectDiagnosticEvent;
    Bot::DisconnectDiagnosticPolicy policy;

    assert(policy.Update(true, 1000) == DisconnectDiagnosticEvent::Healthy);
    assert(policy.HaveSuccessfulSnapshot());
    assert(policy.Update(true, 60999) == DisconnectDiagnosticEvent::None);
    assert(policy.Update(true, 61000) == DisconnectDiagnosticEvent::Healthy);

    assert(policy.Update(false, 62000) == DisconnectDiagnosticEvent::SnapshotLost);
    assert(policy.SnapshotAgeMs(62000) == 1000);
    assert(policy.Update(false, 66999) == DisconnectDiagnosticEvent::None);
    assert(policy.Update(false, 67000) == DisconnectDiagnosticEvent::SnapshotStillUnavailable);
    assert(policy.Update(false, 72000) == DisconnectDiagnosticEvent::SnapshotStillUnavailable);
    assert(policy.Update(false, 92000) == DisconnectDiagnosticEvent::None);
    assert(policy.Update(false, 102000) == DisconnectDiagnosticEvent::SnapshotStillUnavailable);
    assert(policy.IncidentAgeMs(102000) == 40000);
    assert(policy.Update(true, 103000) == DisconnectDiagnosticEvent::SnapshotRecovered);
    assert(policy.SnapshotAgeMs(103000) == 0);
    assert(policy.Update(true, 103250) == DisconnectDiagnosticEvent::None);

    Bot::DisconnectDiagnosticPolicy startupWithoutWorld;
    assert(startupWithoutWorld.Update(false, 0) == DisconnectDiagnosticEvent::SnapshotLost);
    assert(!startupWithoutWorld.HaveSuccessfulSnapshot());
    assert(startupWithoutWorld.Update(true, 1000) == DisconnectDiagnosticEvent::SnapshotRecovered);
    assert(startupWithoutWorld.HaveSuccessfulSnapshot());
}
