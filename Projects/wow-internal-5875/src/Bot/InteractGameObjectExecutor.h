#pragma once

#include "GameThreadDispatcher.h"
#include "ClickToMoveController.h"
#include "IObjectiveExecutor.h"
#include "QuestObjectiveDispatchPolicy.h"
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
#include <sstream>
#include <string>

namespace Bot
{
    class InteractGameObjectExecutor : public IObjectiveExecutor
    {
    public:
        struct ObservedGameObject
        {
            std::uint64_t guid = 0;
            std::uint32_t entry = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            float distance = 0.0f;
        };

    private:
        static constexpr std::uintptr_t OnRightClickObjectRva = 0x001F8660;
        static constexpr std::uintptr_t ObjectManagerRootRva = 0x00741414;
        static constexpr std::uintptr_t FirstObjectOffset = 0x000000AC;
        static constexpr std::uintptr_t DescriptorPointerOffset = 0x00000008;
        static constexpr std::uintptr_t ObjectTypeOffset = 0x00000014;
        static constexpr std::uintptr_t ObjectGuidOffset = 0x00000030;
        static constexpr std::uintptr_t ObjectNextOffset = 0x0000003C;
        // 1.12.1 update-field layout (OBJECT_FIELD_ENTRY = field 0x03;
        // GAMEOBJECT_POS_X/Y/Z = fields 0x0F/0x10/0x11). The descriptor
        // pointer is the authoritative source for both entry and world XYZ.
        static constexpr std::uintptr_t ObjectEntryDescriptorOffset = 0x0000000C;
        static constexpr std::uintptr_t GameObjectXDescriptorOffset = 0x0000003C;
        static constexpr std::uintptr_t GameObjectYDescriptorOffset = 0x00000040;
        static constexpr std::uintptr_t GameObjectZDescriptorOffset = 0x00000044;
        static constexpr std::uint32_t GameObjectType = 5;
        static constexpr std::uint32_t KalimdorMapId = 1;

        static constexpr float InteractionDistance = 5.5f;
        static constexpr float LiveObjectNavArrivalDistance = 4.25f;
        static constexpr float MaximumLiveObjectDistance = 180.0f;
        static constexpr std::uint64_t InteractionRetryTicks = 12;
        static constexpr std::uint64_t InteractionCompletionGraceTicks = 40;
        static constexpr std::uint64_t ObjectMissingGraceTicks = 16;
        static constexpr std::uint64_t MaximumObjectiveTicks = 1200;
        static constexpr int MaximumInteractionAttempts = 6;

        // Phase 11D.2.7 anti-idle policy. At the normal planner pulse this is
        // roughly seven seconds without meaningful player displacement. The
        // generic follower already has its own stuck detector; this outer
        // watchdog prevents an objective-specific navigation leg from owning
        // the bot for a long time when a cave corridor cannot be followed.
        static constexpr std::uint64_t NavigationNoProgressTicks = 28;
        static constexpr std::uint64_t NavigationStatusTicks = 20;
        static constexpr float NavigationProgressDistance = 0.75f;

        // A failed deep-cave leg must not send the character all the way back
        // to the route-group anchor. Once already inside/near Burning Blade
        // Coven, retries start from the live position toward the profiled
        // objective search point.
        static constexpr float DirectSearchRetryDistance = 140.0f;

        // The classic Thazz'ril interaction requires getting very close to
        // the pick. NavMesh remains authoritative for long-range movement;
        // these are short, bounded final-visibility nudges only after the bot
        // is already at the profiled objective area.
        static constexpr float FinalVisibilityProbeDistance = 18.0f;
        static constexpr float FinalVisibilityProbeStep = 2.4f;
        static constexpr std::uint64_t FinalVisibilityProbeRetryTicks = 8;
        static constexpr int MaximumFinalVisibilityProbes = 4;

        // Phase 13D.5: a QuestDB gameobject spawn is a search seed, not a
        // guarantee that the exact XYZ is a good player destination. Objects
        // can sit beside tents, rocks, camp geometry, or on tiny/non-walkable
        // polygons. If the exact seed route stalls/fails, probe a bounded ring
        // of nearby approach points. Every candidate is still validated by the
        // shared NavMesh follower; there are no quest-specific waypoints.
        static constexpr int SearchEnvelopeDirections = 8;
        static constexpr int SearchEnvelopeRadiusCount = 3;
        static constexpr int MaximumSearchEnvelopeCandidates =
            SearchEnvelopeDirections * SearchEnvelopeRadiusCount;
        static constexpr float SearchEnvelopeArrivalDistance = 7.5f;
        static constexpr float SearchEnvelopePi = 3.14159265358979323846f;

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
        std::string failureReason_{};
        const QuestProfile* profile_ = nullptr;
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> navigator_{};
        ObjectiveDefensiveCombatGuard defense_{};

