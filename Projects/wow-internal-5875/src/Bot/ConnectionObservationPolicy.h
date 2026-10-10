#pragma once

#include "ConnectionEvidence5875.h"
#include "LocationCandidates5875.h"

#include <sstream>
#include <string>

namespace Bot
{
    enum class ConnectionMode { Normal, Observe, Blocked };

    inline ConnectionMode ParseConnectionMode(const char* value)
    {
        if (!value) return ConnectionMode::Normal;
        // A configured but unsupported (including empty) value cannot fall
        // through to gameplay. Do not echo arbitrary environment contents.
        return std::string_view(value) == "observe"
            ? ConnectionMode::Observe : ConnectionMode::Blocked;
    }

    struct ConnectionWorldObservation
    {
        bool snapshotValid = false;
        const char* stage = "unknown";
        std::uint32_t manager = 0;
        std::uint64_t playerGuid = 0;
        std::uint32_t localPlayer = 0;

        // WorldStateReader preserves successful partial reads on failure.
        // Explicit stage allowlists fail closed if a new stage is introduced.
        bool GuidRead() const
        {
            const std::string_view s(stage);
            return snapshotValid || s == "active_guid_missing" ||
                s == "first_object_unreadable" || s == "first_object_missing" ||
                s == "local_player_missing" || s == "player_snapshot_unreadable";
        }
        bool ManagerRead() const
        {
            const std::string_view s(stage);
            return GuidRead() || s == "manager_missing" || s == "manager_unreadable" ||
                s == "active_guid_unreadable";
        }
        bool LocalPlayerObserved() const
        {
            return snapshotValid || std::string_view(stage) == "player_snapshot_unreadable";
        }
    };

    // No combined world/connection state machine or action eligibility exists.
    // Compare only lifecycle evidence; clocks, coordinates, HP and unit counts
    // must not turn a stable world into a log stream.
    class ConnectionObservationTracker
    {
        std::string previous_;

        static std::string Hex(std::uint64_t value)
        {
            std::ostringstream out;
            out << "0x" << std::hex << value;
            return out.str();
        }

    public:
        static std::string Fields(const ConnectionEvidence5875& connection,
                                  const ConnectionWorldObservation& world,
                                  const LocationCandidates5875& location = {})
        {
            const bool known = connection.signaturesKnown && connection.serverConnectionKnown;
            const auto candidate = [&](const std::optional<std::uint32_t>& value)
            {
                return location.signaturesKnown && value ? std::to_string(*value) : "unknown";
            };
            return std::string("worldSnapshot=") + (world.snapshotValid ? "valid" : "unavailable") +
                " worldStage=" + world.stage +
                " manager=" + (world.ManagerRead() ? Hex(world.manager) : "unknown") +
                " playerGuid=" + (world.GuidRead() ? Hex(world.playerGuid) : "unknown") +
                " localPlayer=" + (world.LocalPlayerObserved() ? Hex(world.localPlayer) : "unknown") +
                " sourceVerified=" + (connection.signaturesKnown ? "yes" : "no") +
                " serverConnected=" + (known ? (connection.serverConnected ? "yes" : "no") : "unknown") +
                " lastGlueScreen=" + (connection.signaturesKnown ? connection.lastGlueScreen : "unknown") +
                " glueScreenSemantics=historical glueVisibility=unknown dialogState=unknown"
                " dialogVisible=unknown dialogType=unknown"
                " loading=unknown currentGlueScreen=unknown pendingGlueScreen=unknown"
                " glueGeneration=unknown disconnectConfirmed=unknown actionEligibility=unknown"
                " liveGlueReason=source_gap_identity_and_lifetime"
                " processAlive=yes decision=observe_only inputOwner=none commands=none"
                " sourceReason=" + connection.reason +
                " sourceVerifiedScope=connection_instructions"
                " locationSignatures=" + (location.signaturesKnown ? "pass" : "unknown") +
                " locationOwner=" + (location.signaturesKnown && location.owner ? Hex(*location.owner) : "unknown") +
                " mapCandidate=" + candidate(location.mapCandidate) +
                " zoneCandidate=" + candidate(location.zoneCandidate) +
                " areaCandidate=" + candidate(location.areaCandidate) +
                " locationQualification=unqualified locationReason=source_lifetime_gap"
                " worldCorrelation=sequential";
        }

        // First sample and evidence changes only. No retained successful
        // evidence replaces a failed read. Returning to a prior sample emits.
        bool Observe(const ConnectionEvidence5875& connection,
                     const ConnectionWorldObservation& world, std::string& fields,
                     const LocationCandidates5875& location = {})
        {
            fields = Fields(connection, world, location);
            if (fields == previous_) return false;
            previous_ = fields;
            return true;
        }
    };
}
