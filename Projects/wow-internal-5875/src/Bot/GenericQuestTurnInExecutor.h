#pragma once

#include "ClassAwareRewardController.h"
#include "ClickToMoveController.h"
#include "GameThreadDispatcher.h"
#include "QuestPlannerTypes.h"
#include "QuestTurnInAlternatePolicy.h"
#include "QuestAcquisitionPolicy.h"
#include "ObjectiveDefensiveCombatGuard.h"
#include "ValleyQuestNpcDestinations.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"
#include "../Navigation/GenericNavMeshPathFollower.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace Bot
{
    enum class GenericQuestTurnInState
    {
        Idle,
        FindingNpc,
        RoutingToNpc,
        SelectingAlternateApproach,
        RoutingAlternateApproach,
        ApproachingNpc,
        AdvancingDialog,
        EvaluatingReward,
        RewardChoiceRequired,
        WaitingForQuestRemoval,
        Done,
        Failed
    };

    class GenericQuestTurnInExecutor
    {
    private:
        static constexpr std::uintptr_t OnRightClickUnitRva = 0x0020BEA0;
        static constexpr std::uintptr_t LuaDoStringRva = 0x00304CD0;
        static constexpr std::uintptr_t GetTextRva = 0x00303BF0;
        static constexpr std::uintptr_t ObjectManagerRootRva = 0x00741414;
        static constexpr std::uintptr_t FirstObjectOffset = 0x000000AC;
        static constexpr std::uintptr_t ObjectGuidOffset = 0x00000030;
        static constexpr std::uintptr_t ObjectNextOffset = 0x0000003C;

        static constexpr float InteractionDistance = 4.50f;
        static constexpr std::uint64_t MoveCooldownTicks = 4;
        static constexpr std::uint64_t InteractionRetryTicks = 8;
        static constexpr std::uint64_t DialogStepTicks = 4;
        static constexpr std::uint64_t NpcSearchTimeoutTicks = 80;
        static constexpr std::uint64_t SeedArrivalGraceTicks = 48;
        static constexpr std::uint64_t OverallTimeoutTicks = 900;
        static constexpr float TurnInSeedArrivalDistance = 30.0f;
        static constexpr float LiveNpcNavArrivalDistance = 3.25f;
        static constexpr float DirectApproachFallbackDistance = 10.0f;
        static constexpr float LiveNpcReplanDistance = 2.0f;
        static constexpr int MaximumMoveCommands = 8;
        static constexpr int MaximumInteractionAttempts = 6;
        static constexpr const char* ResultVariable = "WOW_INTERNAL_GENERIC_TURNIN_STATE";

        GenericQuestTurnInState state_ = GenericQuestTurnInState::Idle;
        const QuestProfile* profile_ = nullptr;
        QuestProfile actorProfile_{};
        bool actorInitialized_=false, defenseHeld_=false;
        ObjectiveDefensiveCombatGuard defense_{};
        Objects::PlayerState lastPlayer_{};
        std::uint64_t startTick_ = 0;
        std::uint64_t lastMoveTick_ = 0;
        std::uint64_t lastInteractionTick_ = 0;
        std::uint64_t lastDialogTick_ = 0;
        int moveCommands_ = 0;
        int interactionAttempts_ = 0;
        int dialogActions_ = 0;
        int rewardChoices_ = 0;
        std::uint64_t npcGuid_ = 0;
        std::string lastDialogResult_{};
        bool questRemovalObserved_ = false;
        bool noChoiceRewardClaimIssued_ = false;
        bool turnInSeedReached_ = false;
        std::uint64_t turnInSeedReachedTick_ = 0;
        bool routingToLiveNpc_ = false;
        Navigation::NavPoint liveNpcDestination_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> turnInNavigator_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> alternateProbe_{};
        Objects::PlayerState alternateProbeOrigin_{};
        Navigation::NavPoint alternateFailurePosition_{};
        Navigation::NavPoint alternateFinalDestination_{};
        Navigation::NavPoint alternateStageDestination_{};
        Navigation::DirectedPolyTransition inheritedBadEdge_{};
        std::array<Navigation::NavPoint,
            QuestTurnInAlternatePolicy::MaximumCandidates> alternateCandidates_{};
        std::vector<Navigation::NavPoint> triedAlternateStages_{};
        Navigation::NavigationFailureEvidence lastRouteEvidence_{};
        Navigation::NavigationFailureEvidence primaryRouteEvidence_{};
        std::size_t alternateCandidateIndex_ = 0;
        std::uint64_t alternateProbeStartTick_ = 0;
        std::uint32_t alternateMapId_ = 1;
        int strategicLegs_ = 0;
        ClassAwareRewardController rewardController_{};

        static std::string Hex32(std::uintptr_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << std::uppercase
                   << std::setw(8) << std::setfill('0')
                   << static_cast<std::uint32_t>(value);
            return stream.str();
        }

        static std::string Hex64(std::uint64_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << std::uppercase
                   << std::setw(16) << std::setfill('0') << value;
            return stream.str();
        }

        static std::string Float(float value)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3) << value;
            return stream.str();
        }

        static const char* StateNameInternal(GenericQuestTurnInState state)
        {
            switch (state)
            {
                case GenericQuestTurnInState::Idle: return "Idle";
                case GenericQuestTurnInState::FindingNpc: return "FindingNpc";
                case GenericQuestTurnInState::RoutingToNpc: return "RoutingToNpc";
                case GenericQuestTurnInState::SelectingAlternateApproach:
                    return "SelectingAlternateApproach";
                case GenericQuestTurnInState::RoutingAlternateApproach:
                    return "RoutingAlternateApproach";
                case GenericQuestTurnInState::ApproachingNpc: return "ApproachingNpc";
                case GenericQuestTurnInState::AdvancingDialog: return "AdvancingDialog";
                case GenericQuestTurnInState::EvaluatingReward: return "EvaluatingReward";
                case GenericQuestTurnInState::RewardChoiceRequired: return "RewardChoiceRequired";
                case GenericQuestTurnInState::WaitingForQuestRemoval: return "WaitingForQuestRemoval";
                case GenericQuestTurnInState::Done: return "Done";
                case GenericQuestTurnInState::Failed: return "Failed";
                default: return "Unknown";
            }
        }

        void SetState(GenericQuestTurnInState next)
        {
            if (state_ == next)
                return;

            Debug::Logger::Info(
                std::string("GenericQuestTurnIn state: ") +
                StateNameInternal(state_) + " -> " + StateNameInternal(next));
            state_ = next;
        }

        void Fail(const std::string& reason)
        {
            if(lastPlayer_.valid) MovementController::HoldPosition(lastPlayer_);
            turnInNavigator_.reset();
            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST PLANNER 11C: TURN-IN FAILED");
            Debug::Logger::Info("Reason: " + reason);
            if (profile_ != nullptr)
            {
                Debug::Logger::Info(
                    "Quest: " + std::to_string(profile_->questId) +
                    " " + profile_->title);
                Debug::Logger::Info(
                    "Turn-in entry: " + std::to_string(profile_->turnInEntry));
            }
            Debug::Logger::Info("NPC GUID: " + Hex64(npcGuid_));
            Debug::Logger::Info("================================");
            SetState(GenericQuestTurnInState::Failed);
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

            const DWORD protect = info.Protect & 0xFF;
            return protect == PAGE_EXECUTE ||
                   protect == PAGE_EXECUTE_READ ||
                   protect == PAGE_EXECUTE_READWRITE ||
                   protect == PAGE_EXECUTE_WRITECOPY;
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
                if (!ReadValue(static_cast<std::uintptr_t>(current) + ObjectNextOffset, next))
                    break;
                if (next == current)
                    break;
                current = next;
            }

            return 0;
        }

        const Objects::UnitState* FindTurnInNpc(const Objects::WorldState& world) const
        {
            if (profile_ == nullptr || profile_->turnInEntry == 0)
                return nullptr;

            const Objects::UnitState* best = nullptr;
            for (const auto& unit : world.units)
            {
                if (!unit.valid || unit.guid == 0 || unit.entryId != profile_->turnInEntry)
                    continue;
                if (best == nullptr || unit.distance < best->distance ||
                    (unit.distance==best->distance && unit.guid<best->guid))
                    best = &unit;
            }
            return best;
        }

        bool ResolveTurnInSeed(
            Navigation::NavPoint& destination,
            std::uint32_t& mapId) const
        {
            if (profile_ == nullptr)
                return false;
            if (profile_->turnInDestination.valid)
            {
                const auto& seed = profile_->turnInDestination;
                destination = {seed.x, seed.y, seed.z};
                mapId = seed.mapId;
                return true;
            }
            const auto* seed =
                ValleyQuestNpcDestinations::Find(profile_->turnInEntry);
            if (seed == nullptr)
                return false;
            destination = {seed->x, seed->y, seed->z};
            mapId = seed->mapId;
            return true;
        }

        bool BeginAlternateAfterRouteFailure(
            const Objects::WorldState& world,
            std::uint64_t tick,
            const char* leg)
        {
            if (turnInNavigator_ == nullptr || profile_ == nullptr ||
                !turnInNavigator_->Failed())
                return false;
            lastRouteEvidence_ = turnInNavigator_->FailureEvidence();
            if (strategicLegs_ == 0)
                primaryRouteEvidence_ = lastRouteEvidence_;
            if (!inheritedBadEdge_.Valid() &&
                lastRouteEvidence_.learnedTransition.Valid())
                inheritedBadEdge_ = lastRouteEvidence_.learnedTransition;

            alternateFailurePosition_ = {
                world.player.x, world.player.y, world.player.z};
            alternateProbeOrigin_ = world.player;
            const auto* liveNpc = FindTurnInNpc(world);
            if (liveNpc != nullptr)
            {
                alternateFinalDestination_ = {
                    liveNpc->x, liveNpc->y, liveNpc->z};
                alternateMapId_ = 1;
            }
            else if (routingToLiveNpc_)
            {
                alternateFinalDestination_ = liveNpcDestination_;
                alternateMapId_ = 1;
            }
            else if (!ResolveTurnInSeed(
                         alternateFinalDestination_, alternateMapId_))
            {
                return false;
            }

            Debug::Logger::Info(
                "QUEST 16B.3 ROUTE FAILURE questId=" +
                std::to_string(profile_->questId) +
                " phase=turnin leg=" + leg +
                " attempt=" + std::to_string(strategicLegs_) +
                " failureReason=" +
                Navigation::NavigationInitTelemetryPolicy::ReasonName(
                    lastRouteEvidence_.reason) +
                " failurePosition=(" + Float(world.player.x) + "," +
                Float(world.player.y) + "," + Float(world.player.z) + ")" +
                " destinationEntry=" + std::to_string(profile_->turnInEntry) +
                " failedFromPoly=" + Hex64(lastRouteEvidence_.failedTransition.from) +
                " failedToPoly=" + Hex64(lastRouteEvidence_.failedTransition.to) +
                " learnedFromPoly=" + Hex64(inheritedBadEdge_.from) +
                " learnedToPoly=" + Hex64(inheritedBadEdge_.to) +
                " corridorFingerprint=" +
                std::to_string(lastRouteEvidence_.corridorFingerprint));
            turnInNavigator_.reset();
            alternateProbe_.reset();

            if (!QuestTurnInAlternatePolicy::MayRecover(
                    true, lastRouteEvidence_.reason, strategicLegs_))
                return false;
            if (!QuestTurnInAlternatePolicy::CanGenerate(
                    alternateFailurePosition_, alternateFinalDestination_))
                return false;
            alternateCandidates_ = QuestTurnInAlternatePolicy::Generate(
                alternateFailurePosition_, alternateFinalDestination_);
            alternateCandidateIndex_ = 0;
            SetState(GenericQuestTurnInState::SelectingAlternateApproach);
            return true;
        }

        void AdvanceAlternateSelection(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            const auto* liveNpc = FindTurnInNpc(world);
            if (liveNpc != nullptr)
            {
                alternateFinalDestination_ = {
                    liveNpc->x, liveNpc->y, liveNpc->z};
                if (liveNpc->distance <= InteractionDistance)
                {
                    alternateProbe_.reset();
                    SetState(GenericQuestTurnInState::FindingNpc);
                    return;
                }
            }

            if (alternateCandidateIndex_ >= alternateCandidates_.size())
            {
                Fail("no bounded reachable alternate turn-in approach remained.");
                return;
            }

            const Navigation::NavPoint candidate =
                alternateCandidates_[alternateCandidateIndex_];
            if (!alternateProbe_)
            {
                alternateProbe_ = std::make_unique<
                    Navigation::GenericNavMeshPathFollower>();
                alternateProbeStartTick_ = tick;
                const Navigation::GenericNavMeshStartOptions options{
                    false, true, inheritedBadEdge_};
                if (!alternateProbe_->Start(
                        alternateProbeOrigin_, tick, candidate,
                        alternateMapId_, 2.0f,
                        "quest turn-in alternate planning probe", true,
                        options))
                {
                    alternateProbe_.reset();
                    ++alternateCandidateIndex_;
                }
                return;
            }

            alternateProbe_->Update(alternateProbeOrigin_, tick);
            const auto result = alternateProbe_->PlanningOnlyResult();
            const bool timedOut = tick >= alternateProbeStartTick_ &&
                tick - alternateProbeStartTick_ >=
                    QuestTurnInAlternatePolicy::MaximumProbeTicks;
            if (result.status == Navigation::RouteCostProbeStatus::Pending &&
                !timedOut)
                return;

            const auto& hazards =
                Navigation::NavigationHazardMemory::Instance();
            const float currentRisk = hazards.RiskAt(
                alternateMapId_, alternateFailurePosition_);
            const float candidateRisk = hazards.RiskAt(
                alternateMapId_, candidate);
            const bool complete =
                result.status == Navigation::RouteCostProbeStatus::Reachable &&
                alternateProbe_->PlanningOnlyReachedDestination();
            const Navigation::DirectedPolyTransition rejectedEdge =
                inheritedBadEdge_.Valid()
                    ? inheritedBadEdge_ : primaryRouteEvidence_.failedTransition;
            const bool crossesFailedEdge =
                alternateProbe_->PlanningOnlyCorridorContainsTransition(
                    rejectedEdge) ||
                alternateProbe_->PlanningOnlyCorridorContainsTransition(
                    lastRouteEvidence_.failedTransition);
            const bool accepted = QuestTurnInAlternatePolicy::Accept(
                alternateFailurePosition_, candidate, result.pathLength,
                complete, crossesFailedEdge, currentRisk, candidateRisk,
                triedAlternateStages_);
            Debug::Logger::Info(
                "QUEST 16B.3 ALTERNATE decision=" +
                std::string(accepted ? "selected" : "rejected") +
                " candidate=(" + Float(candidate.x) + "," +
                Float(candidate.y) + "," + Float(candidate.z) + ")" +
                " distance=" + Float(QuestTurnInAlternatePolicy::Distance2D(
                    alternateFailurePosition_, candidate)) +
                " routeCost=" + Float(result.pathLength) +
                " complete=" + (complete ? "yes" : "no") +
                " crossesFailedTransition=" +
                (crossesFailedEdge ? "yes" : "no") +
                " reason=" + (timedOut ? "probe_timeout" :
                    accepted ? "bounded_reachable_egress" :
                    "unsafe_or_unreachable_egress"));
            alternateProbe_.reset();
            ++alternateCandidateIndex_;
            if (!accepted)
                return;

            turnInNavigator_ = std::make_unique<
                Navigation::GenericNavMeshPathFollower>();
            const Navigation::GenericNavMeshStartOptions options{
                false, false, inheritedBadEdge_};
            if (!turnInNavigator_->Start(
                    world.player, tick, candidate, alternateMapId_, 2.5f,
                    "quest turn-in strategic local egress", true, options))
            {
                turnInNavigator_.reset();
                return;
            }
            alternateStageDestination_ = candidate;
            triedAlternateStages_.push_back(candidate);
            ++strategicLegs_;
            SetState(GenericQuestTurnInState::RoutingAlternateApproach);
        }

        void UpdateAlternateApproach(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            const auto* liveNpc = FindTurnInNpc(world);
            if (liveNpc != nullptr && liveNpc->distance <= InteractionDistance)
            {
                turnInNavigator_.reset();
                SetState(GenericQuestTurnInState::FindingNpc);
                return;
            }
            if (turnInNavigator_ == nullptr)
            {
                Fail("strategic turn-in approach lost navigation ownership.");
                return;
            }
            turnInNavigator_->Update(world.player, tick);
            if (turnInNavigator_->Failed())
            {
                if (!BeginAlternateAfterRouteFailure(world, tick, "egress"))
                    Fail("bounded strategic turn-in approaches exhausted.");
                return;
            }
            if (!turnInNavigator_->Arrived())
                return;
            turnInNavigator_.reset();
            if (!QuestTurnInAlternatePolicy::StageReached(
                    alternateFailurePosition_,
                    Navigation::NavPoint{
                        world.player.x, world.player.y, world.player.z}))
            {
                Fail("alternate turn-in approach arrived without meaningful displacement.");
                return;
            }

            Debug::Logger::Info(
                "QUEST 16B.3 RETRY attempt=" +
                std::to_string(strategicLegs_) + " from=(" +
                Float(world.player.x) + "," + Float(world.player.y) + "," +
                Float(world.player.z) + ") destinationEntry=" +
                std::to_string(profile_->turnInEntry) +
                " stage=(" + Float(alternateStageDestination_.x) + "," +
                Float(alternateStageDestination_.y) + "," +
                Float(alternateStageDestination_.z) + ")");
            if (liveNpc != nullptr)
            {
                if (!StartLiveNpcNavigation(
                        world, *liveNpc, tick, "strategic alternate reached"))
                    Fail("failed to start turn-in NPC route after alternate approach.");
            }
            else if (routingToLiveNpc_)
            {
                // The last live position remains a search hint only. If the
                // NPC reappears the normal live-XYZ handoff takes precedence.
                turnInNavigator_ = std::make_unique<
                    Navigation::GenericNavMeshPathFollower>();
                const Navigation::GenericNavMeshStartOptions options{
                    true, false, inheritedBadEdge_};
                if (!turnInNavigator_->Start(
                        world.player, tick, liveNpcDestination_, 1,
                        LiveNpcNavArrivalDistance,
                        "last live turn-in NPC position", true, options))
                    Fail("failed to retry last live turn-in NPC position.");
                else
                    SetState(GenericQuestTurnInState::RoutingToNpc);
            }
            else if (!StartTurnInSeedNavigation(world, tick))
            {
                Fail("failed to retry turn-in search seed after alternate approach.");
            }
        }


        bool StartTurnInSeedNavigation(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (profile_ == nullptr)
                return false;

            float seedX = 0.0f;
            float seedY = 0.0f;
            float seedZ = 0.0f;
            std::uint32_t seedMapId = 1;
            std::string seedLabel;
            bool databaseSeed = false;

            if (profile_->turnInDestination.valid)
            {
                seedX = profile_->turnInDestination.x;
                seedY = profile_->turnInDestination.y;
                seedZ = profile_->turnInDestination.z;
                seedMapId = profile_->turnInDestination.mapId;
                seedLabel = profile_->turnInDestination.label == nullptr
                    ? "QuestDB turn-in spawn"
                    : profile_->turnInDestination.label;
                databaseSeed = profile_->databaseDerived;
            }
            else
            {
                const auto* seed =
                    ValleyQuestNpcDestinations::Find(profile_->turnInEntry);
                if (seed == nullptr)
                    return false;

                seedX = seed->x;
                seedY = seed->y;
                seedZ = seed->z;
                seedMapId = seed->mapId;
                seedLabel = seed->label;
            }

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST PLANNER 12B: TURN-IN NPC NOT VISIBLE - NAVMESH SEARCH");
            Debug::Logger::Info("Turn-in entry: " + std::to_string(profile_->turnInEntry));
            Debug::Logger::Info("Seed source: " + std::string(databaseSeed ? "QuestDB" : "legacy profile"));
            Debug::Logger::Info("Seed: " + seedLabel);
            Debug::Logger::Info(
                "Seed position: (" + Float(seedX) + "," +
                Float(seedY) + "," + Float(seedZ) + ")");
            Debug::Logger::Info("Policy: seed is search-only; live NPC XYZ overrides immediately when visible.");
            Debug::Logger::Info("================================");

            turnInNavigator_ =
                std::make_unique<Navigation::GenericNavMeshPathFollower>();

            const Navigation::NavPoint destination{
                seedX, seedY, seedZ
            };

            const Navigation::GenericNavMeshStartOptions options{
                true, false, inheritedBadEdge_};

            if (!turnInNavigator_->Start(
                    world.player,
                    tick,
                    destination,
                    seedMapId,
                    TurnInSeedArrivalDistance,
                    std::string("turn-in search seed: ") + seedLabel,
                    true, options))
            {
                turnInNavigator_.reset();
                return false;
            }

            SetState(GenericQuestTurnInState::RoutingToNpc);
            return true;
        }


        static float Distance2D(float ax, float ay, float bx, float by)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            return std::sqrt(dx * dx + dy * dy);
        }

        bool StartLiveNpcNavigation(
            const Objects::WorldState& world,
            const Objects::UnitState& npc,
            std::uint64_t tick,
            const char* reason)
        {
            turnInNavigator_.reset();
            turnInNavigator_ =
                std::make_unique<Navigation::GenericNavMeshPathFollower>();

            liveNpcDestination_ = Navigation::NavPoint{npc.x, npc.y, npc.z};

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST PLANNER 11C.3: LIVE TURN-IN NPC NAVMESH HANDOFF");
            Debug::Logger::Info("Reason: " + std::string(reason));
            Debug::Logger::Info("Entry: " + std::to_string(npc.entryId));
            Debug::Logger::Info("GUID: " + Hex64(npc.guid));
            Debug::Logger::Info(
                "Live position: (" + Float(npc.x) + "," +
                Float(npc.y) + "," + Float(npc.z) + ")");
            Debug::Logger::Info("Live distance: " + Float(npc.distance));
            Debug::Logger::Info("================================");

            if (!turnInNavigator_->Start(
                    world.player,
                    tick,
                    liveNpcDestination_,
                    1,
                    LiveNpcNavArrivalDistance,
                    "live turn-in NPC entry " + std::to_string(npc.entryId),
                    true,
                    Navigation::GenericNavMeshStartOptions{
                        true, false, inheritedBadEdge_}))
            {
                turnInNavigator_.reset();
                routingToLiveNpc_ = false;
                return false;
            }

            routingToLiveNpc_ = true;
            SetState(GenericQuestTurnInState::RoutingToNpc);
            return true;
        }

        bool IssueApproach(
            const Objects::PlayerState& player,
            const Objects::UnitState& npc,
            std::uint64_t tick)
        {
            if (moveCommands_ >= MaximumMoveCommands)
                return false;

            Debug::Logger::Info("QUEST PLANNER 11C: approaching turn-in NPC.");
            Debug::Logger::Info("Entry: " + std::to_string(npc.entryId));
            Debug::Logger::Info("Distance: " + Float(npc.distance));

            if (!ClickToMoveController::MoveTo(
                    player, npc.x, npc.y, npc.z, 1.25f))
            {
                return false;
            }

            ++moveCommands_;
            lastMoveTick_ = tick;
            SetState(GenericQuestTurnInState::ApproachingNpc);
            return true;
        }

    public:
        // Shared build-5875 interaction primitive; no dialogue or quest policy.
        static bool InteractUnitGuid(std::uint64_t guid)
        {
            const auto functionAddress = OnRightClickUnitAddress();
            if (!IsExecutable(functionAddress))
                return false;
            const std::uintptr_t objectAddress = FindObjectAddressByGuid(guid);
            if (objectAddress == 0)
                return false;

            using OnRightClickUnitFunction = void (__thiscall*)(std::uint32_t, int);
            const auto onRightClickUnit =
                reinterpret_cast<OnRightClickUnitFunction>(functionAddress);

            bool onGameThread = false;
            const std::uint32_t npcThis = static_cast<std::uint32_t>(objectAddress);

            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    if (onGameThread)
                        onRightClickUnit(npcThis, 0);
                });

            return dispatched && onGameThread;
        }

        static bool RunQuestLua(const std::string& script, const char* variable, std::string& output)
        {
            if (!IsExecutable(LuaDoStringAddress()) || !IsExecutable(GetTextAddress())) return false;
            using DoString = bool (__fastcall*)(const char*, const char*);
            using GetText = const char* (__fastcall*)(char*, std::uint32_t, int);
            bool ok = false;
            const bool dispatched = GameThreadDispatcher::Invoke([&] {
                if (!GameThreadDispatcher::IsGameThread()) return;
                if (!reinterpret_cast<DoString>(LuaDoStringAddress())(script.c_str(),
                    "wow-internal/GenericQuestTurnInExecutor.lua")) return;
                const char* raw = reinterpret_cast<GetText>(GetTextAddress())(
                    const_cast<char*>(variable), 0xFFFFFFFFu, 0);
                if (raw && *raw)
                {
                    char buffer[128]{};
                    std::strncpy(buffer, raw, sizeof(buffer) - 1);
                    output = buffer;
                    ok = true;
                }
            });
            return dispatched && ok;
        }

    private:
        bool IssueInteraction(const Objects::UnitState& npc, std::uint64_t tick)
        {
            if (interactionAttempts_ >= MaximumInteractionAttempts || !InteractUnitGuid(npc.guid))
                return false;
            Debug::Logger::Info("QUEST PLANNER 11C: interacting with turn-in NPC. Quest=" +
                std::to_string(profile_->questId) + " entry=" + std::to_string(npc.entryId) + " GUID=" + Hex64(npc.guid));
            npcGuid_ = npc.guid;
            ++interactionAttempts_;
            lastInteractionTick_ = tick;
            lastDialogTick_ = tick;
            SetState(GenericQuestTurnInState::AdvancingDialog);
            return true;
        }

        static std::string LuaSingleQuoted(const std::string& value)
        {
            std::string result;
            result.reserve(value.size() + 2);
            result.push_back('\'');
            for (char ch : value)
            {
                if (ch == '\\' || ch == '\'')
                    result.push_back('\\');
                result.push_back(ch);
            }
            result.push_back('\'');
            return result;
        }

        bool AdvanceDialog(std::uint64_t tick)
        {
            if (profile_ == nullptr)
                return false;

            const auto doStringAddress = LuaDoStringAddress();
            const auto getTextAddress = GetTextAddress();
            if (!IsExecutable(doStringAddress) || !IsExecutable(getTextAddress))
                return false;

            const std::string expected = LuaSingleQuoted(profile_->title);
            const std::string script =
                "WOW_INTERNAL_GENERIC_TURNIN_STATE='waiting'; "
                "local expected=" + expected + "; "
                "local t=GetTitleText(); "
                "if QuestFrameRewardPanel and QuestFrameRewardPanel:IsVisible() and t==expected then "
                "local c=GetNumQuestChoices(); if c==nil then c=0 end; "
                "if c==0 then WOW_INTERNAL_GENERIC_TURNIN_STATE='reward_claim'; GetQuestReward(0); "
                "else WOW_INTERNAL_GENERIC_TURNIN_STATE='reward_evaluate:'..c; end; "
                "elseif QuestFrameProgressPanel and QuestFrameProgressPanel:IsVisible() and t==expected then "
                "if IsQuestCompletable() then WOW_INTERNAL_GENERIC_TURNIN_STATE='progress_complete'; CompleteQuest(); "
                "else WOW_INTERNAL_GENERIC_TURNIN_STATE='progress_blocked'; end; "
                "elseif QuestFrameGreetingPanel and QuestFrameGreetingPanel:IsVisible() then "
                "local n=GetNumActiveQuests(); local f=0; for i=1,n do "
                "if GetActiveTitle(i)==expected then WOW_INTERNAL_GENERIC_TURNIN_STATE='greeting_select'; SelectActiveQuest(i); f=1; break; end; end; "
                "if f==0 then WOW_INTERNAL_GENERIC_TURNIN_STATE='greeting_not_found'; end; "
                "elseif GossipFrame and GossipFrame:IsVisible() then "
                "local a={GetGossipActiveQuests()}; local q=1; local f=0; "
                "for i=1,table.getn(a),2 do if a[i]==expected then "
                "WOW_INTERNAL_GENERIC_TURNIN_STATE='gossip_select'; SelectGossipActiveQuest(q); f=1; break; end; q=q+1; end; "
                "if f==0 then WOW_INTERNAL_GENERIC_TURNIN_STATE='gossip_not_found'; end; end";

            std::string result;
            if (!RunQuestLua(script, ResultVariable, result))
                return false;

            ++dialogActions_;
            lastDialogTick_ = tick;
            const std::string current = result;

            if (current != lastDialogResult_)
            {
                Debug::Logger::Info("QUEST PLANNER 11C dialog: " + current);
                lastDialogResult_ = current;
            }

            if (current == "greeting_select" ||
                current == "gossip_select" ||
                current == "progress_complete")
            {
                return true;
            }

            if (current == "reward_claim")
            {
                noChoiceRewardClaimIssued_ = true;
                Debug::Logger::Info("QUEST PLANNER 11C: GetQuestReward(0) issued.");
                SetState(GenericQuestTurnInState::WaitingForQuestRemoval);
                return true;
            }

            static constexpr char RewardEvaluatePrefix[] = "reward_evaluate:";
            if (current.rfind(RewardEvaluatePrefix, 0) == 0)
            {
                rewardChoices_ = std::atoi(
                    current.substr(sizeof(RewardEvaluatePrefix) - 1).c_str());

                Debug::Logger::Info(
                    "QUEST PLANNER 11C: class-aware reward choices=" +
                    std::to_string(rewardChoices_));

                if (!rewardController_.Start(tick))
                {
                    Fail("class-aware reward controller failed to start.");
                    return true;
                }

                SetState(GenericQuestTurnInState::EvaluatingReward);
                return true;
            }

            if (current == "progress_blocked")
            {
                Fail("progress panel visible but IsQuestCompletable() returned false.");
                return true;
            }

            return true;
        }

        bool RewardVerified() const
        {
            return noChoiceRewardClaimIssued_ || rewardController_.Verified();
        }

        void TryCompleteAfterRemoval()
        {
            if (!QuestAcquisitionPolicy::VerifiedTurnIn(true,questRemovalObserved_,RewardVerified()))
                return;

            Debug::Logger::Info("QUEST TURNIN VERIFY quest="+std::to_string(profile_->questId)+
                " result=confirmed reason=reward_action_and_live_log_removal");
            if(lastPlayer_.valid) MovementController::HoldPosition(lastPlayer_);
            turnInNavigator_.reset();

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST PLANNER 11C: TURN-IN PASS");
            Debug::Logger::Info(
                "Quest " + std::to_string(profile_->questId) +
                " " + profile_->title + " removed from quest log.");
            if (rewardChoices_ > 0)
            {
                Debug::Logger::Info(
                    "Selected reward: " + rewardController_.SelectedName() +
                    " id=" + std::to_string(rewardController_.SelectedItemId()));
                Debug::Logger::Info(
                    std::string("Auto-equipped: ") +
                    (rewardController_.ShouldEquip() ? "yes" : "no"));
            }
            else
            {
                Debug::Logger::Info("Reward path: GetQuestReward(0), no choice required.");
            }
            Debug::Logger::Info("================================");
            SetState(GenericQuestTurnInState::Done);
        }

    public:
        bool Start(const QuestProfile& profile, std::uint64_t tick)
        {
            if (state_ != GenericQuestTurnInState::Idle ||
                profile.turnInIsGameObject || profile.turnInEntry == 0 || profile.title == nullptr || profile.title[0] == '\0')
            {
                return false;
            }

            if (!IsExecutable(OnRightClickUnitAddress()) ||
                !IsExecutable(LuaDoStringAddress()) ||
                !IsExecutable(GetTextAddress()))
            {
                return false;
            }

            actorProfile_=profile;
            profile_ = &actorProfile_;
            actorInitialized_=false; defenseHeld_=false;
            startTick_ = tick;
            lastMoveTick_ = 0;
            lastInteractionTick_ = 0;
            lastDialogTick_ = 0;
            moveCommands_ = 0;
            interactionAttempts_ = 0;
            dialogActions_ = 0;
            rewardChoices_ = 0;
            npcGuid_ = 0;
            lastDialogResult_.clear();
            questRemovalObserved_ = false;
            noChoiceRewardClaimIssued_ = false;
            turnInSeedReached_ = false;
            turnInSeedReachedTick_ = 0;
            routingToLiveNpc_ = false;
            liveNpcDestination_ = Navigation::NavPoint{};
            turnInNavigator_.reset();
            alternateProbe_.reset();
            alternateProbeOrigin_ = {};
            alternateFailurePosition_ = {};
            alternateFinalDestination_ = {};
            alternateStageDestination_ = {};
            inheritedBadEdge_ = {};
            alternateCandidates_ = {};
            triedAlternateStages_.clear();
            lastRouteEvidence_ = {};
            primaryRouteEvidence_ = {};
            alternateCandidateIndex_ = 0;
            alternateProbeStartTick_ = 0;
            alternateMapId_ = 1;
            strategicLegs_ = 0;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST PLANNER PHASE 11C: GENERIC TURN-IN START");
            Debug::Logger::Info(
                "Quest: " + std::to_string(profile.questId) + " " + profile.title);
            Debug::Logger::Info("Turn-in entry: " + std::to_string(profile.turnInEntry));
            Debug::Logger::Info(
                "Policy: exact active quest title only; CompleteQuest(); then verified reward claim/removal.");
            Debug::Logger::Info("================================");

            SetState(GenericQuestTurnInState::FindingNpc);
            return true;
        }

        void Update(const Objects::WorldState& world, CombatController& combat, std::uint64_t tick)
        {
            if (state_ == GenericQuestTurnInState::Idle ||
                state_ == GenericQuestTurnInState::Done ||
                state_ == GenericQuestTurnInState::Failed ||
                state_ == GenericQuestTurnInState::RewardChoiceRequired)
            {
                return;
            }

            if(!world.player.valid || !world.player.health) return;
            lastPlayer_=world.player;
            if(!actorInitialized_)
            {
                actorInitialized_=true;
                if(!actorProfile_.turnInDestinations.empty())
                    actorProfile_.turnInDestination=QuestAcquisitionPolicy::SelectDestination(actorProfile_.turnInDestination,
                        actorProfile_.turnInDestinations,world.player.x,world.player.y,world.player.z);
                defense_.Reset(world,"quest turn-in");
                Debug::Logger::Info("QUEST TURNIN ACTOR quest="+std::to_string(profile_->questId)+
                    " type=npc entry="+std::to_string(profile_->turnInEntry)+" result=source_actor_selected");
            }
            const auto defense=defense_.Update(world,combat,turnInNavigator_.get(),0,tick);
            if(defense!=ObjectiveDefenseUpdate::Clear)
            {
                if(!defenseHeld_ && !turnInNavigator_) MovementController::HoldPosition(world.player);
                defenseHeld_=true;
                if(defense==ObjectiveDefenseUpdate::Failed) Fail("defensive recovery interrupted turn-in");
                return;
            }
            defenseHeld_=false;

            if (tick >= startTick_ + OverallTimeoutTicks)
            {
                Fail("turn-in flow timed out.");
                return;
            }

            if (state_ == GenericQuestTurnInState::EvaluatingReward)
            {
                rewardController_.Update(tick);

                if (rewardController_.UnsupportedClass())
                {
                    Debug::Logger::Info(
                        "QUEST PLANNER 11C: reward choices require manual selection for this class.");
                    SetState(GenericQuestTurnInState::RewardChoiceRequired);
                    return;
                }

                if (rewardController_.Failed())
                {
                    Fail("class-aware reward evaluation failed.");
                    return;
                }

                if (rewardController_.Verified())
                {
                    SetState(GenericQuestTurnInState::WaitingForQuestRemoval);
                    TryCompleteAfterRemoval();
                }
                return;
            }

            if (state_ == GenericQuestTurnInState::WaitingForQuestRemoval)
            {
                TryCompleteAfterRemoval();
                return;
            }

            if (state_ == GenericQuestTurnInState::SelectingAlternateApproach)
            {
                AdvanceAlternateSelection(world, tick);
                return;
            }
            if (state_ == GenericQuestTurnInState::RoutingAlternateApproach)
            {
                UpdateAlternateApproach(world, tick);
                return;
            }

            const auto* npc = FindTurnInNpc(world);
            if (npc == nullptr)
            {
                if (turnInNavigator_ != nullptr)
                {
                    turnInNavigator_->Update(world.player, tick);

                    if (turnInNavigator_->Failed())
                    {
                        if (!BeginAlternateAfterRouteFailure(
                                world, tick, "npc_search"))
                            Fail("NavMesh routing to turn-in NPC search seed failed; bounded alternate recovery unavailable.");
                        return;
                    }

                    if (turnInNavigator_->Arrived())
                    {
                        turnInNavigator_.reset();

                        if (routingToLiveNpc_)
                        {
                            Debug::Logger::Info(
                                "QUEST PLANNER 11C.3: last live NPC position reached while NPC is not visible; returning to bounded NPC search.");
                            routingToLiveNpc_ = false;
                            turnInSeedReached_ = true;
                            turnInSeedReachedTick_ = tick;
                            SetState(GenericQuestTurnInState::FindingNpc);
                        }
                        else
                        {
                            Debug::Logger::Info(
                                "QUEST PLANNER 11C.2: TURN-IN SEARCH SEED REACHED; waiting for live NPC.");
                            turnInSeedReached_ = true;
                            turnInSeedReachedTick_ = tick;
                            SetState(GenericQuestTurnInState::FindingNpc);
                        }
                    }
                    return;
                }

                if (turnInSeedReached_)
                {
                    if (tick >= turnInSeedReachedTick_ + SeedArrivalGraceTicks)
                        Fail("turn-in NPC was still not visible after reaching its search seed.");
                    return;
                }

                const bool hasQuestDbTurnInSeed =
                    profile_ != nullptr && profile_->turnInDestination.valid;
                const bool hasLegacyTurnInSeed =
                    ValleyQuestNpcDestinations::Find(profile_->turnInEntry) != nullptr;

                if (hasQuestDbTurnInSeed || hasLegacyTurnInSeed)
                {
                    Debug::Logger::Info(
                        std::string("QUESTDB 12B.7: TURN-IN SEARCH SEED AVAILABLE source=") +
                        (hasQuestDbTurnInSeed ? "QuestDB profile" : "legacy NPC table") +
                        " questId=" + std::to_string(profile_->questId) +
                        " turnInEntry=" + std::to_string(profile_->turnInEntry));

                    if (!StartTurnInSeedNavigation(world, tick))
                        Fail("failed to start NavMesh routing to turn-in NPC search seed.");
                    return;
                }

                if (tick >= startTick_ + NpcSearchTimeoutTicks)
                    Fail("turn-in NPC was not found in WorldState and neither QuestDB nor legacy profile supplied a search seed.");
                return;
            }

            turnInSeedReached_ = false;
            npcGuid_ = npc->guid;

            if (npc->distance > InteractionDistance)
            {
                if (turnInNavigator_ != nullptr)
                {
                    if (!routingToLiveNpc_)
                    {
                        Debug::Logger::Info(
                            "QUEST PLANNER 11C.2: LIVE TURN-IN NPC ACQUIRED DURING NAVMESH SEARCH; switching to live XYZ.");

                        if (!StartLiveNpcNavigation(
                                world,
                                *npc,
                                tick,
                                "NPC entered WorldState during search-seed route"))
                        {
                            if (npc->distance <= DirectApproachFallbackDistance &&
                                (lastMoveTick_ == 0 || tick >= lastMoveTick_ + MoveCooldownTicks))
                            {
                                Debug::Logger::Info(
                                    "QUEST PLANNER 11C.3: live-NPC NavMesh start failed; using bounded close-range direct CTM fallback.");
                                if (!IssueApproach(world.player, *npc, tick))
                                    Fail("failed to approach turn-in NPC after live NavMesh handoff failure.");
                            }
                            else
                            {
                                Fail("failed to start NavMesh handoff to live turn-in NPC.");
                            }
                        }
                        return;
                    }

                    const float liveDestinationDrift = Distance2D(
                        liveNpcDestination_.x,
                        liveNpcDestination_.y,
                        npc->x,
                        npc->y);

                    if (liveDestinationDrift >= LiveNpcReplanDistance)
                    {
                        if (!StartLiveNpcNavigation(
                                world,
                                *npc,
                                tick,
                                "turn-in NPC moved away from the current live NavMesh destination"))
                        {
                            Fail("failed to refresh NavMesh route to moving turn-in NPC.");
                        }
                        return;
                    }

                    turnInNavigator_->Update(world.player, tick);

                    if (turnInNavigator_->Failed())
                    {
                        if (npc->distance <= DirectApproachFallbackDistance &&
                            (lastMoveTick_ == 0 || tick >= lastMoveTick_ + MoveCooldownTicks))
                        {
                            Debug::Logger::Info(
                                "QUEST PLANNER 11C.3: live-NPC NavMesh failed close to target; using bounded direct CTM fallback.");
                            turnInNavigator_.reset();
                            routingToLiveNpc_ = false;
                            if (!IssueApproach(world.player, *npc, tick))
                                Fail("failed close-range fallback approach to turn-in NPC.");
                        }
                        else
                        {
                            if (!BeginAlternateAfterRouteFailure(
                                    world, tick, "live_npc"))
                                Fail("NavMesh routing to live turn-in NPC failed; bounded alternate recovery unavailable.");
                        }
                        return;
                    }

                    if (turnInNavigator_->Arrived())
                    {
                        Debug::Logger::Info(
                            "QUEST PLANNER 11C.3: LIVE TURN-IN NPC NAVMESH ARRIVAL; interaction handoff.");
                        turnInNavigator_.reset();
                        routingToLiveNpc_ = false;
                        SetState(GenericQuestTurnInState::ApproachingNpc);

                        if (npc->distance > InteractionDistance)
                        {
                            if (!StartLiveNpcNavigation(
                                    world,
                                    *npc,
                                    tick,
                                    "NavMesh arrival was outside live interaction distance; refining route"))
                            {
                                if (npc->distance <= DirectApproachFallbackDistance &&
                                    (lastMoveTick_ == 0 || tick >= lastMoveTick_ + MoveCooldownTicks))
                                {
                                    if (!IssueApproach(world.player, *npc, tick))
                                        Fail("failed final close-range approach to turn-in NPC.");
                                }
                                else
                                {
                                    Fail("turn-in NavMesh arrived outside interaction range and refinement failed.");
                                }
                            }
                        }
                        return;
                    }

                    return;
                }

                if (!StartLiveNpcNavigation(
                        world,
                        *npc,
                        tick,
                        "visible turn-in NPC is outside interaction distance"))
                {
                    if (npc->distance <= DirectApproachFallbackDistance &&
                        (lastMoveTick_ == 0 || tick >= lastMoveTick_ + MoveCooldownTicks))
                    {
                        if (!IssueApproach(world.player, *npc, tick))
                            Fail("failed bounded close-range approach to turn-in NPC.");
                    }
                    else
                    {
                        Fail("failed to start NavMesh routing to visible turn-in NPC.");
                    }
                }
                return;
            }

            if (turnInNavigator_ != nullptr)
            {
                Debug::Logger::Info(
                    "QUEST PLANNER 11C.3: live turn-in NPC is within interaction distance; stopping NavMesh handoff.");
                turnInNavigator_.reset();
                routingToLiveNpc_ = false;
                SetState(GenericQuestTurnInState::ApproachingNpc);
            }

            if (state_ == GenericQuestTurnInState::RoutingToNpc)
                SetState(GenericQuestTurnInState::ApproachingNpc);

            if (state_ == GenericQuestTurnInState::FindingNpc ||
                state_ == GenericQuestTurnInState::ApproachingNpc)
            {
                if (interactionAttempts_ == 0 ||
                    tick >= lastInteractionTick_ + InteractionRetryTicks)
                {
                    if (!IssueInteraction(*npc, tick))
                        Fail("failed to interact with turn-in NPC.");
                }
                return;
            }

            if (state_ == GenericQuestTurnInState::AdvancingDialog)
            {
                if (lastDialogResult_ == "waiting" &&
                    tick >= lastInteractionTick_ + InteractionRetryTicks)
                {
                    if (!IssueInteraction(*npc, tick))
                        Fail("failed to retry turn-in NPC interaction.");
                    return;
                }

                if (tick < lastDialogTick_ + DialogStepTicks)
                    return;

                if (!AdvanceDialog(tick))
                    Fail("failed to advance generic quest turn-in dialog.");
            }
        }

        void MarkQuestRemoved()
        {
            questRemovalObserved_ = true;
            TryCompleteAfterRemoval();
        }

        bool OwnsControl() const
        {
            return state_ != GenericQuestTurnInState::Idle &&
                   state_ != GenericQuestTurnInState::Done &&
                   state_ != GenericQuestTurnInState::Failed;
        }

        bool Done() const { return state_ == GenericQuestTurnInState::Done; }
        bool Failed() const { return state_ == GenericQuestTurnInState::Failed; }
        bool RewardChoiceRequired() const
        {
            return state_ == GenericQuestTurnInState::RewardChoiceRequired;
        }
        const char* StateName() const { return StateNameInternal(state_); }
    };
}
