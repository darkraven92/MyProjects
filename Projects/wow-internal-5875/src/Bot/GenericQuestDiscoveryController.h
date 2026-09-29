#pragma once

#include "GameThreadDispatcher.h"
#include "ClickToMoveController.h"
#include "QuestPlannerTypes.h"
#include "ValleyOfTrialsProfiles.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"
#include "../Navigation/GenericNavMeshPathFollower.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace Bot
{
    enum class GenericQuestDiscoveryState
    {
        Idle,
        FindingNearbyGiver,
        TravelingToGiverSeed,
        TravelingToGiver,
        DirectApproachToGiver,
        AdvancingDialog,
        WaitingForQuestLog,
        NoNearbyOffer,
        Done,
        Failed
    };

    class GenericQuestDiscoveryController
    {
    private:
        static constexpr std::uintptr_t OnRightClickUnitRva = 0x0020BEA0;
        static constexpr std::uintptr_t LuaDoStringRva = 0x00304CD0;
        static constexpr std::uintptr_t GetTextRva = 0x00303BF0;
        static constexpr std::uintptr_t ObjectManagerRootRva = 0x00741414;
        static constexpr std::uintptr_t FirstObjectOffset = 0x000000AC;
        static constexpr std::uintptr_t ObjectGuidOffset = 0x00000030;
        static constexpr std::uintptr_t ObjectNextOffset = 0x0000003C;

        static constexpr float InteractionDistance = 5.5f;
        static constexpr float VisibleGiverSearchDistance = 120.0f;
        static constexpr float GiverNavArrivalDistance = 5.0f;

        // Phase 13D.5.2/13D.5.3: live quest givers can stand on small platforms,
        // stairs, porches or mesh seams where a full Detour route to their
        // exact XYZ is impossible even though the player is already only a
        // few yards away. NavMesh remains the primary approach. When it fails
        // near a live giver, a bounded direct-CTM interaction envelope tries
        // short stand-off points around that same live NPC.
        static constexpr float NearGiverDirectFallbackDistance = 14.0f;
        static constexpr float DirectGiverStandOffDistance = 4.25f;
        static constexpr float DirectGiverRaisedStandOffDistance = 3.75f;
        static constexpr float DirectGiverCtmPrecision = 0.75f;
        static constexpr float DirectGiverProgressEpsilon = 0.35f;
        static constexpr float DirectGiverRaisedVerticalThreshold = 1.50f;
        static constexpr std::uint64_t DirectGiverCandidateTicks = 28;
        static constexpr int DirectGiverCandidateCount = 8;
        // Phase 13D.5.3: the total timeout must be long enough to actually
        // exercise every bounded candidate. 13D.5.2 used 180 ticks while
        // 8*28=224 candidate ticks were possible, so the eighth candidate
        // could never be reached in a slow last-mile case.
        static constexpr std::uint64_t DirectGiverMaximumTicks =
            DirectGiverCandidateTicks *
                static_cast<std::uint64_t>(DirectGiverCandidateCount) +
            56;
        // Phase 13A.1: a hub is selected from live/DB-local evidence rather
        // than trusting completion history alone. This keeps a restart in
        // Sen'jin from looking like the Valley hub when quest 805 was turned
        // in before its completion was persisted.
        static constexpr float HubAnchorProximityDistance = 120.0f;
        // Phase 13B: the complete Durotar catalogue must not make a local hub
        // sweep walk across the entire zone. Each discovery pass is scoped to
        // giver anchors near the position where the sweep began. Travel/report
        // quests move the player to the next hub and naturally start a fresh
        // scoped sweep there.
        static constexpr float ZoneHubAuditRadius = 300.0f;
        static constexpr std::uint32_t KalimdorMapId = 1;
        static constexpr std::uint64_t DialogStepTicks = 4;
        static constexpr std::uint64_t ReinteractTicks = 12;
        static constexpr std::uint64_t OverallTimeoutTicks = 900;
        static constexpr int MaximumInteractionAttempts = 2;
        static constexpr const char* ResultVariable =
            "WOW_INTERNAL_DISCOVERY_STATE";

        GenericQuestDiscoveryState state_ =
            GenericQuestDiscoveryState::Idle;

        std::uint64_t startTick_ = 0;
        std::uint64_t lastInteractionTick_ = 0;
        std::uint64_t lastDialogTick_ = 0;
        int interactionAttempts_ = 0;

        std::uint64_t giverGuid_ = 0;
        std::uint32_t giverEntry_ = 0;

        const QuestProfile* selectedProfile_ = nullptr;
        std::string selectedTitle_{};
        std::string lastDialogResult_{};

        std::set<std::uint64_t> checkedGivers_{};
        std::set<std::uint32_t> checkedGiverEntries_{};
        std::set<int> excludedQuestIds_{};
        int activeHubUnlockQuestId_ = 0;
        std::string activeHubEvidence_{};
        float auditOriginX_ = 0.0f;
        float auditOriginY_ = 0.0f;
        std::uint32_t giverSeedEntry_ = 0;
        const QuestProfile* giverSeedProfile_ = nullptr;
        bool noNearbyOfferLogged_ = false;

        std::unique_ptr<Navigation::GenericNavMeshPathFollower> giverNavigator_{};

        std::uint64_t directGiverStartTick_ = 0;
        std::uint64_t directGiverCandidateStartTick_ = 0;
        float directGiverBestDistance_ = 0.0f;
        int directGiverCandidateIndex_ = 0;

        static const char* StateNameInternal(
            GenericQuestDiscoveryState state)
        {
            switch (state)
            {
                case GenericQuestDiscoveryState::Idle:
                    return "Idle";
                case GenericQuestDiscoveryState::FindingNearbyGiver:
                    return "FindingNearbyGiver";
                case GenericQuestDiscoveryState::TravelingToGiverSeed:
                    return "TravelingToGiverSeed";
                case GenericQuestDiscoveryState::TravelingToGiver:
                    return "TravelingToGiver";
                case GenericQuestDiscoveryState::DirectApproachToGiver:
                    return "DirectApproachToGiver";
                case GenericQuestDiscoveryState::AdvancingDialog:
                    return "AdvancingDialog";
                case GenericQuestDiscoveryState::WaitingForQuestLog:
                    return "WaitingForQuestLog";
                case GenericQuestDiscoveryState::NoNearbyOffer:
                    return "NoNearbyOffer";
                case GenericQuestDiscoveryState::Done:
                    return "Done";
                case GenericQuestDiscoveryState::Failed:
                    return "Failed";
                default:
                    return "Unknown";
            }
        }

        void SetState(GenericQuestDiscoveryState next)
        {
            if (state_ == next)
                return;

            Debug::Logger::Info(
                std::string("GenericQuestDiscovery state: ") +
                StateNameInternal(state_) + " -> " +
                StateNameInternal(next));

            state_ = next;
        }

        void Fail(const std::string& reason)
        {
            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST PLANNER DISCOVERY: FAILED");
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info("================================");
            SetState(GenericQuestDiscoveryState::Failed);
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

        static std::uintptr_t OnRightClickUnitAddress()
        {
            return Wow5875::Client::Base() + OnRightClickUnitRva;
        }

        static std::uintptr_t LuaDoStringAddress()
        {
            return Wow5875::Client::Base() + LuaDoStringRva;
        }

        static std::uintptr_t GetTextAddress()
        {
            return Wow5875::Client::Base() + GetTextRva;
        }

        static std::string Hex64(std::uint64_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << std::uppercase
                   << std::setw(16) << std::setfill('0') << value;
            return stream.str();
        }

        static std::string LuaSingleQuoted(const std::string& value)
        {
            std::string result;
            result.reserve(value.size() + 2);
            result.push_back('\'');
            for (const char ch : value)
            {
                switch (ch)
                {
                    case '\\': result += "\\\\"; break;
                    case '\'': result += "\\\'"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    default: result.push_back(ch); break;
                }
            }
            result.push_back('\'');
            return result;
        }

        static std::uintptr_t FindObjectAddressByGuid(std::uint64_t guid)
        {
            if (guid == 0)
                return 0;

            std::uint32_t manager = 0;
            if (!ReadValue(Wow5875::Client::Base() + ObjectManagerRootRva, manager) ||
                manager == 0 || (manager & 1u) != 0)
            {
                return 0;
            }

            std::uint32_t current = 0;
            if (!ReadValue(static_cast<std::uintptr_t>(manager) + FirstObjectOffset, current))
                return 0;

            for (int i = 0; i < 4096; ++i)
            {
                if (current == 0 || (current & 1u) != 0)
                    break;

                std::uint64_t currentGuid = 0;
                if (!ReadValue(static_cast<std::uintptr_t>(current) + ObjectGuidOffset, currentGuid))
                    break;

                if (currentGuid == guid)
                    return static_cast<std::uintptr_t>(current);

                std::uint32_t next = 0;
                if (!ReadValue(static_cast<std::uintptr_t>(current) + ObjectNextOffset, next) ||
                    next == current)
                {
                    break;
                }

                current = next;
            }

            return 0;
        }

        static bool Eligible(
            const QuestProfile& profile,
            const QuestPlannerSnapshot& snapshot)
        {
            if (snapshot.playerLevel < profile.minimumLevel)
                return false;

            if (profile.classToken != nullptr &&
                profile.classToken[0] != '\0' &&
                snapshot.classToken != profile.classToken)
            {
                return false;
            }

            return true;
        }

        int ResolveActiveHubUnlockQuestId(
            const QuestPlannerSnapshot& snapshot,
            const Objects::WorldState& world,
            std::string& evidence) const
        {
            // Prefer live giver visibility. This is the strongest evidence
            // that the character is physically inside a gated hub.
            const QuestProfile* bestLiveProfile = nullptr;
            float bestLiveDistance = VisibleGiverSearchDistance + 1.0f;
            for (const auto& unit : world.units)
            {
                if (!unit.valid || unit.guid == 0 ||
                    unit.distance > VisibleGiverSearchDistance)
                {
                    continue;
                }

                for (const auto& profile : ValleyOfTrialsProfiles::All())
                {
                    if (profile.hubUnlockQuestId == 0 ||
                        !Eligible(profile, snapshot) ||
                        profile.giverEntry != unit.entryId)
                    {
                        continue;
                    }

                    if (unit.distance < bestLiveDistance)
                    {
                        bestLiveDistance = unit.distance;
                        bestLiveProfile = &profile;
                    }
                }
            }

            if (bestLiveProfile != nullptr)
            {
                evidence =
                    "live giver entry=" +
                    std::to_string(bestLiveProfile->giverEntry) +
                    " distance=" + std::to_string(bestLiveDistance);
                return bestLiveProfile->hubUnlockQuestId;
            }

            // Fallback to the database giver anchor. This handles a restart
            // with a short object draw distance or a temporarily absent NPC.
            // Coordinates remain QuestDB data; there is no Sen'jin-specific
            // waypoint in the controller.
            const float maxDistanceSq =
                HubAnchorProximityDistance * HubAnchorProximityDistance;
            const QuestProfile* bestAnchorProfile = nullptr;
            float bestAnchorDistanceSq = maxDistanceSq;

            for (const auto& profile : ValleyOfTrialsProfiles::All())
            {
                if (profile.hubUnlockQuestId == 0 ||
                    !Eligible(profile, snapshot) ||
                    !profile.giverDestination.valid ||
                    profile.giverDestination.mapId != KalimdorMapId)
                {
                    continue;
                }

                const float dx = profile.giverDestination.x - world.player.x;
                const float dy = profile.giverDestination.y - world.player.y;
                const float distanceSq = dx * dx + dy * dy;
                if (distanceSq < bestAnchorDistanceSq)
                {
                    bestAnchorDistanceSq = distanceSq;
                    bestAnchorProfile = &profile;
                }
            }

            if (bestAnchorProfile != nullptr)
            {
                evidence =
                    "QuestDB giver proximity entry=" +
                    std::to_string(bestAnchorProfile->giverEntry) +
                    " distance2=" + std::to_string(bestAnchorDistanceSq);
                return bestAnchorProfile->hubUnlockQuestId;
            }

            evidence = "base hub (no gated giver evidence nearby)";
            return 0;
        }

        bool ProfileEligible(
            const QuestProfile& profile,
            const QuestPlannerSnapshot& snapshot) const
        {
            if (!Eligible(profile, snapshot) ||
                !profile.automatable ||
                excludedQuestIds_.find(profile.questId) != excludedQuestIds_.end())
            {
                return false;
            }

            // Phase 13A.1 scopes each audit to the hub the player is actually
            // standing in. Completion history is still used to suppress quests
            // already turned in, but it is no longer the sole proof that a
            // remote hub is active. This prevents both failure modes:
            //   1) missing 805 persistence -> Sen'jin profiles all filtered out
            //   2) completed 805 -> remote Sen'jin audit while already elsewhere
            if (activeHubUnlockQuestId_ == 0)
                return profile.hubUnlockQuestId == 0;

            return profile.hubUnlockQuestId == activeHubUnlockQuestId_;
        }

        bool ProfileWithinLocalAuditRadius(
            const QuestProfile& profile) const
        {
            // Hand-authored fallback profiles without a database giver anchor
            // keep their legacy behavior. Database-backed Durotar profiles
            // are spatially scoped to the current hub sweep.
            if (!profile.giverDestination.valid ||
                profile.giverDestination.mapId != KalimdorMapId)
            {
                return true;
            }

            const float dx = profile.giverDestination.x - auditOriginX_;
            const float dy = profile.giverDestination.y - auditOriginY_;
            return dx * dx + dy * dy <= ZoneHubAuditRadius * ZoneHubAuditRadius;
        }

        bool EntryHasEligibleProfile(
            std::uint32_t giverEntry,
            const QuestPlannerSnapshot& snapshot) const
        {
            for (const auto& profile : ValleyOfTrialsProfiles::All())
            {
                if (profile.giverEntry == giverEntry && ProfileEligible(profile, snapshot))
                    return true;
            }
            return false;
        }

        static bool SnapshotHasActiveLocalHubQuest(
            const QuestPlannerSnapshot& snapshot)
        {
            for (const auto& entry : snapshot.quests)
            {
                const auto* profile =
                    ValleyOfTrialsProfiles::Find(entry, snapshot.classToken);

                if (profile != nullptr &&
                    !profile->hubExit)
                {
                    return true;
                }
            }

            return false;
        }

        const Objects::UnitState* FindGiverByGuid(
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

        const Objects::UnitState* FindVisibleKnownGiver(
            const QuestPlannerSnapshot& snapshot,
            const Objects::WorldState& world) const
        {
            const Objects::UnitState* best = nullptr;

            for (const auto& unit : world.units)
            {
                if (!unit.valid ||
                    unit.guid == 0 ||
                    unit.distance > VisibleGiverSearchDistance ||
                    checkedGivers_.find(unit.guid) != checkedGivers_.end() ||
                    checkedGiverEntries_.find(unit.entryId) != checkedGiverEntries_.end() ||
                    !EntryHasEligibleProfile(unit.entryId, snapshot))
                {
                    continue;
                }

                if (best == nullptr || unit.distance < best->distance)
                    best = &unit;
            }

            return best;
        }

        const Objects::UnitState* FindVisibleGiverByEntry(
            const Objects::WorldState& world,
            std::uint32_t entryId) const
        {
            const Objects::UnitState* best = nullptr;
            for (const auto& unit : world.units)
            {
                if (!unit.valid || unit.entryId != entryId || unit.guid == 0)
                    continue;
                if (best == nullptr || unit.distance < best->distance)
                    best = &unit;
            }
            return best;
        }

        const QuestProfile* FindNextGiverAuditProfile(
            const QuestPlannerSnapshot& snapshot,
            bool hubExit) const
        {
            const QuestProfile* best = nullptr;
            for (const auto& profile : ValleyOfTrialsProfiles::All())
            {
                if (!ProfileEligible(profile, snapshot) ||
                    !ProfileWithinLocalAuditRadius(profile) ||
                    profile.hubExit != hubExit ||
                    profile.giverEntry == 0 ||
                    !profile.giverDestination.valid ||
                    checkedGiverEntries_.find(profile.giverEntry) != checkedGiverEntries_.end())
                {
                    continue;
                }

                if (best == nullptr || profile.priority > best->priority)
                    best = &profile;
            }
            return best;
        }

        int CountRemoteEligibleProfiles(
            const QuestPlannerSnapshot& snapshot) const
        {
            int count = 0;
            for (const auto& profile : ValleyOfTrialsProfiles::All())
            {
                if (!ProfileEligible(profile, snapshot) ||
                    ProfileWithinLocalAuditRadius(profile) ||
                    profile.giverEntry == 0 ||
                    !profile.giverDestination.valid ||
                    checkedGiverEntries_.find(profile.giverEntry) != checkedGiverEntries_.end())
                {
                    continue;
                }
                ++count;
            }
            return count;
        }

        void RefreshAuditLegDeadline(
            std::uint64_t tick,
            const char* reason)
        {
            startTick_ = tick;
            Debug::Logger::Info(
                std::string("QUESTDB 12B.6: giver-audit leg watchdog refreshed; reason=") +
                (reason == nullptr ? "unspecified" : reason) +
                " giverEntry=" + std::to_string(giverEntry_ != 0 ? giverEntry_ : giverSeedEntry_));
        }

        const QuestProfile* ResolveActivatedProfile(
            const QuestPlannerSnapshot& snapshot) const
        {
            if (state_ != GenericQuestDiscoveryState::WaitingForQuestLog ||
                selectedTitle_.empty())
            {
                return nullptr;
            }

            for (const auto& entry : snapshot.quests)
            {
                if (entry.title != selectedTitle_)
                    continue;

                // Phase 13C.2: discovery already owns the exact QuestDB
                // profile selected from this live giver. Preserve that stable
                // quest-id-backed identity across AcceptQuest() instead of
                // re-resolving duplicate presentation text from scratch.
                if (selectedProfile_ != nullptr)
                {
                    if (giverEntry_ != 0 && selectedProfile_->giverEntry != giverEntry_)
                        continue;

                    if (selectedProfile_->expectedObjectiveCount >= 0 &&
                        selectedProfile_->expectedObjectiveCount != entry.objectiveCount)
                    {
                        continue;
                    }

                    return selectedProfile_;
                }

                const auto* actual = ValleyOfTrialsProfiles::Find(
                    entry,
                    snapshot.classToken,
                    &excludedQuestIds_,
                    giverEntry_);
                if (actual != nullptr)
                    return actual;
            }

            return nullptr;
        }

        bool StartGiverSeedNavigation(
            const Objects::WorldState& world,
            const QuestProfile& profile,
            std::uint64_t tick)
        {
            if (!profile.giverDestination.valid)
                return false;

            giverSeedEntry_ = profile.giverEntry;
            giverSeedProfile_ = &profile;
            giverNavigator_ = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            RefreshAuditLegDeadline(tick, "starting database giver seed route");

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUESTDB 12B.6: PROFILED GIVER AUDIT ROUTE");
            Debug::Logger::Info(
                "QuestId=" + std::to_string(profile.questId) +
                " giverEntry=" + std::to_string(profile.giverEntry) +
                " destination=" + std::string(profile.giverDestination.label));
            Debug::Logger::Info(
                "Policy: route to the configured giver seed during the current hub audit; live NPC XYZ takes over immediately when visible.");
            Debug::Logger::Info("================================");

            if (!giverNavigator_->Start(
                    world.player,
                    tick,
                    Navigation::NavPoint{
                        profile.giverDestination.x,
                        profile.giverDestination.y,
                        profile.giverDestination.z},
                    profile.giverDestination.mapId,
                    profile.giverDestination.arrivalDistance > 0.0f
                        ? profile.giverDestination.arrivalDistance
                        : 6.0f,
                    profile.giverDestination.label))
            {
                giverNavigator_.reset();
                DeferCurrentGiverSeedForWave(
                    tick,
                    "generic NavMesh route to the configured giver seed failed to start; continue auditing the remaining hub givers.");
                return true;
            }

            SetState(GenericQuestDiscoveryState::TravelingToGiverSeed);
            return true;
        }

        bool StartGiverNavigation(
            const Objects::WorldState& world,
            const Objects::UnitState& giver,
            std::uint64_t tick)
        {
            giverGuid_ = giver.guid;
            giverEntry_ = giver.entryId;
            RefreshAuditLegDeadline(tick, "live giver acquired");
            if (giverSeedEntry_ == giver.entryId)
            {
                giverSeedEntry_ = 0;
                giverSeedProfile_ = nullptr;
            }

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST PLANNER 11D.1: PROFILED GIVER FOUND");
            Debug::Logger::Info(
                "Giver entry=" + std::to_string(giverEntry_) +
                " guid=" + Hex64(giverGuid_) +
                " distance=" + std::to_string(giver.distance));
            Debug::Logger::Info(
                "Live giver position=(" +
                std::to_string(giver.x) + "," +
                std::to_string(giver.y) + "," +
                std::to_string(giver.z) + ").");
            Debug::Logger::Info(
                "Policy: NavMesh to live giver position, then interact only inside bounded range.");
            Debug::Logger::Info("================================");

            if (giver.distance <= InteractionDistance)
            {
                return IssueInteraction(giver, tick);
            }

            const Navigation::NavPoint destination{
                giver.x,
                giver.y,
                giver.z
            };

            giverNavigator_ =
                std::make_unique<Navigation::GenericNavMeshPathFollower>();

            if (!giverNavigator_->Start(
                    world.player,
                    tick,
                    destination,
                    KalimdorMapId,
                    GiverNavArrivalDistance,
                    "profiled quest giver entry=" + std::to_string(giverEntry_)))
            {
                giverNavigator_.reset();
                if (StartDirectGiverApproach(
                        world,
                        giver,
                        tick,
                        "generic NavMesh route to the live giver failed to start."))
                {
                    return true;
                }

                DeferCurrentGiverApproachForWave(
                    tick,
                    "generic NavMesh route to the live giver failed to start outside the bounded direct-fallback radius.");
                return true;
            }

            SetState(GenericQuestDiscoveryState::TravelingToGiver);
            return true;
        }

        bool IssueDirectGiverCandidate(
            const Objects::WorldState& world,
            const Objects::UnitState& giver,
            std::uint64_t tick,
            int candidateIndex)
        {
            static constexpr float Pi = 3.14159265358979323846f;
            static constexpr float AngleOffsetsDegrees[DirectGiverCandidateCount] =
            {
                0.0f,
                45.0f,
                -45.0f,
                90.0f,
                -90.0f,
                135.0f,
                -135.0f,
                180.0f
            };

            float dx = world.player.x - giver.x;
            float dy = world.player.y - giver.y;
            const float horizontalDistance = std::sqrt(dx * dx + dy * dy);
            if (horizontalDistance < 0.001f)
            {
                dx = 1.0f;
                dy = 0.0f;
            }
            else
            {
                dx /= horizontalDistance;
                dy /= horizontalDistance;
            }

            const float baseAngle = std::atan2(dy, dx);
            const int boundedIndex =
                std::max(0, std::min(candidateIndex, DirectGiverCandidateCount - 1));
            const float angle =
                baseAngle + AngleOffsetsDegrees[boundedIndex] * Pi / 180.0f;

            const float verticalDelta =
                std::fabs(world.player.z - giver.z);
            const bool raisedGiver =
                verticalDelta > DirectGiverRaisedVerticalThreshold;
            const float standOffDistance =
                raisedGiver ?
                    DirectGiverRaisedStandOffDistance :
                    DirectGiverStandOffDistance;

            const float destinationX =
                giver.x + std::cos(angle) * standOffDistance;
            const float destinationY =
                giver.y + std::sin(angle) * standOffDistance;

            // Phase 13D.5.3: when the live giver is vertically offset from
            // the player (small platform, porch, stairs, mesh seam), using
            // giver.z for every direct-CTM candidate can make the client keep
            // trying to climb the platform face. Keep raised-giver fallback
            // candidates on the player's current ground plane and close the
            // horizontal gap instead. The measured 3-D distance still gates
            // the actual interaction, so this does not bypass range checks.
            const float destinationZ =
                raisedGiver ? world.player.z : giver.z;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST DISCOVERY 13D.5.3: DIRECT GIVER APPROACH CANDIDATE");
            Debug::Logger::Info(
                "giverEntry=" + std::to_string(giver.entryId) +
                " candidate=" + std::to_string(boundedIndex + 1) +
                "/" + std::to_string(DirectGiverCandidateCount) +
                " liveDistance=" + std::to_string(giver.distance));
            Debug::Logger::Info(
                "destination=(" +
                std::to_string(destinationX) + "," +
                std::to_string(destinationY) + "," +
                std::to_string(destinationZ) + ")" +
                " standOff=" + std::to_string(standOffDistance));
            Debug::Logger::Info(
                "verticalDelta=" + std::to_string(verticalDelta) +
                " raisedGiver=" + std::string(raisedGiver ? "yes" : "no") +
                " zStrategy=" + std::string(
                    raisedGiver ? "player-ground-plane" : "giver-plane"));
            Debug::Logger::Info(
                "Reason: close live giver is reachable enough for a bounded last-mile CTM fallback after NavMesh approach failure.");
            Debug::Logger::Info("================================");

            if (!ClickToMoveController::MoveTo(
                    world.player,
                    destinationX,
                    destinationY,
                    destinationZ,
                    DirectGiverCtmPrecision))
            {
                return false;
            }

            directGiverCandidateIndex_ = boundedIndex;
            directGiverCandidateStartTick_ = tick;
            return true;
        }

        bool StartDirectGiverApproach(
            const Objects::WorldState& world,
            const Objects::UnitState& giver,
            std::uint64_t tick,
            const std::string& reason)
        {
            if (giver.distance > NearGiverDirectFallbackDistance)
                return false;

            giverNavigator_.reset();
            directGiverStartTick_ = tick;
            directGiverCandidateStartTick_ = tick;
            directGiverBestDistance_ = giver.distance;
            directGiverCandidateIndex_ = 0;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST DISCOVERY 13D.5.3: CLOSE LIVE GIVER FALLBACK START");
            Debug::Logger::Info(
                "giverEntry=" + std::to_string(giver.entryId) +
                " guid=" + Hex64(giver.guid) +
                " distance=" + std::to_string(giver.distance));
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info(
                "Policy: keep the live GUID fixed, try short bounded stand-off CTM candidates around the NPC, and interact as soon as the measured live distance enters interaction range.");
            Debug::Logger::Info("================================");

            if (!IssueDirectGiverCandidate(world, giver, tick, 0))
                return false;

            SetState(GenericQuestDiscoveryState::DirectApproachToGiver);
            return true;
        }

        void DeferCurrentGiverApproachForWave(
            std::uint64_t tick,
            const std::string& reason)
        {
            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST DISCOVERY 13D.5.3: GIVER APPROACH DEFERRED FOR CURRENT WAVE");
            Debug::Logger::Info(
                "giverEntry=" + std::to_string(giverEntry_) +
                " guid=" + Hex64(giverGuid_));
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info(
                "Policy: do not let one locally awkward quest giver block pickup of the remaining hub quests; this giver may be retried in a later fresh pickup wave.");
            Debug::Logger::Info("================================");

            if (giverGuid_ != 0)
                checkedGivers_.insert(giverGuid_);
            if (giverEntry_ != 0)
                checkedGiverEntries_.insert(giverEntry_);

            giverGuid_ = 0;
            giverEntry_ = 0;
            interactionAttempts_ = 0;
            giverNavigator_.reset();
            directGiverStartTick_ = 0;
            directGiverCandidateStartTick_ = 0;
            directGiverBestDistance_ = 0.0f;
            directGiverCandidateIndex_ = 0;
            RefreshAuditLegDeadline(tick, "close giver approach deferred");
            SetState(GenericQuestDiscoveryState::FindingNearbyGiver);
        }

        void DeferCurrentGiverSeedForWave(
            std::uint64_t tick,
            const std::string& reason)
        {
            const std::uint32_t deferredEntry = giverSeedEntry_;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST DISCOVERY 13D.6: GIVER SEED DEFERRED FOR CURRENT WAVE");
            Debug::Logger::Info(
                "giverEntry=" + std::to_string(deferredEntry));
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info(
                "Policy: a disconnected/partial local giver route must not make the entire pickup wave idle; mark this seed checked for this wave and continue the deterministic audit.");
            Debug::Logger::Info("================================");

            if (deferredEntry != 0)
                checkedGiverEntries_.insert(deferredEntry);

            giverSeedEntry_ = 0;
            giverSeedProfile_ = nullptr;
            giverNavigator_.reset();
            RefreshAuditLegDeadline(tick, "giver seed route deferred");
            SetState(GenericQuestDiscoveryState::FindingNearbyGiver);
        }

        const QuestProfile* ResolveOfferedTitle(
            const QuestPlannerSnapshot& snapshot,
            const std::string& title) const
        {
            std::vector<const QuestProfile*> candidates;
            for (const auto& profile : ValleyOfTrialsProfiles::All())
            {
                if (profile.giverEntry != giverEntry_ ||
                    !ProfileEligible(profile, snapshot) ||
                    title != profile.title)
                {
                    continue;
                }
                candidates.push_back(&profile);
            }

            if (candidates.size() <= 1)
                return candidates.empty() ? nullptr : candidates.front();

            std::vector<const QuestProfile*> remaining;
            for (const auto* candidate : candidates)
            {
                if (excludedQuestIds_.find(candidate->questId) ==
                    excludedQuestIds_.end())
                {
                    remaining.push_back(candidate);
                }
            }
            if (!remaining.empty())
                candidates = std::move(remaining);

            if (candidates.size() == 1)
                return candidates.front();

            std::vector<const QuestProfile*> chainSatisfied;
            for (const auto* candidate : candidates)
            {
                const int previous = candidate->previousQuestId < 0
                    ? -candidate->previousQuestId
                    : candidate->previousQuestId;
                if (previous == 0 ||
                    excludedQuestIds_.find(previous) != excludedQuestIds_.end())
                {
                    chainSatisfied.push_back(candidate);
                }
            }

            if (chainSatisfied.size() == 1)
                return chainSatisfied.front();

            Debug::Logger::Info(
                "QUESTDB 13C.2: AMBIGUOUS LIVE OFFER deferred title=\"" +
                title + "\" giverEntry=" + std::to_string(giverEntry_) +
                " candidates=" + std::to_string(candidates.size()) +
                "; no title-only guess was made.");
            return nullptr;
        }

        const QuestProfile* BestMatchingProfile(
            const QuestPlannerSnapshot& snapshot,
            const std::vector<std::string>& offeredTitles) const
        {
            const QuestProfile* best = nullptr;

            for (const auto& title : offeredTitles)
            {
                const auto* candidate = ResolveOfferedTitle(snapshot, title);
                if (candidate == nullptr)
                    continue;

                if (best == nullptr ||
                    candidate->priority > best->priority ||
                    (candidate->priority == best->priority &&
                     candidate->questId < best->questId))
                {
                    best = candidate;
                }
            }

            return best;
        }

        bool IssueInteraction(
            const Objects::UnitState& giver,
            std::uint64_t tick)
        {
            if (interactionAttempts_ >= MaximumInteractionAttempts)
                return false;

            const auto functionAddress = OnRightClickUnitAddress();
            const auto objectAddress = FindObjectAddressByGuid(giver.guid);
            if (!IsExecutable(functionAddress) || objectAddress == 0)
                return false;

            using OnRightClickUnitFunction = void (__thiscall*)(std::uint32_t, int);
            const auto onRightClickUnit =
                reinterpret_cast<OnRightClickUnitFunction>(functionAddress);

            bool onGameThread = false;
            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    onRightClickUnit(static_cast<std::uint32_t>(objectAddress), 0);
                });

            if (!dispatched || !onGameThread)
                return false;

            giverGuid_ = giver.guid;
            giverEntry_ = giver.entryId;
            ++interactionAttempts_;
            lastInteractionTick_ = tick;
            lastDialogTick_ = tick;
            lastDialogResult_.clear();

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST PLANNER DISCOVERY: interacting with profiled giver.");
            Debug::Logger::Info("Giver entry=" + std::to_string(giverEntry_) +
                                " guid=" + Hex64(giverGuid_) +
                                " distance=" + std::to_string(giver.distance));
            Debug::Logger::Info("================================");

            SetState(GenericQuestDiscoveryState::AdvancingDialog);
            return true;
        }

        static std::vector<std::string> SplitUnitSeparator(const std::string& value)
        {
            std::vector<std::string> parts;
            std::size_t begin = 0;
            while (begin <= value.size())
            {
                const auto end = value.find(static_cast<char>(31), begin);
                parts.push_back(value.substr(begin, end == std::string::npos ?
                    std::string::npos : end - begin));
                if (end == std::string::npos)
                    break;
                begin = end + 1;
            }
            return parts;
        }

        bool RunDialogScript(const std::string& script, std::string& result)
        {
            const auto doStringAddress = LuaDoStringAddress();
            const auto getTextAddress = GetTextAddress();
            if (!IsExecutable(doStringAddress) || !IsExecutable(getTextAddress))
                return false;

            using DoStringFunction = bool (__fastcall*)(const char*, const char*);
            using GetTextFunction = const char* (__fastcall*)(char*, std::uint32_t, int);
            const auto doString = reinterpret_cast<DoStringFunction>(doStringAddress);
            const auto getText = reinterpret_cast<GetTextFunction>(getTextAddress);

            char buffer[1024]{};
            bool onGameThread = false;
            bool luaExecuted = false;
            bool gotText = false;

            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    luaExecuted = doString(
                        script.c_str(),
                        "wow-internal/GenericQuestDiscoveryController.lua");

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

        bool CloseQuestPanels()
        {
            std::string ignored;
            const std::string script =
                "WOW_INTERNAL_DISCOVERY_STATE='closed'; "
                "if CloseGossip then CloseGossip(); end; "
                "if CloseQuest then CloseQuest(); end";
            return RunDialogScript(script, ignored);
        }

        bool ReadOrAdvanceDialog(
            const QuestPlannerSnapshot& snapshot,
            std::uint64_t tick)
        {
            std::string result;

            if (selectedProfile_ == nullptr)
            {
                const std::string script =
                    "WOW_INTERNAL_DISCOVERY_STATE='waiting'; "
                    "local sep=string.char(31); "
                    "local t=GetTitleText(); "
                    "if QuestFrameDetailPanel and QuestFrameDetailPanel:IsVisible() and t then "
                    "WOW_INTERNAL_DISCOVERY_STATE='detail'..sep..t; "
                    "elseif QuestFrameGreetingPanel and QuestFrameGreetingPanel:IsVisible() then "
                    "local s='greeting'; local n=GetNumAvailableQuests(); "
                    "for i=1,n do local q=GetAvailableTitle(i); if q then s=s..sep..q; end; end; "
                    "WOW_INTERNAL_DISCOVERY_STATE=s; "
                    "elseif GossipFrame and GossipFrame:IsVisible() then "
                    "local a={GetGossipAvailableQuests()}; local s='gossip'; "
                    "for i=1,table.getn(a),2 do if a[i] then s=s..sep..a[i]; end; end; "
                    "WOW_INTERNAL_DISCOVERY_STATE=s; end";

                if (!RunDialogScript(script, result))
                    return false;

                if (result != lastDialogResult_)
                {
                    Debug::Logger::Info("QUEST PLANNER DISCOVERY dialog: " + result);
                    lastDialogResult_ = result;
                }

                const auto parts = SplitUnitSeparator(result);
                if (parts.empty())
                    return true;

                std::vector<std::string> offered;
                if (parts[0] == "detail")
                {
                    if (parts.size() > 1)
                        offered.push_back(parts[1]);
                }
                else if (parts[0] == "greeting" || parts[0] == "gossip")
                {
                    for (std::size_t i = 1; i < parts.size(); ++i)
                    {
                        if (!parts[i].empty())
                            offered.push_back(parts[i]);
                    }
                }
                else
                {
                    return true;
                }

                selectedProfile_ = BestMatchingProfile(snapshot, offered);
                if (selectedProfile_ == nullptr)
                {
                    checkedGivers_.insert(giverGuid_);
                    checkedGiverEntries_.insert(giverEntry_);
                    Debug::Logger::Info("QUEST PLANNER DISCOVERY: no eligible profiled quest was offered by giver entry=" +
                                        std::to_string(giverEntry_) + ".");
                    CloseQuestPanels();
                    giverGuid_ = 0;
                    giverEntry_ = 0;
                    interactionAttempts_ = 0;
                    RefreshAuditLegDeadline(tick, "giver checked: no eligible offer");
                    SetState(GenericQuestDiscoveryState::FindingNearbyGiver);
                    return true;
                }

                selectedTitle_ = selectedProfile_->title;
                Debug::Logger::Info("================================");
                Debug::Logger::Info("QUEST PLANNER DISCOVERY: PROFILE MATCH");
                Debug::Logger::Info("QuestId=" + std::to_string(selectedProfile_->questId) +
                                    " title=\"" + selectedTitle_ + "\" priority=" +
                                    std::to_string(selectedProfile_->priority));
                Debug::Logger::Info("Giver entry=" + std::to_string(giverEntry_));
                Debug::Logger::Info("Policy: only this exact offered title may be selected/accepted.");
                Debug::Logger::Info("================================");
                lastDialogTick_ = tick;
                return true;
            }

            const std::string title = LuaSingleQuoted(selectedTitle_);
            const std::string script =
                "WOW_INTERNAL_DISCOVERY_STATE='waiting'; "
                "local t=GetTitleText(); "
                "if QuestFrameDetailPanel and QuestFrameDetailPanel:IsVisible() and t==" + title + " then "
                "WOW_INTERNAL_DISCOVERY_STATE='accept'; AcceptQuest(); "
                "elseif QuestFrameGreetingPanel and QuestFrameGreetingPanel:IsVisible() then "
                "local n=GetNumAvailableQuests(); local f=0; "
                "for i=1,n do if GetAvailableTitle(i)==" + title + " then "
                "WOW_INTERNAL_DISCOVERY_STATE='greeting_select'; SelectAvailableQuest(i); f=1; break; end; end; "
                "if f==0 then WOW_INTERNAL_DISCOVERY_STATE='selected_not_found'; end; "
                "elseif GossipFrame and GossipFrame:IsVisible() then "
                "local a={GetGossipAvailableQuests()}; local q=1; local f=0; "
                "for i=1,table.getn(a),2 do if a[i]==" + title + " then "
                "WOW_INTERNAL_DISCOVERY_STATE='gossip_select'; SelectGossipAvailableQuest(q); f=1; break; end; q=q+1; end; "
                "if f==0 then WOW_INTERNAL_DISCOVERY_STATE='selected_not_found'; end; end";

            if (!RunDialogScript(script, result))
                return false;

            if (result != lastDialogResult_)
            {
                Debug::Logger::Info("QUEST PLANNER DISCOVERY dialog: " + result);
                lastDialogResult_ = result;
            }

            if (result == "accept")
            {
                Debug::Logger::Info("================================");
                Debug::Logger::Info("QUEST PLANNER DISCOVERY: AcceptQuest() issued.");
                Debug::Logger::Info("Selected questId=" + std::to_string(selectedProfile_->questId) +
                                    " title=\"" + selectedTitle_ + "\".");
                Debug::Logger::Info("Waiting for planner snapshot to verify a known active quest.");
                Debug::Logger::Info("================================");
                SetState(GenericQuestDiscoveryState::WaitingForQuestLog);
            }
            else if (result == "selected_not_found")
            {
                checkedGivers_.insert(giverGuid_);
                checkedGiverEntries_.insert(giverEntry_);
                selectedProfile_ = nullptr;
                selectedTitle_.clear();
                CloseQuestPanels();
                giverGuid_ = 0;
                giverEntry_ = 0;
                interactionAttempts_ = 0;
                giverNavigator_.reset();
                RefreshAuditLegDeadline(tick, "selected title disappeared; giver checked");
                SetState(GenericQuestDiscoveryState::FindingNearbyGiver);
            }

            lastDialogTick_ = tick;
            return true;
        }

    public:
        static bool Validate()
        {
            const bool valid =
                IsExecutable(OnRightClickUnitAddress()) &&
                IsExecutable(LuaDoStringAddress()) &&
                IsExecutable(GetTextAddress());

            Debug::Logger::Info(
                std::string("GenericQuestDiscovery validation ") +
                (valid ? "PASS." : "FAILED."));
            return valid;
        }

        bool Start(
            const QuestPlannerSnapshot& snapshot,
            const Objects::WorldState& world,
            std::uint64_t tick,
            const std::set<int>& completedQuestIds = {},
            const std::set<std::uint32_t>& precheckedGiverEntries = {})
        {
            if (state_ != GenericQuestDiscoveryState::Idle ||
                !snapshot.valid ||
                !Validate())
            {
                return false;
            }

            startTick_ = tick;
            lastInteractionTick_ = 0;
            lastDialogTick_ = tick;
            interactionAttempts_ = 0;
            giverGuid_ = 0;
            giverEntry_ = 0;
            selectedProfile_ = nullptr;
            selectedTitle_.clear();
            lastDialogResult_.clear();
            checkedGivers_.clear();
            checkedGiverEntries_ = precheckedGiverEntries;
            excludedQuestIds_ = completedQuestIds;
            activeHubEvidence_.clear();
            activeHubUnlockQuestId_ =
                ResolveActiveHubUnlockQuestId(snapshot, world, activeHubEvidence_);
            auditOriginX_ = world.player.x;
            auditOriginY_ = world.player.y;
            giverSeedEntry_ = 0;
            giverSeedProfile_ = nullptr;
            noNearbyOfferLogged_ = false;
            giverNavigator_.reset();
            directGiverStartTick_ = 0;
            directGiverCandidateStartTick_ = 0;
            directGiverBestDistance_ = 0.0f;
            directGiverCandidateIndex_ = 0;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUESTDB 13A: DATABASE HUB GIVER AUDIT START");
            Debug::Logger::Info("Policy: audit only unresolved eligible QuestDB/profile giver entries for the currently unlocked hub; preserve verified-empty giver entries and exclude completed quests.");
            Debug::Logger::Info(
                "Audit ledger: completedQuestIds=" + std::to_string(excludedQuestIds_.size()) +
                " precheckedGiverEntries=" + std::to_string(checkedGiverEntries_.size()));
            Debug::Logger::Info(
                "QUESTDB 13A.1: HUB CONTEXT activeUnlockQuestId=" +
                std::to_string(activeHubUnlockQuestId_) +
                " evidence=" + activeHubEvidence_);
            Debug::Logger::Info(
                "QUESTDB 13B: LOCAL HUB SCOPE origin=(" +
                std::to_string(auditOriginX_) + "," +
                std::to_string(auditOriginY_) + ") radius=" +
                std::to_string(ZoneHubAuditRadius));
            if (activeHubUnlockQuestId_ != 0 &&
                excludedQuestIds_.find(activeHubUnlockQuestId_) ==
                    excludedQuestIds_.end())
            {
                Debug::Logger::Info(
                    "QUESTDB 13A.1: HUB CONTEXT RECOVERY - completion ledger lacks unlock quest, but local giver evidence activates this hub without fabricating quest completion history.");
            }
            Debug::Logger::Info("================================");

            SetState(GenericQuestDiscoveryState::FindingNearbyGiver);
            return true;
        }

        void Update(
            const QuestPlannerSnapshot& snapshot,
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (state_ == GenericQuestDiscoveryState::Idle ||
                state_ == GenericQuestDiscoveryState::Done ||
                state_ == GenericQuestDiscoveryState::Failed ||
                state_ == GenericQuestDiscoveryState::WaitingForQuestLog)
            {
                return;
            }

            if (tick > startTick_ + OverallTimeoutTicks)
            {
                Fail("current database giver-audit leg timed out before it could be verified.");
                return;
            }

            if (state_ == GenericQuestDiscoveryState::NoNearbyOffer)
            {
                /*
                 * Phase 13D.5.1: an already-active quest must not terminate
                 * the pickup wave. Phase 13B.1 promises "accept everything
                 * available in the local hub first"; the legacy 11D.2.1
                 * shortcut violated that promise by ending discovery as soon
                 * as the first local quest appeared in the live quest log.
                 *
                 * Reaching NoNearbyOffer now means the deterministic local
                 * giver/seed queue is exhausted, regardless of how many
                 * quests are already active. That is the correct point to
                 * close the pickup wave and hand control to objectives.
                 */
                Debug::Logger::Info("================================");
                Debug::Logger::Info(
                    "QUESTDB 13D.5.1: PICKUP WAVE EXHAUSTED -> OBJECTIVE EXECUTION");
                Debug::Logger::Info(
                    "No eligible supported local giver/seed remains; active quests no longer short-circuit the hub pickup sweep.");
                Debug::Logger::Info("================================");
                SetState(GenericQuestDiscoveryState::Done);
                return;
            }

            if (state_ == GenericQuestDiscoveryState::FindingNearbyGiver)
            {
                /*
                 * Phase 13D.5.1: deliberately continue auditing while local
                 * quests are active. MarkKnownQuestActive() returns here after
                 * every verified pickup so another quest from the same NPC, or
                 * from the next giver, can be accepted into the same wave.
                 */

                // Phase 12B.6: consume the database seed queue first. This
                // makes the sweep deterministic and prevents a cluster of
                // already-completed nearby NPCs from starving a remote giver.
                const auto* auditProfile = FindNextGiverAuditProfile(snapshot, false);
                if (auditProfile != nullptr)
                {
                    const auto* liveAuditGiver =
                        FindVisibleGiverByEntry(world, auditProfile->giverEntry);

                    Debug::Logger::Info("================================");
                    Debug::Logger::Info("QUESTDB 12B.6: AUDIT NEXT UNIQUE GIVER");
                    Debug::Logger::Info(
                        "giverEntry=" + std::to_string(auditProfile->giverEntry) +
                        " questId=" + std::to_string(auditProfile->questId) +
                        " title=\"" + std::string(auditProfile->title) + "\"" +
                        " source=" + (liveAuditGiver != nullptr ? "live" : "database-seed"));
                    Debug::Logger::Info("================================");

                    if (liveAuditGiver != nullptr)
                    {
                        if (!StartGiverNavigation(world, *liveAuditGiver, tick))
                            Fail("failed to start deterministic live giver-audit approach.");
                    }
                    else if (!StartGiverSeedNavigation(world, *auditProfile, tick))
                    {
                        Fail("failed to start deterministic database giver-audit route.");
                    }
                    return;
                }

                /*
                 * Phase 13A: once every eligible local giver seed for the
                 * unlocked hub has been exhausted, deterministically audit a
                 * hub-exit giver. This is how Report to Orgnil (823) is picked
                 * up even when Master Gadrin is no longer inside WorldState.
                 * The exit quest still cannot execute ahead of local work: we
                 * only reach this branch after the non-exit seed queue is empty.
                 */
                const auto* exitProfile = FindNextGiverAuditProfile(snapshot, true);
                if (exitProfile != nullptr)
                {
                    const auto* liveExitGiver =
                        FindVisibleGiverByEntry(world, exitProfile->giverEntry);

                    Debug::Logger::Info("================================");
                    Debug::Logger::Info("QUESTDB 13A: LOCAL HUB AUDIT COMPLETE -> EXIT GIVER");
                    Debug::Logger::Info(
                        "giverEntry=" + std::to_string(exitProfile->giverEntry) +
                        " questId=" + std::to_string(exitProfile->questId) +
                        " title=\"" + std::string(exitProfile->title) + "\"" +
                        " source=" + (liveExitGiver != nullptr ? "live" : "database-seed"));
                    Debug::Logger::Info("================================");

                    if (liveExitGiver != nullptr)
                    {
                        if (!StartGiverNavigation(world, *liveExitGiver, tick))
                            Fail("failed to start deterministic hub-exit giver approach.");
                    }
                    else if (!StartGiverSeedNavigation(world, *exitProfile, tick))
                    {
                        Fail("failed to start deterministic hub-exit giver seed route.");
                    }
                    return;
                }

                // Fallback only for a hand-authored profile that has no DB
                // seed metadata. Database-backed profiles should normally be
                // exhausted before this branch is reached.
                const auto* giver = FindVisibleKnownGiver(snapshot, world);
                if (giver != nullptr)
                {
                    Debug::Logger::Info(
                        "QUESTDB 12B.6: AUDIT FALLBACK LIVE GIVER giverEntry=" +
                        std::to_string(giver->entryId));
                    if (!StartGiverNavigation(world, *giver, tick))
                        Fail("failed to start fallback profiled quest giver approach.");
                    return;
                }

                if (!checkedGivers_.empty() || !checkedGiverEntries_.empty())
                {
                    Debug::Logger::Info("================================");
                    Debug::Logger::Info("QUESTDB 13A: HUB GIVER AUDIT COMPLETE");
                    Debug::Logger::Info(
                        "All deterministic database giver seeds inside the Phase 13B local-hub scope plus visible fallback givers were checked with no additional eligible offer.");
                    Debug::Logger::Info(
                        "Checked giver instances=" + std::to_string(checkedGivers_.size()) +
                        " entries=" + std::to_string(checkedGiverEntries_.size()) + ".");
                    const int remoteProfiles = CountRemoteEligibleProfiles(snapshot);
                    Debug::Logger::Info(
                        "QUESTDB 13B: REMOTE DUROTAR PROFILES REMAIN count=" +
                        std::to_string(remoteProfiles) +
                        "; remote profiles are intentionally not pulled into the current hub sweep.");
                    Debug::Logger::Info("================================");
                    SetState(GenericQuestDiscoveryState::Done);
                    return;
                }

                if (!noNearbyOfferLogged_)
                {
                    noNearbyOfferLogged_ = true;
                    Debug::Logger::Info("================================");
                    Debug::Logger::Info("QUEST PLANNER 11D.2: NO PROFILED QUEST GIVER VISIBLE");
                    Debug::Logger::Info(
                        "No eligible profiled giver or database giver seed remains for the current sweep.");
                    Debug::Logger::Info("================================");
                }
                SetState(GenericQuestDiscoveryState::NoNearbyOffer);
                return;
            }

            if (state_ == GenericQuestDiscoveryState::TravelingToGiverSeed)
            {
                const auto* liveGiver = FindVisibleGiverByEntry(world, giverSeedEntry_);
                if (liveGiver != nullptr)
                {
                    Debug::Logger::Info("================================");
                    Debug::Logger::Info("QUESTDB 12B.6: AUDIT GIVER ACQUIRED LIVE");
                    Debug::Logger::Info(
                        "Giver entry=" + std::to_string(giverSeedEntry_) +
                        " distance=" + std::to_string(liveGiver->distance));
                    Debug::Logger::Info("================================");
                    giverNavigator_.reset();
                    if (!StartGiverNavigation(world, *liveGiver, tick))
                        Fail("failed to hand off giver-audit seed to live giver navigation.");
                    return;
                }

                if (!giverNavigator_)
                {
                    Fail("profiled giver-audit navigation state is missing.");
                    return;
                }

                giverNavigator_->Update(world.player, tick);
                if (giverNavigator_->Failed())
                {
                    DeferCurrentGiverSeedForWave(
                        tick,
                        "NavMesh route to configured profiled giver-audit seed failed after bounded navigation/recovery.");
                    return;
                }

                if (giverNavigator_->Arrived())
                {
                    // One final live scan at the configured giver location.
                    liveGiver = FindVisibleGiverByEntry(world, giverSeedEntry_);
                    if (liveGiver != nullptr)
                    {
                        giverNavigator_.reset();
                        if (!StartGiverNavigation(world, *liveGiver, tick))
                            Fail("failed to interact with giver acquired at audit destination.");
                        return;
                    }

                    Debug::Logger::Info("================================");
                    Debug::Logger::Info("QUESTDB 12B.6: GIVER AUDIT SEED EMPTY");
                    Debug::Logger::Info(
                        "No live giver entry=" + std::to_string(giverSeedEntry_) +
                        " was present after reaching its configured seed; marking this entry checked for the current sweep only.");
                    Debug::Logger::Info("================================");
                    checkedGiverEntries_.insert(giverSeedEntry_);
                    giverSeedEntry_ = 0;
                    giverSeedProfile_ = nullptr;
                    giverNavigator_.reset();
                    RefreshAuditLegDeadline(tick, "giver seed checked empty");
                    SetState(GenericQuestDiscoveryState::FindingNearbyGiver);
                }
                return;
            }

            if (state_ == GenericQuestDiscoveryState::TravelingToGiver)
            {
                const auto* giver = FindGiverByGuid(world, giverGuid_);
                if (giver == nullptr)
                {
                    DeferCurrentGiverApproachForWave(
                        tick,
                        "profiled quest giver disappeared during the bounded NavMesh approach.");
                    return;
                }

                if (!giverNavigator_)
                {
                    DeferCurrentGiverApproachForWave(
                        tick,
                        "profiled quest giver navigation state is missing; continuing the remaining pickup-wave audit.");
                    return;
                }

                giverNavigator_->Update(world.player, tick);

                if (giverNavigator_->Failed())
                {
                    if (StartDirectGiverApproach(
                            world,
                            *giver,
                            tick,
                            "NavMesh approach to profiled quest giver failed inside the close-giver fallback radius."))
                    {
                        return;
                    }

                    DeferCurrentGiverApproachForWave(
                        tick,
                        "NavMesh approach to profiled quest giver failed and the live giver is outside/unreachable by the bounded direct fallback.");
                    return;
                }

                if (giverNavigator_->Arrived() || giver->distance <= InteractionDistance)
                {
                    if (giver->distance > InteractionDistance)
                    {
                        if (StartDirectGiverApproach(
                                world,
                                *giver,
                                tick,
                                "NavMesh reached the giver handoff but the live NPC remained outside interaction distance."))
                        {
                            return;
                        }

                        Fail("NavMesh reached giver handoff but live NPC remains outside interaction distance.");
                        return;
                    }

                    if (!IssueInteraction(*giver, tick))
                    {
                        Fail("failed to interact after NavMesh approach to profiled quest giver.");
                        return;
                    }
                }
                return;
            }

            if (state_ == GenericQuestDiscoveryState::DirectApproachToGiver)
            {
                const auto* giver = FindGiverByGuid(world, giverGuid_);
                if (giver == nullptr)
                {
                    Fail("profiled quest giver disappeared during direct close-giver fallback.");
                    return;
                }

                if (giver->distance <= InteractionDistance)
                {
                    Debug::Logger::Info("================================");
                    Debug::Logger::Info("QUEST DISCOVERY 13D.5.3: DIRECT GIVER APPROACH REACHED INTERACTION RANGE");
                    Debug::Logger::Info(
                        "giverEntry=" + std::to_string(giver->entryId) +
                        " distance=" + std::to_string(giver->distance));
                    Debug::Logger::Info("================================");

                    if (!IssueInteraction(*giver, tick))
                    {
                        Fail("failed to interact after direct close-giver fallback.");
                    }
                    return;
                }

                if (giver->distance + DirectGiverProgressEpsilon <
                    directGiverBestDistance_)
                {
                    directGiverBestDistance_ = giver->distance;
                    directGiverCandidateStartTick_ = tick;

                    Debug::Logger::Info(
                        "QUEST DISCOVERY 13D.5.3: DIRECT GIVER PROGRESS distance=" +
                        std::to_string(giver->distance) +
                        " best=" + std::to_string(directGiverBestDistance_));
                }

                if (tick >= directGiverStartTick_ + DirectGiverMaximumTicks)
                {
                    DeferCurrentGiverApproachForWave(
                        tick,
                        "direct close-giver fallback exhausted its bounded total timeout.");
                    return;
                }

                if (tick >=
                    directGiverCandidateStartTick_ + DirectGiverCandidateTicks)
                {
                    const int nextCandidate = directGiverCandidateIndex_ + 1;
                    if (nextCandidate >= DirectGiverCandidateCount)
                    {
                        DeferCurrentGiverApproachForWave(
                            tick,
                            "direct close-giver fallback exhausted all bounded interaction-envelope candidates.");
                        return;
                    }

                    if (!IssueDirectGiverCandidate(
                            world,
                            *giver,
                            tick,
                            nextCandidate))
                    {
                        DeferCurrentGiverApproachForWave(
                            tick,
                            "failed to issue next direct close-giver interaction-envelope candidate.");
                    }
                }
                return;
            }

            if (state_ == GenericQuestDiscoveryState::AdvancingDialog)
            {
                if (tick >= lastDialogTick_ + DialogStepTicks)
                {
                    if (!ReadOrAdvanceDialog(snapshot, tick))
                    {
                        Fail("failed to inspect or advance quest discovery dialog.");
                        return;
                    }
                }

                if (state_ == GenericQuestDiscoveryState::AdvancingDialog &&
                    tick >= lastInteractionTick_ + ReinteractTicks)
                {
                    if (interactionAttempts_ >= MaximumInteractionAttempts)
                    {
                        Debug::Logger::Info(
                            "QUESTDB 12B.6: GIVER CHECK COMPLETE - no usable quest dialog after bounded retries; giverEntry=" +
                            std::to_string(giverEntry_) +
                            " attempts=" + std::to_string(interactionAttempts_));
                        checkedGivers_.insert(giverGuid_);
                        checkedGiverEntries_.insert(giverEntry_);
                        CloseQuestPanels();
                        giverGuid_ = 0;
                        giverEntry_ = 0;
                        interactionAttempts_ = 0;
                        selectedProfile_ = nullptr;
                        selectedTitle_.clear();
                        RefreshAuditLegDeadline(tick, "giver interaction attempts exhausted");
                        SetState(GenericQuestDiscoveryState::FindingNearbyGiver);
                        return;
                    }

                    const auto* giver = FindGiverByGuid(world, giverGuid_);
                    if (giver == nullptr || giver->distance > InteractionDistance)
                    {
                        checkedGivers_.insert(giverGuid_);
                        checkedGiverEntries_.insert(giverEntry_);
                        giverGuid_ = 0;
                        giverEntry_ = 0;
                        interactionAttempts_ = 0;
                        selectedProfile_ = nullptr;
                        selectedTitle_.clear();
                        RefreshAuditLegDeadline(tick, "giver left interaction range; entry checked");
                        SetState(GenericQuestDiscoveryState::FindingNearbyGiver);
                        return;
                    }

                    if (!IssueInteraction(*giver, tick))
                        Fail("failed to retry quest giver interaction.");
                }
            }
        }

        void MarkKnownQuestActive(
            const QuestProfile& actualProfile,
            std::uint64_t tick)
        {
            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST PLANNER 11D.2: DISCOVERY/PICKUP PASS");
            Debug::Logger::Info("Planner snapshot now contains questId=" +
                                std::to_string(actualProfile.questId) +
                                " title=\"" + actualProfile.title + "\".");
            if (selectedProfile_ != nullptr)
            {
                Debug::Logger::Info("Originally selected offered profile questId=" +
                                    std::to_string(selectedProfile_->questId) +
                                    " title=\"" + selectedProfile_->title + "\".");
            }
            Debug::Logger::Info(
                "Phase 13B.1 wave policy: pickup verified; discovery keeps ownership until every eligible supported offer in the current local hub has been audited.");
            Debug::Logger::Info(
                "Objectives do not start until the pickup wave is closed, preventing accept-one/run-one routing churn.");
            Debug::Logger::Info("================================");

            CloseQuestPanels();
            selectedProfile_ = nullptr;
            selectedTitle_.clear();
            lastDialogResult_.clear();
            giverGuid_ = 0;
            giverEntry_ = 0;
            interactionAttempts_ = 0;
            lastInteractionTick_ = 0;
            giverNavigator_.reset();
            directGiverStartTick_ = 0;
            directGiverCandidateStartTick_ = 0;
            directGiverBestDistance_ = 0.0f;
            directGiverCandidateIndex_ = 0;
            noNearbyOfferLogged_ = false;
            startTick_ = tick;

            /*
             * Phase 13B.1: continue the same deterministic giver sweep after
             * each verified pickup.  We intentionally do NOT mark the giver
             * entry checked here: reopening it allows another simultaneously
             * available quest from the same NPC to be accepted.  Once no
             * eligible offer remains, the normal bounded dialog path marks the
             * giver checked and advances to the next seed.
             */
            Debug::Logger::Info(
                "QUESTDB 13B.1: PICKUP ADDED TO WAVE -> CONTINUE HUB SWEEP");
            SetState(GenericQuestDiscoveryState::FindingNearbyGiver);
        }

        const QuestProfile* ActivatedProfile(
            const QuestPlannerSnapshot& snapshot) const
        {
            return ResolveActivatedProfile(snapshot);
        }

        bool WaitingForQuestLog() const
        {
            return state_ == GenericQuestDiscoveryState::WaitingForQuestLog;
        }

        const QuestProfile* SelectedProfile() const
        {
            return selectedProfile_;
        }

        const std::set<std::uint32_t>& CheckedGiverEntries() const
        {
            return checkedGiverEntries_;
        }

        bool OwnsControl() const
        {
            return state_ != GenericQuestDiscoveryState::Idle &&
                   state_ != GenericQuestDiscoveryState::Done &&
                   state_ != GenericQuestDiscoveryState::Failed;
        }

        bool Done() const
        {
            return state_ == GenericQuestDiscoveryState::Done;
        }

        bool Failed() const
        {
            return state_ == GenericQuestDiscoveryState::Failed;
        }

        const char* StateName() const
        {
            return StateNameInternal(state_);
        }
    };
}
