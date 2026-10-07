#pragma once
#include "AfkProtectionPolicy.h"

namespace Bot
{
    enum class AfkAgreementState { BothClear, BothActive, ClientOnlyActive, ServerOnlyActive, Unknown };

    // Classification only. Neither mixed state authorizes a native clear:
    // 5EB830 sends a server toggle when client-active, or returns when clear.
    struct AfkAgreementPolicy
    {
        static AfkAgreementState Classify(const AfkObservation& o)
        {
            if (!AfkProtectionPolicy::Valid(o)) return AfkAgreementState::Unknown;
            if (o.clientAfk) return o.serverAfk ? AfkAgreementState::BothActive : AfkAgreementState::ClientOnlyActive;
            return o.serverAfk ? AfkAgreementState::ServerOnlyActive : AfkAgreementState::BothClear;
        }
        static bool Mixed(AfkAgreementState state)
        {
            return state==AfkAgreementState::ClientOnlyActive || state==AfkAgreementState::ServerOnlyActive;
        }
        static const char* Name(AfkAgreementState state)
        {
            switch (state)
            {
            case AfkAgreementState::BothClear: return "both_clear";
            case AfkAgreementState::BothActive: return "both_active";
            case AfkAgreementState::ClientOnlyActive: return "client_only";
            case AfkAgreementState::ServerOnlyActive: return "server_only";
            case AfkAgreementState::Unknown: return "unknown";
            }
            return "unknown";
        }
        static const char* UnsupportedReason(AfkAgreementState state)
        {
            if (state==AfkAgreementState::ClientOnlyActive) return "client_only_afk_no_safe_reconciliation";
            if (state==AfkAgreementState::ServerOnlyActive) return "server_only_afk_no_safe_reconciliation";
            return "afk_reconciliation_observation_unknown";
        }
        static const char* ObservationReason(AfkAgreementState state)
        {
            return state==AfkAgreementState::ClientOnlyActive ? "observing_client_only_afk_no_safe_toggle" :
                "observing_server_only_afk_no_safe_toggle";
        }
    };
}
