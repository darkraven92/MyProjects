#pragma once
#include "EquipmentUpgradeScript.h"
#include "GrindBagMonitor.h"
#include "ObjectiveDefensiveCombatGuard.h"

namespace Bot
{
    class EquipmentUpgradeController
    {
        std::uint64_t nextProbe_=0;
        bool pending_=false;
        std::uint64_t pendingSince_=0;
        std::string lastResult_{};
    public:
        bool Pending() const { return pending_; }
        // Called only at the existing planner-safe boundary. This transaction
        // never writes movement, changes selection, or owns combat/navigation.
        void Update(const Objects::WorldState& world,const CombatController& combat,std::uint64_t tick)
        {
            if(pending_ && tick>=pendingSince_ && tick-pendingSince_>=40)
            {
                pending_=false;
                Debug::Logger::Info("EQUIPMENT UPGRADE result=failed reason=verification_deadline");
            }
            if(tick<nextProbe_ || !world.player.valid || world.player.health<=1 ||
               combat.LockedGuid()!=0 || combat.HasDeferredCorpseLootPending() ||
               ObjectiveDefensiveCombatGuard::HasDirectAggressor(world) ||
               (combat.State()!=CombatState::Idle && combat.State()!=CombatState::AcquiringTarget &&
                combat.State()!=CombatState::PostKillDelay)) return;
            nextProbe_=tick+(pending_?2:40);
            static const auto script=EquipmentUpgradeScript();
            std::string result;
            if(!GrindBagMonitor::ExecuteLuaReadback<65536>(
                    "WOW_INTERNAL_EQUIPMENT_TICK="+std::to_string(tick)+"; local function scan() "+script+
                    " end; scan(); WOW_INTERNAL_EQUIPMENT_READBACK=WOW_INTERNAL_EQUIPMENT_RESULT..'\\n'..(WOW_INTERNAL_EQUIPMENT_DIAGNOSTICS or '')",
                    "wow-internal/EquipmentUpgrade.lua",result,
                                                   "WOW_INTERNAL_EQUIPMENT_READBACK")) return;
            if(result.size()==65535)
                Debug::Logger::Info("EQUIPMENT CANDIDATE result=diagnostics_truncated reason=bounded_readback");
            const auto separator=result.find('\n');
            if(separator!=std::string::npos)
            {
                std::istringstream diagnostics(result.substr(separator+1));
                std::string line;
                while(std::getline(diagnostics,line))
                    if(!line.empty()) Debug::Logger::Info(line);
                result.resize(separator);
            }
            pending_=result=="pending" || result.rfind("issued|",0)==0;
            if(result.rfind("issued|",0)==0) pendingSince_=tick;
            nextProbe_=tick+(pending_?2:40);
            if(result!=lastResult_ && result!="no_change" && result!="pending")
            {
                std::istringstream fields(result);
                std::string state,item,slot,before,after;
                std::getline(fields,state,'|'); std::getline(fields,item,'|');
                std::getline(fields,slot,'|'); std::getline(fields,before,'|'); std::getline(fields,after,'|');
                if(state=="issued" || state=="verified")
                    Debug::Logger::Info("EQUIPMENT UPGRADE item="+item+" slot="+slot+
                        " result="+(state=="issued"?"issued":"confirmed")+
                        " reason="+(state=="issued"?"client_slot_approved":"equipped_item_id_matches")+
                        (state=="issued"?" currentScore="+before+" candidateScore="+after:""));
                else
                {
                    auto detail=result;
                    std::replace(detail.begin(),detail.end(),'|',' ');
                    Debug::Logger::Info("EQUIPMENT UPGRADE result="+detail);
                }
            }
            lastResult_=result;
        }
    };
}