        std::uint64_t objectiveStartTick_ = 0;
        std::uint64_t targetGuid_ = 0;
        std::uint64_t lastInteractionTick_ = 0;
        std::uint64_t objectMissingSinceTick_ = 0;
        int interactionAttempts_ = 0;
        bool navigatingSearchSeed_ = false;
        bool searchSeedVisited_ = false;
        bool routeAnchorActive_ = false;
        bool routeAnchorVisited_ = false;

        std::uint64_t navigationStartTick_ = 0;
        std::uint64_t navigationLastMovementTick_ = 0;
        std::uint64_t navigationLastStatusTick_ = 0;
        float navigationBaselineX_ = 0.0f;
        float navigationBaselineY_ = 0.0f;

        bool defenseOwnedPreviousTick_ = false;
        int finalVisibilityProbeAttempts_ = 0;
        std::uint64_t lastFinalVisibilityProbeTick_ = 0;
        bool scanDiagnosticLogged_ = false;

        int searchEnvelopeCandidateIndex_ = 0;
        bool searchEnvelopeCandidateActive_ = false;
        bool searchEnvelopeExhausted_ = false;

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
                std::string("InteractGameObjectExecutor state: ") +
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
            {
                return false;
            }

            const std::uintptr_t regionEnd =
                reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
            if (address + sizeof(T) > regionEnd)
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
            {
                return false;
            }

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

        static std::string Hex64(std::uint64_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::uppercase << std::hex
                   << std::setw(16) << std::setfill('0') << value;
            return stream.str();
        }

        static float Distance2D(
            const Objects::PlayerState& player,
            float x,
            float y)
        {
            const float dx = player.x - x;
            const float dy = player.y - y;
            return std::sqrt(dx * dx + dy * dy);
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
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;

            if (!ReadValue(address + ObjectTypeOffset, type) ||
                type != GameObjectType ||
                !ReadValue(address + ObjectGuidOffset, guid) ||
                guid == 0 ||
                !ReadValue(address + DescriptorPointerOffset, descriptor) ||
                descriptor == 0 ||
                !ReadValue(static_cast<std::uintptr_t>(descriptor) + ObjectEntryDescriptorOffset, entry) ||
                !ReadValue(static_cast<std::uintptr_t>(descriptor) + GameObjectXDescriptorOffset, x) ||
                !ReadValue(static_cast<std::uintptr_t>(descriptor) + GameObjectYDescriptorOffset, y) ||
                !ReadValue(static_cast<std::uintptr_t>(descriptor) + GameObjectZDescriptorOffset, z))
            {
                return false;
            }

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

        static bool ObjectManagerFirst(std::uint32_t& first)
        {
            std::uint32_t manager = 0;
            if (!ReadValue(Wow5875::Client::Base() + ObjectManagerRootRva, manager) ||
                manager == 0 || (manager & 1u) != 0)
            {
                return false;
            }

            return ReadValue(
                static_cast<std::uintptr_t>(manager) + FirstObjectOffset,
                first);
        }

        static bool FindGameObjectByGuid(
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
                {
                    break;
                }
                current = next;
            }

            return false;
        }

        static void LogGameObjectScanDiagnostic(
            std::uint32_t wantedEntry,
            const Objects::PlayerState& player)
        {
            std::uint32_t current = 0;
            if (!ObjectManagerFirst(current))
            {
                Debug::Logger::Info(
                    "INTERACT GAMEOBJECT 11D.2.7: SCAN DIAGNOSTIC object-manager root unavailable");
                return;
            }

            int typedGameObjects = 0;
            int readableGameObjects = 0;
            int matchingEntryCount = 0;
            float nearestReadableDistance = std::numeric_limits<float>::infinity();
            std::uint32_t nearestReadableEntry = 0;

            for (int i = 0; i < 4096; ++i)
            {
                if (current == 0 || (current & 1u) != 0)
                    break;

                std::uint32_t type = 0;
                if (ReadValue(static_cast<std::uintptr_t>(current) + ObjectTypeOffset, type) &&
                    type == GameObjectType)
                {
                    ++typedGameObjects;
                    LiveGameObject candidate{};
                    if (ReadGameObject(static_cast<std::uintptr_t>(current), player, candidate))
                    {
                        ++readableGameObjects;
                        if (candidate.entryId == wantedEntry)
                            ++matchingEntryCount;
                        if (candidate.distance < nearestReadableDistance)
                        {
                            nearestReadableDistance = candidate.distance;
                            nearestReadableEntry = candidate.entryId;
                        }
                    }
                }

                std::uint32_t next = 0;
                if (!ReadValue(static_cast<std::uintptr_t>(current) + ObjectNextOffset, next) ||
                    next == current)
                {
                    break;
                }
                current = next;
            }

            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                "INTERACT GAMEOBJECT 11D.2.7: SCAN DIAGNOSTIC");
            Debug::Logger::Info(
                "GameObject typed=" + std::to_string(typedGameObjects) +
                " descriptor-readable=" + std::to_string(readableGameObjects) +
                " matchingEntry=" + std::to_string(matchingEntryCount));
            if (std::isfinite(nearestReadableDistance))
            {
                Debug::Logger::Info(
                    "Nearest readable GO entry=" +
                    std::to_string(nearestReadableEntry) +
                    " distance=" + std::to_string(nearestReadableDistance));
            }
            Debug::Logger::Info("================================");
        }

