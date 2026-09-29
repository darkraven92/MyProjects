#pragma once

#include "GameThreadDispatcher.h"
#include "IObjectiveExecutor.h"
#include "ObjectiveDefensiveCombatGuard.h"
#include "TargetController.h"
#include "ValleyOfTrialsProfiles.h"

#include "../Debug/Logger.h"
#include "../Navigation/GenericNavMeshPathFollower.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

namespace Bot
{
    class UseItemOnUnitExecutor : public IObjectiveExecutor
    {
    private:
        static constexpr std::uintptr_t LuaDoStringRva = 0x00304CD0;
        static constexpr std::uintptr_t GetTextRva = 0x00303BF0;
        static constexpr const char* ResultVariable = "WOW_INTERNAL_USEITEM_RESULT";

        static constexpr std::uint32_t KalimdorMapId = 1;
        static constexpr float InteractionDistance = 6.5f;
        static constexpr float LiveTargetSearchDistance = 140.0f;
        static constexpr float SearchSeedArrivalDistance = 35.0f;
        static constexpr std::uint64_t TargetReuseCooldownTicks = 48;
        static constexpr std::uint64_t UseRetryTicks = 8;
        static constexpr std::uint64_t MaximumObjectiveTicks = 1600;

        ObjectiveExecutorState state_ = ObjectiveExecutorState::Idle;
        const QuestProfile* profile_ = nullptr;
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> navigator_{};
        std::uint64_t objectiveStartTick_ = 0;
        std::uint64_t targetGuid_ = 0;
        std::uint64_t lastUseTick_ = 0;
        int usesIssued_ = 0;
        bool navigatingSearchSeed_ = false;
        bool searchSeedVisited_ = false;
        std::unordered_map<std::uint64_t, std::uint64_t> targetCooldownUntil_{};
        ObjectiveDefensiveCombatGuard defense_{};

        static const char* StateNameInternal(ObjectiveExecutorState state)
        {
            switch (state)
            {
                case ObjectiveExecutorState::Idle: return "Idle";
                case ObjectiveExecutorState::Navigating: return "Navigating";
                case ObjectiveExecutorState::Executing: return "Executing";
                case ObjectiveExecutorState::ReadyForTurnIn: return "ReadyForTurnIn";
                case ObjectiveExecutorState::Failed: return "Failed";
                default: return "Unknown";
            }
        }

        void SetState(ObjectiveExecutorState next)
        {
            if (state_ == next)
                return;
            Debug::Logger::Info(
                std::string("UseItemOnUnitExecutor state: ") +
                StateNameInternal(state_) + " -> " + StateNameInternal(next));
            state_ = next;
        }

        const PlannerQuestLogEntry* FindLiveQuest(
            const QuestPlannerSnapshot& snapshot) const
        {
            if (profile_ == nullptr)
                return nullptr;

            for (const auto& entry : snapshot.quests)
            {
                const auto* mapped = ValleyOfTrialsProfiles::Find(entry, snapshot.classToken);
                if (mapped != nullptr && mapped->questId == profile_->questId)
                    return &entry;
            }
            return nullptr;
        }

        const Objects::UnitState* FindUnitByGuid(
            const Objects::WorldState& world,
            std::uint64_t guid) const
        {
            for (const auto& unit : world.units)
            {
                if (unit.valid && unit.guid == guid)
                    return &unit;
            }
            return nullptr;
        }

        const Objects::UnitState* FindAvailableTarget(
            const Objects::WorldState& world,
            std::uint64_t tick) const
        {
            if (profile_ == nullptr)
                return nullptr;

            const Objects::UnitState* best = nullptr;
            for (const auto& unit : world.units)
            {
                if (!unit.valid || unit.guid == 0 || unit.health == 0 ||
                    unit.maxHealth == 0 ||
                    unit.entryId != profile_->objective.targetEntry ||
                    unit.distance > LiveTargetSearchDistance)
                {
                    continue;
                }

                const auto it = targetCooldownUntil_.find(unit.guid);
                if (it != targetCooldownUntil_.end() && tick < it->second)
                    continue;

                if (best == nullptr || unit.distance < best->distance)
                    best = &unit;
            }
            return best;
        }

