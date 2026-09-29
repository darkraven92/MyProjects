#pragma once

#include "GameThreadDispatcher.h"
#include "IObjectiveExecutor.h"
#include "ObjectiveDefensiveCombatGuard.h"
#include "ValleyOfTrialsProfiles.h"

#include "../Debug/Logger.h"
#include "../Navigation/GenericNavMeshPathFollower.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace Bot
{
    class CollectWorldItemExecutor : public IObjectiveExecutor
    {
    private:
        static constexpr std::uintptr_t OnRightClickObjectRva = 0x001F8660;
        static constexpr std::uintptr_t AutoLootRva = 0x000C1FA0;
        static constexpr std::uintptr_t GetLootSlotsRva = 0x000C2260;
        static constexpr std::uintptr_t ObjectManagerRootRva = 0x00741414;
        static constexpr std::uintptr_t FirstObjectOffset = 0x000000AC;
        static constexpr std::uintptr_t DescriptorPointerOffset = 0x00000008;
        static constexpr std::uintptr_t ObjectTypeOffset = 0x00000014;
        static constexpr std::uintptr_t ObjectGuidOffset = 0x00000030;
        static constexpr std::uintptr_t ObjectNextOffset = 0x0000003C;
        static constexpr std::uintptr_t ObjectEntryDescriptorOffset = 0x0000000C;
        static constexpr std::uintptr_t GameObjectXDescriptorOffset = 0x0000003C;
        static constexpr std::uintptr_t GameObjectYDescriptorOffset = 0x00000040;
        static constexpr std::uintptr_t GameObjectZDescriptorOffset = 0x00000044;
        static constexpr std::uint32_t GameObjectType = 5;

        static constexpr float InteractionDistance = 5.5f;
        static constexpr float LiveObjectArrivalDistance = 4.25f;
        static constexpr float CloseObjectDirectApproachDistance = 11.0f;
        static constexpr float CloseObjectStandOffDistance = 3.75f;
        static constexpr float CloseObjectApproachPrecision = 0.65f;
        static constexpr float CloseObjectProgressThreshold = 0.35f;
        static constexpr std::uint64_t CloseObjectReissueTicks = 7;
        static constexpr std::uint64_t CloseObjectStallTicks = 14;
        static constexpr int MaximumCloseObjectApproachAttempts = 4;
        static constexpr float MaximumVisibleObjectDistance = 180.0f;
        static constexpr std::uint64_t InteractionRetryTicks = 8;
        static constexpr std::uint64_t AutoLootDelayTicks = 2;
        static constexpr std::uint64_t InteractionSettleTicks = 7;
        static constexpr std::uint64_t MaximumObjectiveTicks = 1600;
        static constexpr int MaximumInteractionAttempts = 3;
        static constexpr int MaximumSearchCycles = 4;

        struct LiveGameObject
        {
            std::uintptr_t address = 0;
            std::uint64_t guid = 0;
            std::uint32_t entryId = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            float distance = std::numeric_limits<float>::infinity();
        };

        ObjectiveExecutorState state_ = ObjectiveExecutorState::Idle;
        const QuestProfile* profile_ = nullptr;
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> navigator_{};
        ObjectiveDefensiveCombatGuard defense_{};

        std::uint64_t objectiveStartTick_ = 0;
        std::uint64_t targetGuid_ = 0;
        std::uint64_t lastInteractionTick_ = 0;
        int interactionAttempts_ = 0;
        int autoLootAttempts_ = 0;
        int maxLootSlotsSeen_ = 0;
        bool interactionIssued_ = false;
        bool navigatingSearchPoint_ = false;
        bool defenseOwnedPreviousTick_ = false;
        bool directApproachActive_ = false;
        int directApproachAttempts_ = 0;
        std::uint64_t directApproachLastCommandTick_ = 0;
        std::uint64_t directApproachLastProgressTick_ = 0;
        float directApproachBestDistance_ = std::numeric_limits<float>::infinity();

        std::set<std::uint64_t> consumedGuids_{};
        std::vector<bool> visitedSearchPoints_{};
        std::size_t activeSearchPoint_ = static_cast<std::size_t>(-1);
        int searchCycles_ = 0;

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
                std::string("CollectWorldItemExecutor state: ") +
                StateNameInternal(state_) + " -> " + StateNameInternal(next));
            state_ = next;
        }

        template <typename T>
        static bool ReadValue(std::uintptr_t address, T& value)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (address == 0 ||
                VirtualQuery(reinterpret_cast<LPCVOID>(address), &info, sizeof(info)) == 0 ||
                info.State != MEM_COMMIT ||
                (info.Protect & PAGE_GUARD) != 0 ||
                (info.Protect & PAGE_NOACCESS) != 0)
                return false;

            const std::uintptr_t end =
                reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
            if (address + sizeof(T) > end)
                return false;

            std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
            return true;
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

        static float Distance2D(const Objects::PlayerState& player, float x, float y)
        {
            const float dx = player.x - x;
            const float dy = player.y - y;
            return std::sqrt(dx * dx + dy * dy);
        }

        static bool ComputeStandOffDestination(
            const Objects::PlayerState& player,
            const LiveGameObject& object,
            float standOff,
            float& outX,
            float& outY,
            float& outZ)
        {
            const float dx = player.x - object.x;
            const float dy = player.y - object.y;
            const float dz = player.z - object.z;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (!std::isfinite(length) || length <= 0.05f)
                return false;

            const float scale = standOff / length;
            outX = object.x + dx * scale;
            outY = object.y + dy * scale;
            outZ = object.z + dz * scale;
            return std::isfinite(outX) && std::isfinite(outY) && std::isfinite(outZ);
        }

        static std::string Hex64(std::uint64_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::uppercase << std::hex
                   << std::setw(16) << std::setfill('0') << value;
            return stream.str();
        }

        static bool ObjectManagerFirst(std::uint32_t& first)
        {
            std::uint32_t manager = 0;
            if (!ReadValue(Wow5875::Client::Base() + ObjectManagerRootRva, manager) ||
                manager == 0 || (manager & 1u) != 0)
                return false;

            return ReadValue(static_cast<std::uintptr_t>(manager) + FirstObjectOffset, first);
        }

        static bool ReadGameObject(
            std::uintptr_t address,
            const Objects::PlayerState& player,
            LiveGameObject& result)
        {
            std::uint32_t type = 0;
            std::uint64_t guid = 0;
            std::uint32_t descriptor = 0;
            std::uint32_t entry = 0;
            float x = 0.0f, y = 0.0f, z = 0.0f;

            if (!ReadValue(address + ObjectTypeOffset, type) || type != GameObjectType ||
                !ReadValue(address + ObjectGuidOffset, guid) || guid == 0 ||
                !ReadValue(address + DescriptorPointerOffset, descriptor) || descriptor == 0 ||
                !ReadValue(static_cast<std::uintptr_t>(descriptor) + ObjectEntryDescriptorOffset, entry) ||
                !ReadValue(static_cast<std::uintptr_t>(descriptor) + GameObjectXDescriptorOffset, x) ||
                !ReadValue(static_cast<std::uintptr_t>(descriptor) + GameObjectYDescriptorOffset, y) ||
                !ReadValue(static_cast<std::uintptr_t>(descriptor) + GameObjectZDescriptorOffset, z))
                return false;

            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                return false;

            result.address = address;
            result.guid = guid;
            result.entryId = entry;
            result.x = x;
            result.y = y;
            result.z = z;
            result.distance = Distance2D(player, x, y);
            return std::isfinite(result.distance);
        }

        bool FindNearestObject(
            const Objects::PlayerState& player,
            LiveGameObject& result) const
        {
            if (profile_ == nullptr)
                return false;

            std::uint32_t current = 0;
            if (!ObjectManagerFirst(current))
                return false;

            bool found = false;
            LiveGameObject best{};
            for (int i = 0; i < 4096; ++i)
            {
                if (current == 0 || (current & 1u) != 0)
                    break;

                LiveGameObject candidate{};
                if (ReadGameObject(static_cast<std::uintptr_t>(current), player, candidate) &&
                    candidate.entryId == profile_->objective.objectEntry &&
                    candidate.distance <= MaximumVisibleObjectDistance &&
                    consumedGuids_.find(candidate.guid) == consumedGuids_.end() &&
                    (!found || candidate.distance < best.distance))
                {
                    best = candidate;
                    found = true;
                }

                std::uint32_t next = 0;
                if (!ReadValue(static_cast<std::uintptr_t>(current) + ObjectNextOffset, next) ||
                    next == current)
                    break;
                current = next;
            }

            if (found)
                result = best;
            return found;
        }

        static bool FindObjectByGuid(
            std::uint64_t guid,
            const Objects::PlayerState& player,
            LiveGameObject& result)
        {
            if (guid == 0)
                return false;

            std::uint32_t current = 0;
            if (!ObjectManagerFirst(current))
                return false;

            for (int i = 0; i < 4096; ++i)
            {
                if (current == 0 || (current & 1u) != 0)
                    break;

                std::uint64_t currentGuid = 0;
                if (!ReadValue(static_cast<std::uintptr_t>(current) + ObjectGuidOffset, currentGuid))
                    break;
                if (currentGuid == guid)
                    return ReadGameObject(static_cast<std::uintptr_t>(current), player, result);

                std::uint32_t next = 0;
                if (!ReadValue(static_cast<std::uintptr_t>(current) + ObjectNextOffset, next) ||
                    next == current)
                    break;
                current = next;
            }
            return false;
        }

        const PlannerQuestLogEntry* FindLiveQuest(const QuestPlannerSnapshot& snapshot) const
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

        std::vector<ObjectiveDestination> SearchPoints() const
        {
            if (profile_ == nullptr)
                return {};
            if (!profile_->searchDestinations.empty())
                return profile_->searchDestinations;
            if (profile_->destination.valid)
                return {profile_->destination};
            return {};
        }

        bool StartNavigation(
            const Objects::WorldState& world,
            std::uint64_t tick,
            const Navigation::NavPoint& destination,
            float arrival,
            const std::string& label,
            bool searchPoint)
        {
            navigator_ = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            if (!navigator_->Start(
                    world.player,
                    tick,
                    destination,
                    1,
                    arrival,
                    label))
            {
                navigator_.reset();
                return false;
            }

            navigatingSearchPoint_ = searchPoint;
            SetState(ObjectiveExecutorState::Navigating);
            return true;
        }

        bool StartNextSearchPoint(const Objects::WorldState& world, std::uint64_t tick)
        {
            const auto points = SearchPoints();
            if (points.empty())
                return false;

            if (visitedSearchPoints_.size() != points.size())
                visitedSearchPoints_.assign(points.size(), false);

            bool anyUnvisited = false;
            for (bool visited : visitedSearchPoints_)
                anyUnvisited = anyUnvisited || !visited;

            if (!anyUnvisited)
            {
                ++searchCycles_;
                if (searchCycles_ >= MaximumSearchCycles)
                    return false;
                std::fill(visitedSearchPoints_.begin(), visitedSearchPoints_.end(), false);
            }

            std::size_t bestIndex = static_cast<std::size_t>(-1);
            float bestDistance = std::numeric_limits<float>::infinity();
            for (std::size_t i = 0; i < points.size(); ++i)
            {
                if (visitedSearchPoints_[i] || !points[i].valid)
                    continue;
                const float distance = Distance2D(world.player, points[i].x, points[i].y);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestIndex = i;
                }
            }

            if (bestIndex == static_cast<std::size_t>(-1))
                return false;

            activeSearchPoint_ = bestIndex;
            const auto& point = points[bestIndex];
            Debug::Logger::Info("================================");
            Debug::Logger::Info("COLLECT WORLD ITEM 12B.1: SEARCH LEG");
            Debug::Logger::Info(
                "QuestId=" + std::to_string(profile_->questId) +
                " hotspot=" + std::to_string(bestIndex + 1) +
                "/" + std::to_string(points.size()) +
                " label=" + std::string(point.label));
            Debug::Logger::Info("================================");

            return StartNavigation(
                world,
                tick,
                Navigation::NavPoint{point.x, point.y, point.z},
                point.arrivalDistance > 0.0f ? point.arrivalDistance : 22.0f,
                point.label,
                true);
        }

        void ResetDirectApproach()
        {
            directApproachActive_ = false;
            directApproachAttempts_ = 0;
            directApproachLastCommandTick_ = 0;
            directApproachLastProgressTick_ = 0;
            directApproachBestDistance_ = std::numeric_limits<float>::infinity();
        }

        bool IssueDirectApproach(
            const Objects::WorldState& world,
            const LiveGameObject& object,
            std::uint64_t tick,
            const char* reason)
        {
            float destinationX = 0.0f;
            float destinationY = 0.0f;
            float destinationZ = 0.0f;
            if (!ComputeStandOffDestination(
                    world.player,
                    object,
                    CloseObjectStandOffDistance,
                    destinationX,
                    destinationY,
                    destinationZ))
                return false;

            if (!Bot::ClickToMoveController::MoveTo(
                    world.player,
                    destinationX,
                    destinationY,
                    destinationZ,
                    CloseObjectApproachPrecision))
                return false;

            const bool firstCommand = !directApproachActive_;
            directApproachActive_ = true;
            ++directApproachAttempts_;
            directApproachLastCommandTick_ = tick;
            if (firstCommand)
            {
                directApproachLastProgressTick_ = tick;
                directApproachBestDistance_ = object.distance;
            }

            Debug::Logger::Info(
                "COLLECT WORLD ITEM 12B.1: CLOSE OBJECT DIRECT APPROACH"
                " reason=" + std::string(reason) +
                " distance=" + std::to_string(object.distance) +
                " attempt=" + std::to_string(directApproachAttempts_) +
                "/" + std::to_string(MaximumCloseObjectApproachAttempts));
            return true;
        }

        bool StartCloseObjectApproach(
            const Objects::WorldState& world,
            const LiveGameObject& object,
            std::uint64_t tick)
        {
            navigator_.reset();
            navigatingSearchPoint_ = false;
            ResetDirectApproach();
            SetState(ObjectiveExecutorState::Navigating);
            return IssueDirectApproach(world, object, tick, "initial");
        }

        bool BeginObject(
            const Objects::WorldState& world,
            const LiveGameObject& object,
            std::uint64_t tick)
        {
            targetGuid_ = object.guid;
            interactionAttempts_ = 0;
            autoLootAttempts_ = 0;
            maxLootSlotsSeen_ = 0;
            interactionIssued_ = false;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("COLLECT WORLD ITEM 12B.1: LIVE OBJECT");
            Debug::Logger::Info(
                "Entry=" + std::to_string(object.entryId) +
                " guid=" + Hex64(object.guid) +
                " distance=" + std::to_string(object.distance));
            if (profile_->databaseDerived)
            {
                Debug::Logger::Info(
                    "QuestDB semantics: gameObjectType=" +
                    std::to_string(profile_->gameObjectType) +
                    " lootId=" + std::to_string(profile_->gameObjectLootId));
            }
            Debug::Logger::Info("================================");

            if (object.distance > InteractionDistance)
            {
                if (object.distance <= CloseObjectDirectApproachDistance)
                {
                    if (StartCloseObjectApproach(world, object, tick))
                        return true;

                    Debug::Logger::Info(
                        "COLLECT WORLD ITEM 12B.1: close-object direct approach could not start; "
                        "falling back to NavMesh.");
                }

                ResetDirectApproach();
                return StartNavigation(
                    world,
                    tick,
                    Navigation::NavPoint{object.x, object.y, object.z},
                    LiveObjectArrivalDistance,
                    "CollectWorldItem live object",
                    false);
            }

            navigator_.reset();
            navigatingSearchPoint_ = false;
            ResetDirectApproach();
            SetState(ObjectiveExecutorState::Executing);
            return true;
        }

        static bool InteractExactObject(const LiveGameObject& object)
        {
            const auto address = Wow5875::Client::Base() + OnRightClickObjectRva;
            if (!IsExecutable(address))
                return false;

            using Function = void (__thiscall*)(std::uint32_t, int);
            const auto function = reinterpret_cast<Function>(address);
            bool onGameThread = false;
            bool invoked = false;
            const bool dispatched = GameThreadDispatcher::Invoke([&]()
            {
                onGameThread = GameThreadDispatcher::IsGameThread();
                std::uint32_t type = 0;
                std::uint64_t guid = 0;
                if (!ReadValue(object.address + ObjectTypeOffset, type) ||
                    !ReadValue(object.address + ObjectGuidOffset, guid) ||
                    type != GameObjectType || guid != object.guid)
                    return;
                function(static_cast<std::uint32_t>(object.address), 1);
                invoked = true;
            });
            return dispatched && onGameThread && invoked;
        }

        static bool TryNativeAutoLoot(int& slotsBefore)
        {
            slotsBefore = -1;
            const auto autoLootAddress = Wow5875::Client::Base() + AutoLootRva;
            const auto getLootSlotsAddress = Wow5875::Client::Base() + GetLootSlotsRva;
            if (!IsExecutable(autoLootAddress) || !IsExecutable(getLootSlotsAddress))
                return false;

            using AutoLootFunction = void (__stdcall*)();
            using GetLootSlotsFunction = int (__stdcall*)();
            const auto autoLoot = reinterpret_cast<AutoLootFunction>(autoLootAddress);
            const auto getLootSlots = reinterpret_cast<GetLootSlotsFunction>(getLootSlotsAddress);

            bool onGameThread = false;
            const bool dispatched = GameThreadDispatcher::Invoke([&]()
            {
                onGameThread = GameThreadDispatcher::IsGameThread();
                slotsBefore = getLootSlots();
                if (slotsBefore > 0)
                    autoLoot();
            });
            return dispatched && onGameThread;
        }

        void Complete()
        {
            navigator_.reset();
            Debug::Logger::Info("================================");
            Debug::Logger::Info("OBJECTIVE 12B.2: COLLECT WORLD ITEM COMPLETE");
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
            Debug::Logger::Info("OBJECTIVE 12B.2: COLLECT WORLD ITEM FAILED");
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info("================================");
            SetState(ObjectiveExecutorState::Failed);
        }

        void MoveToNextWork(const Objects::WorldState& world, std::uint64_t tick)
        {
            LiveGameObject next{};
            if (FindNearestObject(world.player, next))
            {
                BeginObject(world, next, tick);
                return;
            }

            if (!StartNextSearchPoint(world, tick))
                Fail("all configured world-item search points were exhausted without quest completion.");
        }

    public:
        bool Supports(const QuestProfile& profile) const override
        {
            return profile.objective.type == QuestObjectiveType::CollectWorldItem &&
                   profile.objective.objectEntry != 0;
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
            targetGuid_ = 0;
            lastInteractionTick_ = 0;
            interactionAttempts_ = 0;
            autoLootAttempts_ = 0;
            maxLootSlotsSeen_ = 0;
            interactionIssued_ = false;
            navigatingSearchPoint_ = false;
            defenseOwnedPreviousTick_ = false;
            ResetDirectApproach();
            consumedGuids_.clear();
            visitedSearchPoints_.clear();
            activeSearchPoint_ = static_cast<std::size_t>(-1);
            searchCycles_ = 0;
            navigator_.reset();
            defense_.Reset(world, "CollectWorldItem quest " + std::to_string(profile.questId));

            Debug::Logger::Info("================================");
            Debug::Logger::Info("OBJECTIVE EXECUTOR 12B.2: START");
            Debug::Logger::Info(
                "Quest: " + std::to_string(profile.questId) + " " + profile.title);
            Debug::Logger::Info(
                "Type: CollectWorldItem objectEntry=" +
                std::to_string(profile.objective.objectEntry) +
                " itemId=" + std::to_string(profile.objective.itemId) +
                " required=" + std::to_string(profile.objective.requiredCount));
            Debug::Logger::Info("================================");

            LiveGameObject object{};
            if (FindNearestObject(world.player, object))
                return BeginObject(world, object, tick);

            if (!StartNextSearchPoint(world, tick))
            {
                Fail("no configured search destination is available for CollectWorldItem.");
                return false;
            }
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
            if (SelectedObjectiveComplete(*liveQuest, *profile_))
            {
                Complete();
                return;
            }
            if (world.player.health == 0)
            {
                Fail("player health reached zero; death recovery is not implemented.");
                return;
            }
            if (tick > objectiveStartTick_ + MaximumObjectiveTicks)
            {
                Fail("CollectWorldItem objective timeout reached.");
                return;
            }

            const auto defenseUpdate = defense_.Update(world, combat, navigator_.get(), 0, tick);
            if (defenseUpdate == ObjectiveDefenseUpdate::Failed)
            {
                Fail("defensive combat/recovery failed during CollectWorldItem objective.");
                return;
            }
            if (defenseUpdate == ObjectiveDefenseUpdate::OwnsControl)
            {
                defenseOwnedPreviousTick_ = true;
                return;
            }
            if (defenseOwnedPreviousTick_)
            {
                defenseOwnedPreviousTick_ = false;
                ResetDirectApproach();
                Debug::Logger::Info(
                    "COLLECT WORLD ITEM 12B.1: defensive combat resolved; resetting local movement watchdog.");
            }

            if (state_ == ObjectiveExecutorState::Navigating)
            {
                LiveGameObject object{};

                if (directApproachActive_)
                {
                    if (targetGuid_ == 0 || !FindObjectByGuid(targetGuid_, world.player, object))
                    {
                        ResetDirectApproach();
                        targetGuid_ = 0;
                        MoveToNextWork(world, tick);
                        return;
                    }

                    if (object.distance <= InteractionDistance)
                    {
                        ResetDirectApproach();
                        SetState(ObjectiveExecutorState::Executing);
                        return;
                    }

                    if (object.distance + CloseObjectProgressThreshold < directApproachBestDistance_)
                    {
                        directApproachBestDistance_ = object.distance;
                        directApproachLastProgressTick_ = tick;
                    }

                    const bool stalled =
                        tick >= directApproachLastProgressTick_ &&
                        tick - directApproachLastProgressTick_ >= CloseObjectStallTicks;

                    const bool refreshDue =
                        tick >= directApproachLastCommandTick_ &&
                        tick - directApproachLastCommandTick_ >= CloseObjectReissueTicks;

                    if (stalled || refreshDue)
                    {
                        if (directApproachAttempts_ < MaximumCloseObjectApproachAttempts)
                        {
                            if (IssueDirectApproach(
                                    world,
                                    object,
                                    tick,
                                    stalled ? "stall recovery" : "periodic refresh"))
                                return;
                        }

                        Debug::Logger::Info(
                            "COLLECT WORLD ITEM 12B.1: CLOSE OBJECT APPROACH EXHAUSTED; "
                            "falling back to NavMesh from live position.");
                        ResetDirectApproach();

                        if (!StartNavigation(
                                world,
                                tick,
                                Navigation::NavPoint{object.x, object.y, object.z},
                                LiveObjectArrivalDistance,
                                "CollectWorldItem live object fallback",
                                false))
                        {
                            consumedGuids_.insert(object.guid);
                            targetGuid_ = 0;
                            MoveToNextWork(world, tick);
                        }
                        return;
                    }

                    return;
                }

                if (navigatingSearchPoint_ && FindNearestObject(world.player, object))
                {
                    navigator_.reset();
                    BeginObject(world, object, tick);
                    return;
                }

                if (!navigatingSearchPoint_ && targetGuid_ != 0 &&
                    FindObjectByGuid(targetGuid_, world.player, object) &&
                    object.distance <= InteractionDistance)
                {
                    navigator_.reset();
                    SetState(ObjectiveExecutorState::Executing);
                    return;
                }

                if (!navigator_)
                {
                    MoveToNextWork(world, tick);
                    return;
                }

                navigator_->Update(world.player, tick);
                if (navigator_->Failed())
                {
                    navigator_.reset();
                    if (navigatingSearchPoint_ &&
                        activeSearchPoint_ < visitedSearchPoints_.size())
                        visitedSearchPoints_[activeSearchPoint_] = true;
                    else if (targetGuid_ != 0)
                        consumedGuids_.insert(targetGuid_);
                    targetGuid_ = 0;
                    MoveToNextWork(world, tick);
                    return;
                }

                if (navigator_->Arrived())
                {
                    navigator_.reset();
                    if (navigatingSearchPoint_)
                    {
                        if (activeSearchPoint_ < visitedSearchPoints_.size())
                            visitedSearchPoints_[activeSearchPoint_] = true;
                        LiveGameObject arrivedObject{};
                        if (FindNearestObject(world.player, arrivedObject))
                            BeginObject(world, arrivedObject, tick);
                        else
                            MoveToNextWork(world, tick);
                    }
                    else
                    {
                        SetState(ObjectiveExecutorState::Executing);
                    }
                }
                return;
            }

            if (state_ != ObjectiveExecutorState::Executing)
                return;

            if (interactionIssued_)
            {
                if (tick >= lastInteractionTick_ + AutoLootDelayTicks && autoLootAttempts_ < 3)
                {
                    int slots = -1;
                    if (TryNativeAutoLoot(slots))
                    {
                        ++autoLootAttempts_;
                        if (slots > maxLootSlotsSeen_)
                            maxLootSlotsSeen_ = slots;
                        Debug::Logger::Info(
                            "COLLECT WORLD ITEM 12B: native AutoLoot probe slots=" +
                            std::to_string(slots));
                    }
                }

                if (tick >= lastInteractionTick_ + InteractionSettleTicks)
                {
                    LiveGameObject stillPresent{};
                    const bool objectStillPresent =
                        targetGuid_ != 0 &&
                        FindObjectByGuid(targetGuid_, world.player, stillPresent);

                    /*
                     * Phase 12B: do not blacklist a world object merely
                     * because one native call returned. The old behavior
                     * caused Cactus Apple to advance after slots=0 without
                     * ever retrying the same GUID. Disappearance or an
                     * observed loot window is treated as positive evidence;
                     * otherwise retry the exact GUID within the existing
                     * bounded interaction-attempt budget.
                     */
                    if (objectStillPresent &&
                        maxLootSlotsSeen_ <= 0 &&
                        interactionAttempts_ < MaximumInteractionAttempts)
                    {
                        interactionIssued_ = false;
                        autoLootAttempts_ = 0;
                        maxLootSlotsSeen_ = 0;
                        Debug::Logger::Info("================================");
                        Debug::Logger::Info(
                            "COLLECT WORLD ITEM 12B: INTERACTION NOT CONFIRMED - RETRY SAME GUID");
                        Debug::Logger::Info(
                            "Entry=" + std::to_string(stillPresent.entryId) +
                            " guid=" + Hex64(stillPresent.guid) +
                            " nextAttempt=" +
                            std::to_string(interactionAttempts_ + 1) +
                            "/" + std::to_string(MaximumInteractionAttempts));
                        Debug::Logger::Info("================================");
                        return;
                    }

                    consumedGuids_.insert(targetGuid_);
                    targetGuid_ = 0;
                    interactionIssued_ = false;
                    interactionAttempts_ = 0;
                    autoLootAttempts_ = 0;
                    maxLootSlotsSeen_ = 0;
                    MoveToNextWork(world, tick);
                }
                return;
            }

            LiveGameObject object{};
            if (targetGuid_ == 0 || !FindObjectByGuid(targetGuid_, world.player, object))
            {
                targetGuid_ = 0;
                MoveToNextWork(world, tick);
                return;
            }

            if (object.distance > InteractionDistance)
            {
                BeginObject(world, object, tick);
                return;
            }

            if (lastInteractionTick_ != 0 && tick < lastInteractionTick_ + InteractionRetryTicks)
                return;

            if (interactionAttempts_ >= MaximumInteractionAttempts)
            {
                consumedGuids_.insert(targetGuid_);
                targetGuid_ = 0;
                MoveToNextWork(world, tick);
                return;
            }

            if (!InteractExactObject(object))
            {
                ++interactionAttempts_;
                lastInteractionTick_ = tick;
                return;
            }

            ++interactionAttempts_;
            lastInteractionTick_ = tick;
            interactionIssued_ = true;
            autoLootAttempts_ = 0;
            maxLootSlotsSeen_ = 0;
            Debug::Logger::Info("================================");
            Debug::Logger::Info("COLLECT WORLD ITEM 12B: EXACT GUID INTERACTION");
            Debug::Logger::Info(
                "Entry=" + std::to_string(object.entryId) +
                " guid=" + Hex64(object.guid) +
                " attempt=" + std::to_string(interactionAttempts_) +
                "/" + std::to_string(MaximumInteractionAttempts));
            Debug::Logger::Info("================================");
        }

        bool OwnsControl() const override
        {
            return state_ == ObjectiveExecutorState::Navigating ||
                   state_ == ObjectiveExecutorState::Executing;
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
