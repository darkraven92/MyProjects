#include "../src/Bot/AfkDeadGhostPolicy.h"
#include "../src/Bot/AfkProductionPolicy.h"
#include <cassert>
#include <string>
#include <filesystem>
#include <fstream>
#include <iterator>

int main()
{
    using namespace Bot;
    AfkDeadGhostPolicy evidence;
    assert(!evidence.NeedsQualification(AfkLifeState::Alive));
    AfkSafety gap; gap.navigation=true;
    for (auto life : {AfkLifeState::Dead,AfkLifeState::Ghost})
    {
        assert(evidence.NeedsQualification(life));
        AfkProductionPolicy scheduler;
        AfkObservation o{true,false,false,1000,0,300000};
        assert(scheduler.Update(o,gap,0,false,true).action==AfkAction::None);
        const auto action=scheduler.Update(o,gap,1,false,true,
            AfkAutoClearSetting::Unknown,evidence.NeedsQualification(life));
        assert(action.action==AfkAction::InputPulse);
        assert(!scheduler.Due()); // qualification purpose, not native timer due
        scheduler.Issued(action.action,o,1);
        evidence.Issued(life,o,1,true,true,true);
        assert(!evidence.NeedsQualification(life));
        assert(scheduler.Update(o,gap,2,false,true).action==AfkAction::None);
        assert(evidence.Verify(o,2,life)==AfkResult::Pending);
        o.lastInput=1001; o.clientNow=1002;
        assert(evidence.Verify(o,3,life)==AfkResult::Confirmed);
        assert(evidence.Qualified(life));
        assert(!evidence.NeedsQualification(life));
        assert(scheduler.Update(o,gap,3,false,true).result==AfkResult::Confirmed);
        o.clientNow=o.lastInput+239999;
        assert(scheduler.Update(o,gap,4,false,true).action==AfkAction::None);
        o.clientNow++;
        assert(scheduler.Update(o,gap,5,false,true).action==AfkAction::InputPulse);
    }
    // A life-state request stays armed across command and transaction windows.
    // First safe gap is eligible on the next update, with no timer interval.
    for (auto field : {&AfkSafety::death,&AfkSafety::combat,&AfkSafety::recovery,
        &AfkSafety::water,&AfkSafety::dialog,&AfkSafety::vendor,&AfkSafety::trainer,
        &AfkSafety::talents,&AfkSafety::equipment,&AfkSafety::loot,&AfkSafety::fault})
    {
        AfkProductionPolicy scheduler;
        AfkDeadGhostPolicy trial;
        AfkObservation early{true,false,false,2000,0,300000};
        auto unsafe=gap; unsafe.*field=true;
        assert(scheduler.Update(early,unsafe,0,false,true,AfkAutoClearSetting::Unknown,
            trial.NeedsQualification(AfkLifeState::Ghost)).status==AfkStatus::Blocked);
        assert(trial.NeedsQualification(AfkLifeState::Ghost));
        assert(scheduler.Update(early,gap,1,false,true,AfkAutoClearSetting::Unknown,
            trial.NeedsQualification(AfkLifeState::Ghost)).action==AfkAction::InputPulse);
    }
    AfkObservation late{true,false,false,280000,0,300000};
    AfkDeadGhostPolicy lateDeath;
    assert(lateDeath.Blocker(AfkLifeState::Dead,AfkDeathGap::CommandInFlight,late));
    AfkProductionPolicy scheduler;
    auto command=gap; command.death=true;
    assert(scheduler.Update(late,command,0,false,true,AfkAutoClearSetting::Unknown,true).action==AfkAction::None);
    assert(scheduler.Due());
    late.clientNow=282000;
    assert(!lateDeath.Blocker(AfkLifeState::Ghost,AfkDeathGap::RoutingToCorpse,late));
    auto action=scheduler.Update(late,gap,1,false,true,AfkAutoClearSetting::Unknown,true);
    assert(action.action==AfkAction::InputPulse);
    scheduler.Issued(action.action,late,1);
    lateDeath.Issued(AfkLifeState::Ghost,late,1,true,true,true);
    late.lastInput=282001; late.clientNow=282002;
    assert(lateDeath.Verify(late,2,AfkLifeState::Ghost)==AfkResult::Confirmed);
    assert(scheduler.Update(late,gap,2,false,true).result==AfkResult::Confirmed);
    assert(!scheduler.Due());
    for (int failedProof=0; failedProof<6; ++failedProof)
    {
        AfkDeadGhostPolicy trial;
        AfkObservation early{true,false,false,2000,0,300000};
        trial.Issued(AfkLifeState::Ghost,early,0,failedProof!=0,failedProof!=1,failedProof!=2);
        if (failedProof>=3)
        {
            if (failedProof!=4) { early.lastInput=2001; early.clientNow=2002; }
            assert(trial.Verify(early,failedProof==4 ? 3000 : 1,
                failedProof==3 ? AfkLifeState::Dead : failedProof==4 ?
                    AfkLifeState::Ghost : AfkLifeState::Unknown)==AfkResult::Failed);
        }
        assert(trial.Failed() && !trial.NeedsQualification(AfkLifeState::Ghost));
    }
    AfkDeadGhostPolicy unknown;
    assert(unknown.Blocker(AfkLifeState::Ghost,AfkDeathGap::RoutingToCorpse,{}));
    late.clientNow=late.lastInput+300000;
    assert(unknown.Blocker(AfkLifeState::Ghost,AfkDeathGap::RoutingToCorpse,late));
    for (int flags=1; flags<4; ++flags)
    {
        auto active=late; active.clientAfk=(flags&1)!=0; active.serverAfk=(flags&2)!=0;
        assert(unknown.Blocker(AfkLifeState::Ghost,AfkDeathGap::RoutingToCorpse,active));
    }
    assert(AfkWorkloadSafetyPolicy::LandMovementAllowed(0x13f,true));
    const auto movement=AfkWorkloadSafetyPolicy::MovementEvidence(0x10000001,true);
    assert(movement.allowedMask==0x13f && movement.unsupportedBits==0x10000000);
    assert(movement.Fields()=="movementFlags=0x10000001 allowedMask=0x0000013f unsupportedBits=0x10000000");
    for (auto bit : {0x40u,0x80u,0x400u,0x800u,0x1000u,0x2000u,0x4000u,
        0x200000u,0x2000000u,0x10000000u,0x80000000u})
        assert(!AfkWorkloadSafetyPolicy::LandMovementAllowed(bit,true));
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    auto read=[](const auto& path) {
        std::ifstream file(path); assert(file);
        return std::string(std::istreambuf_iterator<char>(file),{});
    };
    const auto shared=read(root/"src/Bot/SharedAfkController.h");
    assert(shared.find("autoClear,lifeQualificationRequested")!=std::string::npos);
    assert(shared.find("deadGhost_.Verify(o,now,life)")!=std::string::npos);
    assert(shared.find("dispatch.lifeVerified && pulseLife==life")!=std::string::npos);
    assert(shared.find("AFK DEAD/GHOST MOVEMENT BLOCK")!=std::string::npos);
    const auto adapter=read(root/"src/Bot/AfkClient5875.h");
    assert(adapter.find("Core::Memory::Write")==std::string::npos);
    assert(adapter.find("deadGhostPulse && (action!=AfkAction::InputPulse")!=std::string::npos);
}
