#pragma once

#include "QuestPlanner.h"
#include "QuestPlannerStateReader.h"
#include "ValleyOfTrialsProfiles.h"

#include "../Debug/Logger.h"

#include <cstdint>
#include <string>

namespace Bot
{
    class QuestPlannerProbe
    {
    private:
        bool initialized_ =
            false;

        bool readerValid_ =
            false;

        bool haveSnapshot_ =
            false;

        QuestPlannerSnapshot previous_{};

        static const char* YesNo(
            bool value)
        {
            return
                value
                    ? "yes"
                    : "no";
        }

        static void LogProfile(
            const char* prefix,
            const QuestProfile& profile)
        {
            Debug::Logger::Info(
                std::string(prefix) +
                "questId=" +
                std::to_string(
                    profile.questId
                ) +
                " title=\"" +
                profile.title +
                "\" giver=" +
                std::to_string(
                    profile.giverEntry
                ) +
                " turnIn=" +
                std::to_string(
                    profile.turnInEntry
                ) +
                " objective=" +
                QuestPlanner::
                    ObjectiveTypeName(
                        profile.objective.type
                    ) +
                " targetEntry=" +
                std::to_string(
                    profile.objective.targetEntry
                ) +
                " itemId=" +
                std::to_string(
                    profile.objective.itemId
                ) +
                " objectEntry=" +
                std::to_string(
                    profile.objective.objectEntry
                ) +
                " count=" +
                std::to_string(
                    profile.objective.requiredCount
                ) +
                " target=\"" +
                profile.objective.targetName +
                "\" routeGroup=" +
                QuestPlanner::
                    RouteGroupName(
                        profile.routeGroup
                    ) +
                " support=" +
                QuestPlanner::
                    SupportName(
                        profile.support
                    ) +
                " optional=" +
                YesNo(
                    profile.optional
                )
            );
        }

        static void LogSnapshot(
            const QuestPlannerSnapshot& snapshot)
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST PLANNER 11A: SNAPSHOT"
            );

            Debug::Logger::Info(
                "Class: " +
                snapshot.classToken
            );

            Debug::Logger::Info(
                "Level: " +
                std::to_string(
                    snapshot.playerLevel
                )
            );

            Debug::Logger::Info(
                "Raw quest log entries: " +
                std::to_string(
                    snapshot.rawQuestLogEntries
                )
            );

            Debug::Logger::Info(
                "Active non-header quests: " +
                std::to_string(
                    snapshot.quests.size()
                )
            );

            for (
                std::size_t i = 0;
                i < snapshot.quests.size();
                ++i)
            {
                const auto& entry =
                    snapshot.quests[i];

                const auto* profile =
                    ValleyOfTrialsProfiles::Find(
                        entry,
                        snapshot.classToken
                    );

                Debug::Logger::Info(
                    "Planner quest[" +
                    std::to_string(i) +
                    "]: title=\"" +
                    entry.title +
                    "\" complete=" +
                    YesNo(entry.complete) +
                    " objectiveRows=" +
                    std::to_string(
                        entry.objectiveCount
                    ) +
                    " mappedQuestId=" +
                    (
                        profile == nullptr
                            ? std::string("none")
                            : std::to_string(
                                profile->questId
                            )
                    )
                );
            }

            Debug::Logger::Info(
                "================================"
            );
        }

        static void LogPlan(
            const QuestPlannerPlan& plan)
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST PLANNER 11A: PLAN"
            );

            Debug::Logger::Info(
                std::string("Action: ") +
                QuestPlanner::
                    ActionName(
                        plan.action
                    )
            );

            Debug::Logger::Info(
                "Read-only: yes"
            );

            Debug::Logger::Info(
                "Reason: " +
                plan.reason
            );

            if (plan.primary != nullptr)
            {
                LogProfile(
                    "Primary: ",
                    *plan.primary
                );
            }

            if (!plan.routeBundle.empty())
            {
                Debug::Logger::Info(
                    "Route bundle count: " +
                    std::to_string(
                        plan.routeBundle.size()
                    )
                );

                for (
                    std::size_t i = 0;
                    i < plan.routeBundle.size();
                    ++i)
                {
                    LogProfile(
                        (
                            "Bundle[" +
                            std::to_string(i) +
                            "]: "
                        ).c_str(),
                        *plan.routeBundle[i]
                    );
                }
            }

            for (
                const auto& title :
                    plan.unknownActiveTitles)
            {
                Debug::Logger::Info(
                    "Unknown active quest: \"" +
                    title +
                    "\""
                );
            }

            Debug::Logger::Info(
                "================================"
            );
        }

    public:
        void Initialize()
        {
            if (initialized_)
            {
                return;
            }

            initialized_ =
                true;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST PLANNER PHASE 11A: DATA-DRIVEN READ-ONLY"
            );

            Debug::Logger::Info(
                "Profiles: Valley of Trials Orc Warrior catalogue."
            );

            Debug::Logger::Info(
                "No pickup, movement, combat, item-use or turn-in "
                "command is issued by Phase 11A."
            );

            std::string profileError;

            const bool profilesValid =
                ValleyOfTrialsProfiles::
                    Validate(
                        profileError
                    );

            Debug::Logger::Info(
                std::string(
                    "Quest profile validation "
                ) +
                (
                    profilesValid
                        ? "PASS."
                        : "FAILED."
                )
            );

            if (!profilesValid)
            {
                Debug::Logger::Info(
                    "Reason: " +
                    profileError
                );
            }

            Debug::Logger::Info(
                "Profile count: " +
                std::to_string(
                    ValleyOfTrialsProfiles::
                        All().size()
                )
            );

            readerValid_ =
                QuestPlannerStateReader::
                    Validate();

            Debug::Logger::Info(
                "================================"
            );
        }

        void Update(
            std::uint64_t tick)
        {
            if (!initialized_)
            {
                Initialize();
            }

            if (
                !readerValid_ ||
                (tick % 20) != 0)
            {
                return;
            }

            QuestPlannerSnapshot next{};

            if (!QuestPlannerStateReader::
                    Read(
                        next
                    ))
            {
                Debug::Logger::Info(
                    "QUEST PLANNER 11A: snapshot read failed."
                );

                return;
            }

            if (
                haveSnapshot_ &&
                QuestPlannerStateReader::
                    Same(
                        previous_,
                        next
                    ))
            {
                return;
            }

            previous_ =
                next;

            haveSnapshot_ =
                true;

            LogSnapshot(
                next
            );

            const auto plan =
                QuestPlanner::Evaluate(
                    next
                );

            LogPlan(
                plan
            );
        }
    };
}
