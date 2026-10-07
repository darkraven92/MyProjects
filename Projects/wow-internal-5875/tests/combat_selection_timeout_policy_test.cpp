#include "../src/Bot/CombatSelectionTimeoutPolicy.h"
#include <cassert>

int main()
{
    using namespace Bot;
    CombatSelectionTimeoutEvidence e{};
    e.optionalGrind=true; e.known=true; e.hostileEngaged=false;
    e.attackKnown=true; e.attackActive=false;
    assert(DecideSelectionTimeout(e)==CombatSelectionTimeoutAction::AbandonOptional);
    e.optionalGrind=false; e.mandatoryObjective=true;
    assert(DecideSelectionTimeout(e)==CombatSelectionTimeoutAction::FailMandatoryOwner);
    e.hostileEngaged=true;
    assert(DecideSelectionTimeout(e)==CombatSelectionTimeoutAction::SystemFail);
    e.hostileEngaged=false; e.known=false;
    assert(DecideSelectionTimeout(e)==CombatSelectionTimeoutAction::SystemFail);
    e.known=true; e.attackActive=true;
    assert(DecideSelectionTimeout(e)==CombatSelectionTimeoutAction::SystemFail);
    e.attackActive=false; e.attackKnown=false;
    assert(DecideSelectionTimeout(e)==CombatSelectionTimeoutAction::SystemFail);
    e.attackKnown=true; e.mandatoryObjective=false;
    assert(DecideSelectionTimeout(e)==CombatSelectionTimeoutAction::SystemFail);
}