        static bool FindNearestGameObject(
            std::uint32_t entryId,
            const Objects::PlayerState& player,
            LiveGameObject& result)
        {
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
                    candidate.entryId == entryId &&
                    candidate.distance <= MaximumLiveObjectDistance &&
                    (!found || candidate.distance < best.distance))
                {
                    best = candidate;
                    found = true;
                }

                std::uint32_t next = 0;
                if (!ReadValue(static_cast<std::uintptr_t>(current) + ObjectNextOffset, next) ||
                    next == current)
                {
                    break;
                }
                current = next;
            }

            if (found)
                result = best;
            return found;
        }

        const PlannerQuestLogEntry* FindLiveQuest(
            const QuestPlannerSnapshot& snapshot) const
        {
            if (profile_ == nullptr)
                return nullptr;

            for (const auto& entry : snapshot.quests)
            {
                const auto* mapped =
                    ValleyOfTrialsProfiles::Find(entry, snapshot.classToken);
                if (mapped != nullptr && mapped->questId == profile_->questId)
                    return &entry;
            }
            return nullptr;
        }

        static Navigation::NavPoint BurningBladeCovenVerifiedAnchor()
        {
            // Runtime-verified in Phase 11B/11B.8.2: the Yarrog corridor
            // reaches this part of Burning Blade Coven with a complete
            // non-partial Detour path. Use it as a route-group search anchor,
            // not as the interaction point. Live gameobject XYZ always wins.
            return Navigation::NavPoint{-58.1846f, -4220.6400f, 62.3418f};
        }

        static void StopMovement(const Objects::PlayerState& player)
        {
            Bot::ClickToMoveController::MoveTo(
                player,
                player.x,
                player.y,
                player.z,
                0.25f);
        }

        float ProfileDestinationDistance(const Objects::PlayerState& player) const
        {
            if (profile_ == nullptr || !profile_->destination.valid)
                return std::numeric_limits<float>::infinity();

            return Distance2D(
                player,
                profile_->destination.x,
                profile_->destination.y);
        }

        bool IssueFinalVisibilityProbe(
            const Objects::WorldState& world,
            std::uint64_t tick,
            const char* reason)
        {
            if (profile_ == nullptr || !profile_->destination.valid ||
                finalVisibilityProbeAttempts_ >= MaximumFinalVisibilityProbes)
            {
                return false;
            }

            if (lastFinalVisibilityProbeTick_ != 0 &&
                tick < lastFinalVisibilityProbeTick_ + FinalVisibilityProbeRetryTicks)
            {
                return false;
            }

            const float dx = profile_->destination.x - world.player.x;
            const float dy = profile_->destination.y - world.player.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (!std::isfinite(distance) || distance <= 0.10f ||
                distance > FinalVisibilityProbeDistance)
            {
                return false;
            }

            const float step = std::min(FinalVisibilityProbeStep, distance);
            const float scale = step / distance;
            const float targetX = world.player.x + dx * scale;
            const float targetY = world.player.y + dy * scale;

            navigator_.reset();
            StopMovement(world.player);

            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                "INTERACT GAMEOBJECT 11D.2.7: FINAL VISIBILITY PROBE");
            Debug::Logger::Info(std::string("Reason: ") + reason);
            Debug::Logger::Info(
                "Search-point distance=" + std::to_string(distance) +
                " probe=" + std::to_string(finalVisibilityProbeAttempts_ + 1) +
                "/" + std::to_string(MaximumFinalVisibilityProbes));
            Debug::Logger::Info(
                "Bounded CTM nudge target=(" + std::to_string(targetX) + "," +
                std::to_string(targetY) + "," + std::to_string(world.player.z) + ")");
            Debug::Logger::Info("================================");

            if (!Bot::ClickToMoveController::MoveTo(
                    world.player,
                    targetX,
                    targetY,
                    world.player.z,
                    0.50f))
            {
                return false;
            }

            ++finalVisibilityProbeAttempts_;
            lastFinalVisibilityProbeTick_ = tick;
            navigatingSearchSeed_ = false;
            searchSeedVisited_ = true;
            routeAnchorActive_ = false;
            objectMissingSinceTick_ = 0;
            ResetNavigationWatchdog(world.player, tick);
            SetState(ObjectiveExecutorState::Executing);
            return true;
        }

        void ResetNavigationWatchdog(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            navigationStartTick_ = tick;
            navigationLastMovementTick_ = tick;
            navigationLastStatusTick_ = tick;
            navigationBaselineX_ = player.x;
            navigationBaselineY_ = player.y;
        }

        static float SearchEnvelopeRadius(int radiusIndex)
        {
            switch (radiusIndex)
            {
                case 0: return 10.0f;
                case 1: return 18.0f;
                default: return 28.0f;
            }
        }

        static int SearchEnvelopeDirectionSlot(int directionIndex)
        {
            // Near side first, then fan left/right, then the far side.
            static constexpr int order[SearchEnvelopeDirections] =
                {0, 1, 7, 2, 6, 3, 5, 4};
            return order[directionIndex % SearchEnvelopeDirections];
        }

        bool StartNextSearchEnvelopeCandidate(
            const Objects::WorldState& world,
            std::uint64_t tick,
            const char* reason)
        {
            if (profile_ == nullptr || !profile_->destination.valid ||
                searchEnvelopeExhausted_)
            {
                return false;
            }

            const float towardPlayer = std::atan2(
                world.player.y - profile_->destination.y,
                world.player.x - profile_->destination.x);

            while (searchEnvelopeCandidateIndex_ < MaximumSearchEnvelopeCandidates)
            {
                const int candidate = searchEnvelopeCandidateIndex_++;
                const int radiusIndex = candidate / SearchEnvelopeDirections;
                const int directionIndex = candidate % SearchEnvelopeDirections;
                const int slot = SearchEnvelopeDirectionSlot(directionIndex);
                const float angle = towardPlayer +
                    (2.0f * SearchEnvelopePi * static_cast<float>(slot) /
                     static_cast<float>(SearchEnvelopeDirections));
                const float radius = SearchEnvelopeRadius(radiusIndex);

                const Navigation::NavPoint approach{
                    profile_->destination.x + std::cos(angle) * radius,
                    profile_->destination.y + std::sin(angle) * radius,
                    profile_->destination.z};

                Debug::Logger::Info("================================");
                Debug::Logger::Info(
                    "INTERACT GAMEOBJECT 13D.5: SEARCH ENVELOPE CANDIDATE");
                Debug::Logger::Info(std::string("Reason: ") + reason);
                Debug::Logger::Info(
                    "Candidate=" + std::to_string(candidate + 1) + "/" +
                    std::to_string(MaximumSearchEnvelopeCandidates) +
                    " radius=" + std::to_string(radius));
                Debug::Logger::Info(
                    "Approach=(" + std::to_string(approach.x) + "," +
                    std::to_string(approach.y) + "," +
                    std::to_string(approach.z) + ")");
                Debug::Logger::Info("================================");

                if (StartNavigation(
                        world,
                        tick,
                        approach,
                        SearchEnvelopeArrivalDistance,
                        "InteractGameObject reachable search-envelope approach",
                        true,
                        false))
                {
                    searchEnvelopeCandidateActive_ = true;
                    return true;
                }

                Debug::Logger::Info(
                    "INTERACT GAMEOBJECT 13D.5: candidate rejected before movement; trying next bounded approach.");
            }

            searchEnvelopeExhausted_ = true;
            searchEnvelopeCandidateActive_ = false;
            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                "INTERACT GAMEOBJECT 13D.5: SEARCH ENVELOPE EXHAUSTED");
            Debug::Logger::Info(
                "All bounded NavMesh approach candidates were attempted; final visibility/failure policy now applies.");
            Debug::Logger::Info("================================");
            return false;
        }

        bool StartProfileSearchDestination(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (profile_ == nullptr || !profile_->destination.valid)
                return false;

            const float arrival =
                profile_->destination.arrivalDistance > 0.0f
                    ? profile_->destination.arrivalDistance
                    : 35.0f;

            Debug::Logger::Info(
                "INTERACT GAMEOBJECT 13D.5: starting exact QuestDB gameobject search seed first.");

            searchEnvelopeCandidateActive_ = false;
            if (StartNavigation(
                    world,
                    tick,
                    Navigation::NavPoint{
                        profile_->destination.x,
                        profile_->destination.y,
                        profile_->destination.z},
                    arrival,
                    profile_->destination.label,
                    true,
                    false))
            {
                return true;
            }

            return StartNextSearchEnvelopeCandidate(
                world,
                tick,
                "exact QuestDB gameobject search seed could not start");
        }

        bool StartNavigation(
            const Objects::WorldState& world,
            std::uint64_t tick,
            const Navigation::NavPoint& destination,
            float arrivalDistance,
            const std::string& label,
            bool searchSeed,
            bool routeAnchor = false)
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
            routeAnchorActive_ = routeAnchor;
            ResetNavigationWatchdog(world.player, tick);
            SetState(ObjectiveExecutorState::Navigating);
            return true;
        }

        bool BeginObject(
            const Objects::WorldState& world,
            const LiveGameObject& object,
            std::uint64_t tick)
        {
            targetGuid_ = object.guid;
            objectMissingSinceTick_ = 0;
            searchEnvelopeCandidateActive_ = false;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("INTERACT GAMEOBJECT 11D.2.7: LIVE OBJECT");
            Debug::Logger::Info(
                "Entry=" + std::to_string(object.entryId) +
                " guid=" + Hex64(object.guid) +
                " distance=" + std::to_string(object.distance));
            Debug::Logger::Info(
                "Position=(" + std::to_string(object.x) + "," +
                std::to_string(object.y) + "," + std::to_string(object.z) + ")");
            Debug::Logger::Info("================================");

            if (object.distance > InteractionDistance)
            {
                return StartNavigation(
                    world,
                    tick,
                    Navigation::NavPoint{object.x, object.y, object.z},
                    LiveObjectNavArrivalDistance,
                    "InteractGameObject live object entry " + std::to_string(object.entryId),
                    false,
                    false);
            }

            navigator_.reset();
            navigatingSearchSeed_ = false;
            SetState(ObjectiveExecutorState::Executing);
            return true;
        }

        static bool InteractExactObject(
            const LiveGameObject& object)
        {
            const std::uintptr_t address =
                Wow5875::Client::Base() + OnRightClickObjectRva;
            if (!IsExecutable(address))
                return false;

            using Function = void (__thiscall*)(std::uint32_t, int);
            const auto function = reinterpret_cast<Function>(address);

            bool onGameThread = false;
            bool invoked = false;
            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();

                    LiveGameObject current{};
                    Objects::PlayerState unusedPlayer{};
                    // Revalidate type/GUID directly. Position is irrelevant to
                    // the click itself, so the caller's live object remains the
                    // authoritative range check.
                    std::uint32_t type = 0;
                    std::uint64_t guid = 0;
                    if (!ReadValue(object.address + ObjectTypeOffset, type) ||
                        !ReadValue(object.address + ObjectGuidOffset, guid) ||
                        type != GameObjectType || guid != object.guid)
                    {
                        return;
                    }

                    function(static_cast<std::uint32_t>(object.address), 1);
                    invoked = true;
                });

            return dispatched && onGameThread && invoked;
        }

        void Complete()
        {
            navigator_.reset();
            Debug::Logger::Info("================================");
            Debug::Logger::Info("OBJECTIVE 11D.2.7: INTERACT GAMEOBJECT COMPLETE");
            Debug::Logger::Info(
                "Quest " + std::to_string(profile_->questId) + " " + profile_->title +
                " is complete in the live quest log.");
            Debug::Logger::Info("================================");
            SetState(ObjectiveExecutorState::ReadyForTurnIn);
        }

        void Fail(const std::string& reason)
        {
            failureReason_ = reason;
            navigator_.reset();
            Debug::Logger::Info("================================");
            Debug::Logger::Info("OBJECTIVE 11D.2.7: INTERACT GAMEOBJECT FAILED");
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info("================================");
            SetState(ObjectiveExecutorState::Failed);
        }

    public:
        // Read-only exact-entry lookup reused by item-at-GO objectives.
        static bool ObserveMatchingObject(
            std::uint32_t entry, const Objects::PlayerState& player,
            ObservedGameObject& observed)
        {
            LiveGameObject object{};
            if (!FindNearestGameObject(entry, player, object))
                return false;
            observed = {object.guid, object.entryId, object.x,
                object.y, object.z, object.distance};
            return true;
        }

        static bool InteractMatchingObject(
            const ObservedGameObject& observed,
            const Objects::PlayerState& player)
        {
            LiveGameObject object{};
            return observed.guid != 0 &&
                FindGameObjectByGuid(observed.guid, player, object) &&
                object.entryId == observed.entry &&
                object.distance <= InteractionDistance &&
                std::hypot(object.x - observed.x,
                    object.y - observed.y, object.z - observed.z) <= 2.0f &&
                InteractExactObject(object);
        }

        bool Supports(const QuestProfile& profile) const override
        {
            return profile.objective.type == QuestObjectiveType::InteractGameObject &&
                   QuestObjectiveDispatchPolicy::SupportsMaterialized(profile);
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
            objectMissingSinceTick_ = 0;
            interactionAttempts_ = 0;
            navigatingSearchSeed_ = false;
            searchSeedVisited_ = false;
            routeAnchorActive_ = false;
            routeAnchorVisited_ = false;
            navigationStartTick_ = 0;
            navigationLastMovementTick_ = 0;
            navigationLastStatusTick_ = 0;
            navigationBaselineX_ = world.player.x;
            navigationBaselineY_ = world.player.y;
            defenseOwnedPreviousTick_ = false;
            finalVisibilityProbeAttempts_ = 0;
            lastFinalVisibilityProbeTick_ = 0;
            scanDiagnosticLogged_ = false;
            searchEnvelopeCandidateIndex_ = 0;
            searchEnvelopeCandidateActive_ = false;
            searchEnvelopeExhausted_ = false;
            navigator_.reset();
            defense_.Reset(
                world,
                "InteractGameObject quest " + std::to_string(profile.questId));

            Debug::Logger::Info("================================");
            Debug::Logger::Info("OBJECTIVE EXECUTOR 11D.2.7: START");
            Debug::Logger::Info(
                "Quest: " + std::to_string(profile.questId) + " " + profile.title);
            Debug::Logger::Info("Type: InteractGameObject");
            Debug::Logger::Info(
                "Object: " + std::string(profile.objective.targetName) +
                " entry=" + std::to_string(profile.objective.objectEntry));
            Debug::Logger::Info("================================");

            LiveGameObject object{};
            if (FindNearestGameObject(profile.objective.objectEntry, world.player, object))
                return BeginObject(world, object, tick);

            // Burning Blade Coven already has a runtime-verified Detour
            // corridor from the Yarrog work. Route there first and scan for
            // the exact gameobject throughout the trip. This avoids asking a
            // long route query/follower to terminate on the awkward pick
            // location before the object is even in client visibility.
            if (profile.routeGroup == QuestRouteGroup::BurningBladeCoven)
            {
                const float searchDistance = ProfileDestinationDistance(world.player);
                if (profile.destination.valid &&
                    searchDistance <= DirectSearchRetryDistance)
                {
                    Debug::Logger::Info(
                        "INTERACT GAMEOBJECT 11D.2.7: already inside/near Burning Blade Coven; skipping route-group anchor and continuing from live position.");
                    Debug::Logger::Info(
                        "Profiled search distance=" + std::to_string(searchDistance));

                    if (StartProfileSearchDestination(world, tick))
                        return true;
                }

                Debug::Logger::Info(
                    "INTERACT GAMEOBJECT 11D.2.7: using runtime-verified Burning Blade Coven route-group anchor first.");

                if (StartNavigation(
                        world,
                        tick,
                        BurningBladeCovenVerifiedAnchor(),
                        35.0f,
                        "Burning Blade Coven verified Yarrog anchor",
                        true,
                        true))
                {
                    return true;
                }
            }

            if (StartProfileSearchDestination(world, tick))
                return true;

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
            {
                return;
            }

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
                0,
                tick);

            if (defenseUpdate == ObjectiveDefenseUpdate::Failed)
            {
                Fail("defensive combat/recovery failed during InteractGameObject objective.");
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
                if (state_ == ObjectiveExecutorState::Navigating && navigator_)
                {
                    // Combat/recovery time is not navigation no-progress. The
                    // cached corridor has just resumed, so restart the outer
                    // objective watchdog from the live player position.
                    ResetNavigationWatchdog(world.player, tick);
                    Debug::Logger::Info(
                        "INTERACT GAMEOBJECT 11D.2.7: NAV WATCHDOG RESET AFTER DEFENSIVE RESUME");
                }
            }

            if (SelectedObjectiveComplete(*liveQuest, *profile_))
            {
                Complete();
                return;
            }

            if (tick > objectiveStartTick_ + MaximumObjectiveTicks)
            {
                Fail("InteractGameObject objective timeout reached.");
                return;
            }

            if (state_ == ObjectiveExecutorState::Navigating)
            {
                if (!navigator_)
                {
                    Fail("navigation state is missing.");
                    return;
                }

                LiveGameObject object{};
                if (navigatingSearchSeed_ &&
                    FindNearestGameObject(profile_->objective.objectEntry, world.player, object))
                {
                    navigator_.reset();
                    BeginObject(world, object, tick);
                    return;
                }

                if (!navigatingSearchSeed_ && targetGuid_ != 0 &&
                    FindGameObjectByGuid(targetGuid_, world.player, object) &&
                    object.distance <= InteractionDistance)
                {
                    navigator_.reset();
                    navigatingSearchSeed_ = false;
                    routeAnchorActive_ = false;
                    SetState(ObjectiveExecutorState::Executing);
                    return;
                }

                const float movedSinceBaseline =
                    std::sqrt(
                        (world.player.x - navigationBaselineX_) *
                            (world.player.x - navigationBaselineX_) +
                        (world.player.y - navigationBaselineY_) *
                            (world.player.y - navigationBaselineY_));

                if (movedSinceBaseline >= NavigationProgressDistance)
                {
                    navigationBaselineX_ = world.player.x;
                    navigationBaselineY_ = world.player.y;
                    navigationLastMovementTick_ = tick;
                }

                if (tick >= navigationLastStatusTick_ + NavigationStatusTicks)
                {
                    navigationLastStatusTick_ = tick;
                    Debug::Logger::Info(
                        "INTERACT GAMEOBJECT 11D.2.7: NAV ACTIVITY noProgressTicks=" +
                        std::to_string(tick - navigationLastMovementTick_) +
                        " searchSeed=" +
                        std::string(navigatingSearchSeed_ ? "yes" : "no") +
                        " routeAnchor=" +
                        std::string(routeAnchorActive_ ? "yes" : "no"));
                }

                if (tick >= navigationLastMovementTick_ + NavigationNoProgressTicks)
                {
                    Debug::Logger::Info("================================");
                    Debug::Logger::Info(
                        "INTERACT GAMEOBJECT 11D.2.7: NAVIGATION NO-PROGRESS WATCHDOG");
                    Debug::Logger::Info(
                        "No meaningful displacement for " +
                        std::to_string(NavigationNoProgressTicks) +
                        " ticks; releasing the stalled navigation leg immediately.");
                    Debug::Logger::Info("================================");

                    navigator_.reset();
                    StopMovement(world.player);

                    if (routeAnchorActive_)
                    {
                        routeAnchorActive_ = false;
                        routeAnchorVisited_ = true;
                        if (StartProfileSearchDestination(world, tick))
                            return;

                        Fail("route-group anchor stalled and no profiled gameobject search destination could start.");
                        return;
                    }

                    if (navigatingSearchSeed_)
                    {
                        const float searchDistance = ProfileDestinationDistance(world.player);
                        Debug::Logger::Info(
                            "INTERACT GAMEOBJECT 13D.5: stalled search-point distance=" +
                            std::to_string(searchDistance));

                        if (StartNextSearchEnvelopeCandidate(
                                world,
                                tick,
                                searchEnvelopeCandidateActive_
                                    ? "current search-envelope approach made no physical progress"
                                    : "exact search seed made no physical progress"))
                        {
                            return;
                        }

                        if (IssueFinalVisibilityProbe(
                                world,
                                tick,
                                "all NavMesh search-envelope approaches exhausted near profiled gameobject area"))
                        {
                            return;
                        }

                        Fail("InteractGameObject exact seed and bounded reachable search envelope made no progress.");
                        return;
                    }

                    targetGuid_ = 0;
                    navigatingSearchSeed_ = false;
                    SetState(ObjectiveExecutorState::Executing);
                    return;
                }

                navigator_->Update(world.player, tick);

                if (navigator_->Failed())
                {
                    navigator_.reset();
                    StopMovement(world.player);

                    if (routeAnchorActive_)
                    {
                        Debug::Logger::Info(
                            "INTERACT GAMEOBJECT 11D.2.7: verified route-group anchor failed; immediately trying the shorter profiled search leg from the live position.");
                        routeAnchorActive_ = false;
                        routeAnchorVisited_ = true;

                        if (StartProfileSearchDestination(world, tick))
                            return;

                        Fail("Burning Blade route-group anchor failed and profiled search navigation could not start.");
                        return;
                    }

                    if (navigatingSearchSeed_)
                    {
                        if (StartNextSearchEnvelopeCandidate(
                                world,
                                tick,
                                searchEnvelopeCandidateActive_
                                    ? "current search-envelope NavMesh route failed"
                                    : "exact QuestDB search-seed NavMesh route failed"))
                        {
                            return;
                        }

                        if (IssueFinalVisibilityProbe(
                                world,
                                tick,
                                "all bounded search-envelope NavMesh routes failed"))
                        {
                            return;
                        }

                        Fail("NavMesh could not reach the InteractGameObject seed or any bounded search-envelope approach.");
                        return;
                    }

                    targetGuid_ = 0;
                    SetState(ObjectiveExecutorState::Executing);
                    return;
                }

                if (navigator_->Arrived())
                {
                    if (routeAnchorActive_)
                    {
                        Debug::Logger::Info(
                            "INTERACT GAMEOBJECT 11D.2.7: BURNING BLADE ROUTE-GROUP ANCHOR REACHED; rescanning before any deeper search leg.");
                        routeAnchorVisited_ = true;
                        routeAnchorActive_ = false;
                    }
                    else if (navigatingSearchSeed_)
                    {
                        searchSeedVisited_ = true;

                        LiveGameObject arrivedObject{};
                        if (FindNearestGameObject(
                                profile_->objective.objectEntry,
                                world.player,
                                arrivedObject))
                        {
                            navigator_.reset();
                            BeginObject(world, arrivedObject, tick);
                            return;
                        }

                        navigator_.reset();
                        if (StartNextSearchEnvelopeCandidate(
                                world,
                                tick,
                                searchEnvelopeCandidateActive_
                                    ? "reached search-envelope approach but object is still outside client visibility"
                                    : "reached exact QuestDB search seed but object is still outside client visibility"))
                        {
                            return;
                        }
                    }

                    navigator_.reset();
                    navigatingSearchSeed_ = false;
                    searchEnvelopeCandidateActive_ = false;

                    if (searchSeedVisited_ &&
                        IssueFinalVisibilityProbe(
                            world,
                            tick,
                            "bounded NavMesh search envelope exhausted but gameobject is still not visible"))
                    {
                        return;
                    }

                    SetState(ObjectiveExecutorState::Executing);
                }
                return;
            }

            LiveGameObject object{};
            bool haveObject = false;
            if (targetGuid_ != 0)
                haveObject = FindGameObjectByGuid(targetGuid_, world.player, object);
            if (!haveObject)
                haveObject = FindNearestGameObject(
                    profile_->objective.objectEntry,
                    world.player,
                    object);

            if (!haveObject)
            {
                targetGuid_ = 0;

                if (!searchSeedVisited_ && profile_->destination.valid)
                {
                    if (!StartProfileSearchDestination(world, tick))
                        Fail("failed to start gameobject search-seed navigation.");
                    return;
                }

                if (searchSeedVisited_ && !searchEnvelopeExhausted_ &&
                    StartNextSearchEnvelopeCandidate(
                        world,
                        tick,
                        "gameobject not visible after previous search approach"))
                {
                    return;
                }

                if (searchSeedVisited_ && !scanDiagnosticLogged_)
                {
                    scanDiagnosticLogged_ = true;
                    LogGameObjectScanDiagnostic(
                        profile_->objective.objectEntry,
                        world.player);
                }

                if (searchSeedVisited_ &&
                    IssueFinalVisibilityProbe(
                        world,
                        tick,
                        "gameobject still not visible after reaching the profiled search area"))
                {
                    return;
                }

                if (objectMissingSinceTick_ == 0)
                {
                    objectMissingSinceTick_ = tick;
                    Debug::Logger::Info(
                        "INTERACT GAMEOBJECT 11D.2.7: target object not visible after bounded visibility probes; short final grace window.");
                    return;
                }

                if (tick >= objectMissingSinceTick_ + ObjectMissingGraceTicks)
                    Fail("profiled gameobject was not visible after reaching its search area.");
                return;
            }

            objectMissingSinceTick_ = 0;
            targetGuid_ = object.guid;

            if (object.distance > InteractionDistance)
            {
                if (!StartNavigation(
                        world,
                        tick,
                        Navigation::NavPoint{object.x, object.y, object.z},
                        LiveObjectNavArrivalDistance,
                        "InteractGameObject live object entry " +
                            std::to_string(object.entryId),
                        false,
                        false))
                {
                    Fail("failed to start NavMesh route to live gameobject.");
                }
                return;
            }

            if (interactionAttempts_ >= MaximumInteractionAttempts)
            {
                if (tick >= lastInteractionTick_ + InteractionCompletionGraceTicks)
                    Fail("quest did not become complete after bounded exact gameobject interactions.");
                return;
            }

            if (lastInteractionTick_ != 0 &&
                tick < lastInteractionTick_ + InteractionRetryTicks)
            {
                return;
            }

            Debug::Logger::Info("================================");
            Debug::Logger::Info("INTERACT GAMEOBJECT 11D.2.7: EXACT GUID INTERACTION");
            Debug::Logger::Info(
                "Entry=" + std::to_string(object.entryId) +
                " guid=" + Hex64(object.guid) +
                " distance=" + std::to_string(object.distance));
            Debug::Logger::Info(
                "Attempt=" + std::to_string(interactionAttempts_ + 1) + "/" +
                std::to_string(MaximumInteractionAttempts));
            Debug::Logger::Info("================================");

            if (!InteractExactObject(object))
            {
                Fail("native gameobject right-click dispatch failed.");
                return;
            }

            ++interactionAttempts_;
            lastInteractionTick_ = tick;
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

        const char* FailureReason() const override
        {
            return failureReason_.empty() ? nullptr : failureReason_.c_str();
        }
    };
}
