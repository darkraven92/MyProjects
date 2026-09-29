#pragma once

#include "QuestPlannerTypes.h"
#include "ValleyOfTrialsProfiles.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace Bot
{
    class QuestPlanner
    {
    private:
        static int Score(
            const QuestProfile& profile,
            bool complete)
        {
            int score = profile.priority;

            if (complete)
                score += 10000;

            if (!profile.optional)
                score += 1000;

            return score;
        }

        static int SelectActiveObjectiveIndex(
            const QuestProfile& profile,
            const PlannerQuestLogEntry& entry)
        {
            if (entry.complete || profile.objectives.empty())
                return -1;

            int firstKnown = -1;

            for (std::size_t i = 0; i < profile.objectives.size(); ++i)
            {
                const int leaderboard = profile.objectives[i].leaderboardIndex;
                if (leaderboard < 0 ||
                    static_cast<std::size_t>(leaderboard) >=
                        entry.objectiveComplete.size())
                {
                    continue;
                }

                if (firstKnown < 0)
                    firstKnown = static_cast<int>(i);

                if (!entry.objectiveComplete[
                        static_cast<std::size_t>(leaderboard)])
                {
                    return static_cast<int>(i);
                }
            }

            /*
             * If the profile is v2 but the live client has not exposed a
             * complete set of leaderboard bits yet, execute the first step.
             * If every observed row is already complete but the whole quest
             * flag has not flipped on this exact tick, returning the first
             * known step causes an immediate step-complete refresh instead of
             * targeting an unrelated objective.
             */
            if (firstKnown >= 0)
                return firstKnown;

            return 0;
        }

    public:
        static const char* ActionName(
            QuestPlannerAction action)
        {
            switch (action)
            {
                case QuestPlannerAction::None:
                    return "None";
                case QuestPlannerAction::TurnIn:
                    return "TurnIn";
                case QuestPlannerAction::ExecuteObjective:
                    return "ExecuteObjective";
                case QuestPlannerAction::DiscoverPickup:
                    return "DiscoverPickup";
                case QuestPlannerAction::UnsupportedActiveQuest:
                    return "UnsupportedActiveQuest";
                default:
                    return "Unknown";
            }
        }

        static const char* ObjectiveTypeName(
            QuestObjectiveType type)
        {
            switch (type)
            {
                case QuestObjectiveType::TalkToNpc:
                    return "TalkToNpc";
                case QuestObjectiveType::KillMob:
                    return "KillMob";
                case QuestObjectiveType::CollectItemFromMob:
                    return "CollectItemFromMob";
                case QuestObjectiveType::CollectWorldItem:
                    return "CollectWorldItem";
                case QuestObjectiveType::UseItemOnUnit:
                    return "UseItemOnUnit";
                case QuestObjectiveType::InteractGameObject:
                    return "InteractGameObject";
                case QuestObjectiveType::TravelReport:
                    return "TravelReport";
                default:
                    return "Unknown";
            }
        }

        static const char* RouteGroupName(
            QuestRouteGroup group)
        {
            switch (group)
            {
                case QuestRouteGroup::None:
                    return "None";
                case QuestRouteGroup::ValleyExterior:
                    return "ValleyExterior";
                case QuestRouteGroup::BurningBladeCoven:
                    return "BurningBladeCoven";
                case QuestRouteGroup::SenjinRoad:
                    return "SenjinRoad";
                case QuestRouteGroup::SenjinVillage:
                    return "SenjinVillage";
                case QuestRouteGroup::SenjinCoast:
                    return "SenjinCoast";
                case QuestRouteGroup::EchoIsles:
                    return "EchoIsles";
                case QuestRouteGroup::RazorHillRoad:
                    return "RazorHillRoad";
                default:
                    return "Unknown";
            }
        }

        static const char* SupportName(
            QuestExecutionSupport support)
        {
            switch (support)
            {
                case QuestExecutionSupport::VerifiedExistingSubsystem:
                    return "VerifiedExistingSubsystem";
                case QuestExecutionSupport::ProfiledPlannerOnly:
                    return "ProfiledPlannerOnly";
                default:
                    return "Unknown";
            }
        }

        static QuestPlannerPlan Evaluate(
            const QuestPlannerSnapshot& snapshot,
            const std::set<int>* temporarilyBlockedQuestIds = nullptr,
            const std::set<int>* completedQuestIds = nullptr)
        {
            QuestPlannerPlan plan{};

            if (!snapshot.valid)
            {
                plan.reason = "planner snapshot is invalid.";
                return plan;
            }

            struct Candidate
            {
                const QuestProfile* profile = nullptr;
                const PlannerQuestLogEntry* entry = nullptr;
                int score = 0;
                int activeObjectiveIndex = -1;
                bool blocked = false;
            };

            std::vector<Candidate> candidates;

            for (const auto& entry : snapshot.quests)
            {
                const auto* profile = ValleyOfTrialsProfiles::Find(
                    entry,
                    snapshot.classToken,
                    completedQuestIds);

                if (profile == nullptr || !profile->automatable)
                {
                    plan.unknownActiveTitles.push_back(entry.title);
                    continue;
                }

                ++plan.waveActiveQuestCount;
                if (entry.complete)
                    ++plan.waveReadyForTurnInCount;
                else
                    ++plan.waveIncompleteCount;

                const bool blocked =
                    !entry.complete &&
                    temporarilyBlockedQuestIds != nullptr &&
                    temporarilyBlockedQuestIds->find(profile->questId) !=
                        temporarilyBlockedQuestIds->end();
                if (blocked)
                    ++plan.waveBlockedCount;

                candidates.push_back(
                    {
                        profile,
                        &entry,
                        Score(*profile, entry.complete),
                        SelectActiveObjectiveIndex(*profile, entry),
                        blocked
                    });
            }

            if (candidates.empty())
            {
                if (!plan.unknownActiveTitles.empty())
                {
                    plan.action = QuestPlannerAction::UnsupportedActiveQuest;
                    plan.reason =
                        "active quest exists but is either absent from the "
                        "database/profile catalogue or deferred because its "
                        "objective pattern has no generic executor yet.";
                    return plan;
                }

                plan.action = QuestPlannerAction::DiscoverPickup;
                plan.reason =
                    "no known active quest; discover available quests from "
                    "eligible database/profiled quest givers.";
                return plan;
            }

            std::sort(
                candidates.begin(),
                candidates.end(),
                [](const Candidate& a, const Candidate& b)
                {
                    if (a.score != b.score)
                        return a.score > b.score;
                    return a.profile->questId < b.profile->questId;
                });

            /*
             * Phase 13B.1 quest-wave policy:
             *
             * Do not hand in a completed quest while any supported quest in
             * the current accepted wave still has work left.  This is the
             * core "pick up everything -> finish the wave -> turn in the
             * wave" rule.  The live quest log remains authoritative; a
             * follow-up that only appears after a turn-in naturally belongs
             * to the next wave.
             *
             * Candidates are score-sorted. Phase 13C.2.1 refines this rule
             * below so deliberately deferred late-wave work does not prevent
             * the character from banking XP/rewards from the easier core wave.
             */
            /*
             * Phase 13C.2.1 late-wave policy:
             *
             * Some quests are intentionally better after the character has
             * banked XP/rewards from the easier part of a hub.  A profile
             * marked optional/late-wave is therefore allowed to remain
             * incomplete while completed core-wave quests are turned in.
             *
             * Selection order becomes:
             *   1. incomplete core-wave work
             *   2. completed quests (turn in XP/rewards)
             *   3. incomplete late-wave work
             *
             * This preserves the normal "finish the wave before turn-in"
             * behavior for every ordinary quest while providing a generic,
             * data-driven escape hatch for deliberately delayed difficult
             * quests such as Sen'jin quest 786.
             */
            /*
             * Phase 13D.5.1 refines the Sen'jin ordering one step further:
             * hub-exit travel is never "core local work". A deliberately
             * delayed local quest (786 today, data-driven late-wave quests in
             * future hubs) must still run before Report to Orgnil / other
             * zone-exit travel.
             *
             * Final wave order:
             *   1. incomplete ordinary local work
             *   2. completed quests -> bank XP/rewards
             *   3. incomplete late-wave local work
             *   4. incomplete hub-exit travel
             */
            auto selectedIt = std::find_if(
                candidates.begin(),
                candidates.end(),
                [](const Candidate& candidate)
                {
                    return candidate.entry != nullptr &&
                           !candidate.entry->complete &&
                           !candidate.blocked &&
                           !candidate.profile->optional &&
                           !candidate.profile->hubExit;
                });

            if (selectedIt == candidates.end())
            {
                selectedIt = std::find_if(
                    candidates.begin(),
                    candidates.end(),
                    [](const Candidate& candidate)
                    {
                        return candidate.entry != nullptr &&
                               candidate.entry->complete;
                    });
            }

            if (selectedIt == candidates.end())
            {
                selectedIt = std::find_if(
                    candidates.begin(),
                    candidates.end(),
                    [](const Candidate& candidate)
                    {
                        return candidate.entry != nullptr &&
                               !candidate.entry->complete &&
                               !candidate.blocked &&
                               candidate.profile->optional &&
                               !candidate.profile->hubExit;
                    });
            }

            if (selectedIt == candidates.end())
            {
                selectedIt = std::find_if(
                    candidates.begin(),
                    candidates.end(),
                    [](const Candidate& candidate)
                    {
                        return candidate.entry != nullptr &&
                               !candidate.entry->complete &&
                               !candidate.blocked &&
                               candidate.profile->hubExit;
                    });
            }

            if (selectedIt == candidates.end())
            {
                // Bounded-failure policy: if unfinished work is quarantined,
                // hold planner ownership until the quarantine timer expires.
                if (plan.waveBlockedCount > 0)
                {
                    plan.action = QuestPlannerAction::UnsupportedActiveQuest;
                    plan.reason =
                        "all remaining supported quest work is temporarily "
                        "quarantined after bounded runtime/objective failures.";
                    return plan;
                }

                plan.action = QuestPlannerAction::DiscoverPickup;
                plan.reason = "no executable quest-wave candidate remains.";
                return plan;
            }

            const auto& selected = *selectedIt;
            plan.primary = selected.profile;
            plan.activeObjectiveIndex = selected.activeObjectiveIndex;
            plan.action = selected.entry->complete
                ? QuestPlannerAction::TurnIn
                : QuestPlannerAction::ExecuteObjective;

            if (selected.entry->complete)
            {
                const bool lateWorkRemains = std::any_of(
                    candidates.begin(),
                    candidates.end(),
                    [](const Candidate& candidate)
                    {
                        return candidate.entry != nullptr &&
                               !candidate.entry->complete &&
                               !candidate.blocked &&
                               candidate.profile->optional;
                    });

                plan.reason = lateWorkRemains
                    ? "core quest-wave complete; turn in XP/rewards before deferred late-wave work."
                    : "quest-wave objective set is complete; begin batched turn-in.";
            }
            else if (selected.profile->optional && !selected.profile->hubExit)
            {
                plan.reason =
                    "core quest-wave has no remaining work/turn-ins; selected deferred late-wave local quest before hub exit.";
            }
            else if (selected.profile->hubExit)
            {
                plan.reason =
                    "all ordinary and late-wave local work is exhausted; selected hub-exit travel.";
            }
            else if (selected.activeObjectiveIndex >= 0 &&
                     static_cast<std::size_t>(selected.activeObjectiveIndex) <
                         selected.profile->objectives.size())
            {
                const auto& step = selected.profile->objectives[
                    static_cast<std::size_t>(selected.activeObjectiveIndex)];
                plan.reason =
                    "quest-wave still has incomplete work; selected database "
                    "objective step (leaderboard=" +
                    std::to_string(step.leaderboardIndex + 1) + ").";
            }
            else
            {
                plan.reason =
                    "quest-wave still has an incomplete objective.";
            }

            if (!selected.entry->complete &&
                selected.profile->routeGroup != QuestRouteGroup::None)
            {
                for (const auto& candidate : candidates)
                {
                    if (candidate.entry->complete ||
                        candidate.blocked ||
                        candidate.profile->routeGroup != selected.profile->routeGroup)
                    {
                        continue;
                    }

                    plan.routeBundle.push_back(candidate.profile);
                }

                std::sort(
                    plan.routeBundle.begin(),
                    plan.routeBundle.end(),
                    [](const QuestProfile* a, const QuestProfile* b)
                    {
                        return a->priority > b->priority;
                    });
            }

            return plan;
        }
    };
}
