#pragma once

#include "EpisodeBadTransitionPolicy.h"
#include "LocalPortalSteeringPolicy.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace Navigation
{
    enum class NavCommandSource
    {
        OrdinarySteering,
        FallbackNearer,
        PortalStage,
        SurfaceRecovery,
        WallRecovery,
        VerticalRecovery,
        EscapeProbe,
        Backtrack,
        FinalDirect
    };

    struct NavCommandPoint { float x=0, y=0, z=0; };

    struct IssuedSteeringCommand
    {
        bool active=false;
        std::uint64_t corridorFingerprint=0;
        std::uint64_t routeGeneration=0;
        std::uint64_t intentId=0;
        std::uint64_t commandSequence=0;
        std::uint64_t dispatchSerial=0;
        NavCommandSource source=NavCommandSource::OrdinarySteering;
        NavCommandPoint target{};
        NavCommandPoint issuedAt{};
        DirectedPolyTransition transition{};
    };

    enum class HardStallAttributionReason
    {
        IssuedTransitionProven,
        IssuedTransitionUnknown,
        CommandProvenanceStale,
        CommandReplaced,
        MovementNotExpected
    };

    struct HardStallAttribution
    {
        DirectedPolyTransition transition{};
        HardStallAttributionReason reason=
            HardStallAttributionReason::CommandProvenanceStale;
    };

    struct IssuedSteeringCommandPolicy
    {
        static DirectedPolyTransition ProvenRouteEdge(
            const std::vector<std::uint64_t>& corridor,
            std::uint64_t playerPoly, std::uint64_t enteredPoly,
            bool portalKnown, bool adjusted,
            std::uint64_t adjustedTargetPoly)
        {
            const auto edge=LocalPortalSteeringPolicy::CandidateEdge(
                corridor,playerPoly,enteredPoly);
            if (!portalKnown || !edge.Valid() ||
                (adjusted && adjustedTargetPoly!=edge.from &&
                    adjustedTargetPoly!=edge.to))
                return {};
            return edge;
        }

        static constexpr const char* SourceName(NavCommandSource source)
        {
            switch(source)
            {
                case NavCommandSource::OrdinarySteering: return "ordinary_steering";
                case NavCommandSource::FallbackNearer: return "fallback_nearer";
                case NavCommandSource::PortalStage: return "portal_stage";
                case NavCommandSource::SurfaceRecovery: return "surface_recovery";
                case NavCommandSource::WallRecovery: return "wall_recovery";
                case NavCommandSource::VerticalRecovery: return "vertical_recovery";
                case NavCommandSource::EscapeProbe: return "escape_probe";
                case NavCommandSource::Backtrack: return "backtrack";
                case NavCommandSource::FinalDirect: return "final_direct";
            }
            return "unknown";
        }

        static constexpr const char* ReasonName(HardStallAttributionReason reason)
        {
            switch(reason)
            {
                case HardStallAttributionReason::IssuedTransitionProven:
                    return "issued_transition_proven";
                case HardStallAttributionReason::IssuedTransitionUnknown:
                    return "issued_transition_unknown";
                case HardStallAttributionReason::CommandProvenanceStale:
                    return "command_provenance_stale";
                case HardStallAttributionReason::CommandReplaced:
                    return "command_replaced";
                case HardStallAttributionReason::MovementNotExpected:
                    return "movement_not_expected";
            }
            return "command_provenance_stale";
        }

        static void Invalidate(IssuedSteeringCommand& command)
        {
            command={};
        }

        static void Record(IssuedSteeringCommand& command,
            std::uint64_t fingerprint, std::uint64_t generation,
            std::uint64_t intent, std::uint64_t sequence,
            std::uint64_t dispatchSerial, NavCommandSource source,
            NavCommandPoint target, NavCommandPoint issuedAt,
            DirectedPolyTransition transition)
        {
            command={true,fingerprint,generation,intent,sequence,
                dispatchSerial,source,target,issuedAt,
                (source==NavCommandSource::OrdinarySteering ||
                 source==NavCommandSource::FallbackNearer)
                    ? transition : DirectedPolyTransition{}};
        }

        static HardStallAttribution ForHardStall(
            const IssuedSteeringCommand& command,
            std::uint64_t currentDispatchSerial,
            std::uint64_t fingerprint, std::uint64_t generation,
            std::uint64_t intent, NavCommandPoint currentDispatchTarget,
            bool movementExpected)
        {
            if (!movementExpected)
                return {{},HardStallAttributionReason::MovementNotExpected};
            if (!command.active || !command.commandSequence ||
                command.corridorFingerprint!=fingerprint ||
                command.routeGeneration!=generation || command.intentId!=intent)
                return {{},HardStallAttributionReason::CommandProvenanceStale};
            if (!std::isfinite(command.target.x) ||
                !std::isfinite(command.target.y) ||
                !std::isfinite(command.target.z) ||
                !std::isfinite(currentDispatchTarget.x) ||
                !std::isfinite(currentDispatchTarget.y) ||
                !std::isfinite(currentDispatchTarget.z) ||
                command.dispatchSerial!=currentDispatchSerial ||
                std::fabs(command.target.x-currentDispatchTarget.x)>0.01f ||
                std::fabs(command.target.y-currentDispatchTarget.y)>0.01f ||
                std::fabs(command.target.z-currentDispatchTarget.z)>0.01f)
                return {{},HardStallAttributionReason::CommandReplaced};
            if (!command.transition.Valid())
                return {{},HardStallAttributionReason::IssuedTransitionUnknown};
            return {command.transition,
                HardStallAttributionReason::IssuedTransitionProven};
        }
    };
}
