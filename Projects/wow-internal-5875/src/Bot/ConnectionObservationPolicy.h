#pragma once

#include "ConnectionEvidence5875.h"

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
                                  const ConnectionWorldObservation& world)
        {
            const bool known = connection.signaturesKnown && connection.serverConnectionKnown;
            return std::string("worldSnapshot=") + (world.snapshotValid ? "valid" : "unavailable") +
                " worldStage=" + world.stage +
                " manager=" + (world.snapshotValid ? Hex(world.manager) : "unknown") +
                " playerGuid=" + (world.snapshotValid ? Hex(world.playerGuid) : "unknown") +
                " localPlayer=" + (world.snapshotValid ? Hex(world.localPlayer) : "unknown") +
                " sourceVerified=" + (connection.signaturesKnown ? "yes" : "no") +
                " serverConnected=" + (known ? (connection.serverConnected ? "yes" : "no") : "unknown") +
                " lastGlueScreen=" + (connection.signaturesKnown ? connection.lastGlueScreen : "unknown") +
                " glueScreenSemantics=historical glueVisibility=unknown dialogState=unknown"
                " loading=unknown currentGlueScreen=unknown pendingGlueScreen=unknown"
                " glueGeneration=unknown disconnectConfirmed=unknown actionEligibility=unknown"
                " processAlive=yes decision=observe_only inputOwner=none commands=none"
                " sourceReason=" + connection.reason;
        }

        // First sample and evidence changes only. No retained successful
        // evidence replaces a failed read. Returning to a prior sample emits.
        bool Observe(const ConnectionEvidence5875& connection,
                     const ConnectionWorldObservation& world, std::string& fields)
        {
            fields = Fields(connection, world);
            if (fields == previous_) return false;
            previous_ = fields;
            return true;
        }
    };
}
