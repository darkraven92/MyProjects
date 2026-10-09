#pragma once
#include "QuestPlannerTypes.h"
#include <cmath>
namespace Bot
{
    struct TalkToNpcPolicy
    {
        static constexpr float InteractionRange = 4.5f; // existing generic turn-in range
        static constexpr std::uint64_t DialogWaitTicks = 8;
        static constexpr std::uint64_t ProgressWaitTicks = 40;
        static constexpr std::uint64_t MaximumObjectiveTicks = 1200;
        static constexpr unsigned MaximumAttempts = 3;
        static bool Supports(const QuestProfile& p)
        {
            return p.objective.type == QuestObjectiveType::TalkToNpc && p.objective.targetEntry != 0 &&
                p.objective.requiredCount == 1 && p.destination.valid && !p.gossipCreditText.empty();
        }
        static bool MayInteract(bool active, bool complete, bool alive, bool defenseOwned,
            bool matchingNpc, float distance, unsigned attempts)
        {
            return active && !complete && alive && !defenseOwned && matchingNpc &&
                std::isfinite(distance) && distance >= 0 && distance <= InteractionRange && attempts < MaximumAttempts;
        }
        static bool TimedOut(std::uint64_t tick, std::uint64_t start)
        { return tick >= start && tick - start >= MaximumObjectiveTicks; }
        static bool ProgressWaitExpired(std::uint64_t tick, std::uint64_t issued)
        { return tick >= issued && tick - issued >= ProgressWaitTicks; }
        static std::string CreditScript(const std::string& text)
        {
            // Lua decimal escapes avoid script injection/localization quoting.
            std::string literal="'";
            for (unsigned char c : text)
            {
                literal += '\\';
                literal += char('0'+c/100); literal += char('0'+(c/10)%10); literal += char('0'+c%10);
            }
            literal += '\'';
            return "WOW_INTERNAL_TALK_RESULT='no_matching_credit_option'; "
                "if GossipFrame and GossipFrame:IsVisible() and GetGossipOptions and SelectGossipOption then "
                "local a={GetGossipOptions()}; if table.getn(a)==2 and a[1]=="+literal+
                " and a[2]=='gossip' then SelectGossipOption(1); WOW_INTERNAL_TALK_RESULT='issued'; end; end";
        }
    };
}