        static bool IsExecutable(std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &info, sizeof(info)) == 0 ||
                info.State != MEM_COMMIT ||
                (info.Protect & PAGE_GUARD) != 0 ||
                (info.Protect & PAGE_NOACCESS) != 0)
                return false;

            switch (info.Protect & 0xFF)
            {
                case PAGE_EXECUTE:
                case PAGE_EXECUTE_READ:
                case PAGE_EXECUTE_READWRITE:
                case PAGE_EXECUTE_WRITECOPY:
                    return true;
                default:
                    return false;
            }
        }

        static std::uintptr_t LuaDoStringAddress()
        {
            return Wow5875::Client::Base() + LuaDoStringRva;
        }

        static std::uintptr_t GetTextAddress()
        {
            return Wow5875::Client::Base() + GetTextRva;
        }

        bool UseQuestItem(std::string& result)
        {
            if (profile_ == nullptr || profile_->objective.itemId == 0)
                return false;

            const auto doStringAddress = LuaDoStringAddress();
            const auto getTextAddress = GetTextAddress();
            if (!IsExecutable(doStringAddress) || !IsExecutable(getTextAddress))
                return false;

            using DoStringFunction = bool (__fastcall*)(const char*, const char*);
            using GetTextFunction = const char* (__fastcall*)(char*, std::uint32_t, int);
            const auto doString = reinterpret_cast<DoStringFunction>(doStringAddress);
            const auto getText = reinterpret_cast<GetTextFunction>(getTextAddress);

            const std::string itemNeedle =
                "item:" + std::to_string(profile_->objective.itemId) + ":";

            const std::string script =
                "WOW_INTERNAL_USEITEM_RESULT='item_not_found'; "
                "local needle='" + itemNeedle + "'; local used=0; "
                "for b=0,4 do if used==0 then "
                "for s=1,GetContainerNumSlots(b) do "
                "local l=GetContainerItemLink(b,s); "
                "if l and string.find(l,needle,1,true) then "
                "UseContainerItem(b,s); used=1; "
                "WOW_INTERNAL_USEITEM_RESULT='used|'..b..'|'..s; break; end; end; end; end";

            char buffer[256]{};
            bool onGameThread = false;
            bool luaExecuted = false;
            bool gotText = false;
            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    luaExecuted = doString(
                        script.c_str(),
                        "wow-internal/UseItemOnUnitExecutor.lua");
                    if (!luaExecuted)
                        return;

                    const char* raw = getText(
                        const_cast<char*>(ResultVariable),
                        0xFFFFFFFFu,
                        0);
                    if (raw != nullptr && *raw != '\0')
                    {
                        std::strncpy(buffer, raw, sizeof(buffer) - 1);
                        buffer[sizeof(buffer) - 1] = '\0';
                        gotText = true;
                    }
                });

            if (!dispatched || !onGameThread || !luaExecuted || !gotText)
                return false;

            result = buffer;
            return true;
        }

        bool StartNavigation(
            const Objects::WorldState& world,
            std::uint64_t tick,
            const Navigation::NavPoint& destination,
            float arrivalDistance,
            const std::string& label,
            bool searchSeed)
        {
            navigator_ = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            if (!navigator_->Start(
                    world.player,
                    tick,
                    destination,
                    KalimdorMapId,
                    arrivalDistance,
                    label))
            {
                navigator_.reset();
                return false;
            }

            navigatingSearchSeed_ = searchSeed;
            SetState(ObjectiveExecutorState::Navigating);
            return true;
        }

        bool BeginTarget(
            const Objects::WorldState& world,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            targetGuid_ = target.guid;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("USE ITEM ON UNIT: TARGET");
            Debug::Logger::Info(
                "Entry=" + std::to_string(target.entryId) +
                " guid=" + std::to_string(static_cast<unsigned long long>(target.guid)) +
                " distance=" + std::to_string(target.distance));
            Debug::Logger::Info("================================");

            if (target.distance > InteractionDistance)
            {
                const Navigation::NavPoint destination{target.x, target.y, target.z};
                return StartNavigation(
                    world,
                    tick,
                    destination,
                    5.0f,
                    "UseItemOnUnit live target",
                    false);
            }

            SetState(ObjectiveExecutorState::Executing);
            return true;
        }

        void Complete()
        {
            navigator_.reset();
            Debug::Logger::Info("================================");
            Debug::Logger::Info("OBJECTIVE 11D.2.1: USE ITEM ON UNIT COMPLETE");
            Debug::Logger::Info(
                "Quest " + std::to_string(profile_->questId) + " " + profile_->title +
                " is complete in the live quest log.");
            Debug::Logger::Info("================================");
            SetState(ObjectiveExecutorState::ReadyForTurnIn);
        }

        void Fail(const std::string& reason)
        {
            navigator_.reset();
            Debug::Logger::Info("================================");
            Debug::Logger::Info("OBJECTIVE 11D.2.1: USE ITEM ON UNIT FAILED");
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info("================================");
            SetState(ObjectiveExecutorState::Failed);
        }

    public:
        bool Supports(const QuestProfile& profile) const override
        {
            return profile.objective.type == QuestObjectiveType::UseItemOnUnit &&
                   profile.objective.targetEntry != 0 &&
                   profile.objective.itemId != 0;
        }

        bool Start(
            const QuestProfile& profile,
            const Objects::WorldState& world,
            CombatController&,
            std::uint64_t tick) override
        {
            if (state_ != ObjectiveExecutorState::Idle || !Supports(profile))
                return false;

            profile_ = &profile;
            objectiveStartTick_ = tick;
            lastUseTick_ = 0;
            usesIssued_ = 0;
            targetGuid_ = 0;
            targetCooldownUntil_.clear();
            navigator_.reset();
            navigatingSearchSeed_ = false;
            searchSeedVisited_ = false;
            defense_.Reset(world, "UseItemOnUnit quest " + std::to_string(profile.questId));

            Debug::Logger::Info("================================");
            Debug::Logger::Info("OBJECTIVE EXECUTOR 11D.2.1: START");
            Debug::Logger::Info(
                "Quest: " + std::to_string(profile.questId) + " " + profile.title);
            Debug::Logger::Info("Type: UseItemOnUnit");
            Debug::Logger::Info(
                "Target: " + std::string(profile.objective.targetName) +
                " entry=" + std::to_string(profile.objective.targetEntry));
            Debug::Logger::Info(
                "Item=" + std::to_string(profile.objective.itemId) +
                " required=" + std::to_string(profile.objective.requiredCount));
            Debug::Logger::Info("================================");

            const auto* target = FindAvailableTarget(world, tick);
            if (target != nullptr)
                return BeginTarget(world, *target, tick);

            if (profile.destination.valid)
            {
                const Navigation::NavPoint destination{
                    profile.destination.x,
                    profile.destination.y,
                    profile.destination.z};
                if (StartNavigation(
                        world,
                        tick,
                        destination,
                        SearchSeedArrivalDistance,
                        profile.destination.label,
                        true))
                {
                    return true;
                }
            }

            SetState(ObjectiveExecutorState::Executing);
            return true;
        }

        void Update(
            const QuestPlannerSnapshot& snapshot,
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick) override
        {
            if (state_ == ObjectiveExecutorState::Idle ||
                state_ == ObjectiveExecutorState::ReadyForTurnIn ||
                state_ == ObjectiveExecutorState::Failed)
                return;

            const auto* liveQuest = FindLiveQuest(snapshot);
            if (liveQuest == nullptr)
            {
                Fail("active quest disappeared before completion was verified.");
                return;
            }

            if (world.player.health == 0)
            {
                Fail("player health reached zero; death recovery is not implemented.");
                return;
            }

            const ObjectiveDefenseUpdate defenseUpdate = defense_.Update(
                world,
                combat,
                navigator_.get(),
                targetGuid_,
                tick);

            if (defenseUpdate == ObjectiveDefenseUpdate::Failed)
            {
                Fail("defensive combat/recovery failed during UseItemOnUnit objective.");
                return;
            }

            if (defenseUpdate == ObjectiveDefenseUpdate::OwnsControl)
                return;

            if (SelectedObjectiveComplete(*liveQuest, *profile_))
            {
                Complete();
                return;
            }

            if (tick > objectiveStartTick_ + MaximumObjectiveTicks)
            {
                Fail("UseItemOnUnit objective timeout reached.");
                return;
            }

            if (state_ == ObjectiveExecutorState::Navigating)
            {
                if (!navigator_)
                {
                    Fail("navigation state is missing.");
                    return;
                }

                if (!navigatingSearchSeed_ && targetGuid_ != 0)
                {
                    const auto* target = FindUnitByGuid(world, targetGuid_);
                    if (target != nullptr && target->distance <= InteractionDistance)
                    {
                        navigator_.reset();
                        SetState(ObjectiveExecutorState::Executing);
                        return;
                    }
                }

                navigator_->Update(world.player, tick);

                if (navigator_->Failed())
                {
                    if (navigatingSearchSeed_)
                    {
                        Fail("NavMesh route to the UseItemOnUnit search seed failed.");
                        return;
                    }

                    navigator_.reset();
                    targetGuid_ = 0;
                    SetState(ObjectiveExecutorState::Executing);
                    return;
                }

                if (navigator_->Arrived())
                {
                    if (navigatingSearchSeed_)
                        searchSeedVisited_ = true;
                    navigator_.reset();
                    SetState(ObjectiveExecutorState::Executing);
                }
                return;
            }

            if (targetGuid_ == 0)
            {
                const auto* next = FindAvailableTarget(world, tick);
                if (next != nullptr)
                {
                    BeginTarget(world, *next, tick);
                    return;
                }

                if (!searchSeedVisited_ && profile_->destination.valid)
                {
                    const Navigation::NavPoint destination{
                        profile_->destination.x,
                        profile_->destination.y,
                        profile_->destination.z};
                    if (!StartNavigation(
                            world,
                            tick,
                            destination,
                            SearchSeedArrivalDistance,
                            profile_->destination.label,
                            true))
                    {
                        Fail("failed to start search-seed navigation.");
                    }
                }
                return;
            }

            const auto* target = FindUnitByGuid(world, targetGuid_);
            if (target == nullptr || target->health == 0)
            {
                targetGuid_ = 0;
                return;
            }

            if (target->distance > InteractionDistance)
            {
                const Navigation::NavPoint destination{target->x, target->y, target->z};
                if (!StartNavigation(
                        world,
                        tick,
                        destination,
                        5.0f,
                        "UseItemOnUnit live target replan",
                        false))
                {
                    targetCooldownUntil_[targetGuid_] = tick + TargetReuseCooldownTicks;
                    targetGuid_ = 0;
                }
                return;
            }

            if (world.player.targetGuid != targetGuid_)
            {
                Debug::Logger::Info(
                    "USE ITEM ON UNIT: selecting exact target GUID before item use.");
                if (!TargetController::SetTarget(targetGuid_))
                {
                    targetCooldownUntil_[targetGuid_] = tick + TargetReuseCooldownTicks;
                    targetGuid_ = 0;
                }
                return;
            }

            if (tick < lastUseTick_ + UseRetryTicks)
                return;

            std::string result;
            if (!UseQuestItem(result))
            {
                Fail("FrameScript quest-item use failed.");
                return;
            }

            Debug::Logger::Info(
                "USE ITEM ON UNIT: result=" + result +
                " targetGuid=" + std::to_string(static_cast<unsigned long long>(targetGuid_)));

            if (result.find("used|") != 0)
            {
                Fail("required quest item was not found in bags.");
                return;
            }

            ++usesIssued_;
            lastUseTick_ = tick;
            targetCooldownUntil_[targetGuid_] = tick + TargetReuseCooldownTicks;
            targetGuid_ = 0;
        }

        bool OwnsControl() const override
        {
            return state_ == ObjectiveExecutorState::Navigating ||
                   state_ == ObjectiveExecutorState::Executing ||
                   state_ == ObjectiveExecutorState::ReadyForTurnIn;
        }

        ObjectiveExecutorState State() const override
        {
            return state_;
        }

        const char* StateName() const override
        {
            return StateNameInternal(state_);
        }
    };
}
