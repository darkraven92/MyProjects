#include "../src/Bot/BotDeathOwnershipPolicy.h"
#include "../src/Bot/DeathRecoveryTerminalPolicy.h"

#include <cassert>
#include <string>

int main()
{
    using Terminal = Bot::DeathRecoveryTerminalPolicy;
    using Liveness = Bot::DeathRecoveryLivenessPolicy;
    using Point = Terminal::Point;

    const auto signature = Terminal::MakeSignature(
        1, Point{-900.0f, -2000.0f, 90.0f},
        Point{-550.0f, -2500.0f, 95.0f},
        static_cast<int>(Liveness::FailureReason::EpisodeDeadline),
        4, 0xA, 0xB, 123);
    assert(Terminal::SignatureLabel(false, signature) == "none");
    assert(Terminal::SignatureLabel(true, signature) ==
        std::to_string(Terminal::Fingerprint(signature)));
    assert(Terminal::Fingerprint(signature) == Terminal::Fingerprint(
        Terminal::MakeSignature(1, {-900.0f, -2000.0f, 90.0f},
            {-550.0f, -2500.0f, 95.0f},
            static_cast<int>(Liveness::FailureReason::EpisodeDeadline),
            4, 0xA, 0xB, 123)));
    assert(Terminal::Fingerprint(signature) != Terminal::Fingerprint(
        Terminal::MakeSignature(1, {-880.0f, -2000.0f, 90.0f},
            {-550.0f, -2500.0f, 95.0f},
            static_cast<int>(Liveness::FailureReason::EpisodeDeadline),
            4, 0xA, 0xB, 123)));
    assert(Terminal::Fingerprint(signature) != Terminal::Fingerprint(
        Terminal::MakeSignature(1, {-900.0f, -2000.0f, 90.0f},
            {-520.0f, -2500.0f, 95.0f},
            static_cast<int>(Liveness::FailureReason::EpisodeDeadline),
            4, 0xA, 0xB, 123)));
    assert(Terminal::Fingerprint(signature) != Terminal::Fingerprint(
        Terminal::MakeSignature(1, {-900.0f, -2000.0f, 90.0f},
            {-550.0f, -2500.0f, 95.0f},
            static_cast<int>(Liveness::FailureReason::NoPhysicalProgress),
            4, 0xA, 0xB, 123)));
    assert(Terminal::Fingerprint(signature) != Terminal::Fingerprint(
        Terminal::MakeSignature(1, {-900.0f, -2000.0f, 90.0f},
            {-550.0f, -2500.0f, 95.0f},
            static_cast<int>(Liveness::FailureReason::EpisodeDeadline),
            4, 0xB, 0xA, 123)));

    // Failed remains death-owned even with a high-HP snapshot until the
    // controller completes its positive, same-character Lua confirmation.
    assert(Bot::BotDeathOwnershipPolicy::ShouldOwn(
        false, true, true, 179, 179, false));
    const auto manualAlive = [](bool worldValid, std::uint64_t episodeGuid,
        std::uint64_t currentGuid, std::uint32_t localAddress,
        std::uint32_t snapshotAddress, std::uint32_t hp,
        std::uint32_t maxHp, bool probeValid, bool deadKnown,
        bool dead, bool ghost, int streak)
    {
        return Liveness::ManualAliveConfirmedForEpisode(
            worldValid, episodeGuid, currentGuid, localAddress,
            snapshotAddress, true, hp, maxHp, probeValid, deadKnown,
            dead, ghost, streak);
    };
    assert(!manualAlive(true, 42, 42, 0x1000, 0x1000,
        1, 438, true, true, false, false, 2));
    assert(!manualAlive(true, 42, 42, 0x1000, 0x1000,
        179, 179, false, true, false, false, 2));
    assert(!manualAlive(true, 42, 42, 0x1000, 0x1000,
        179, 179, true, false, false, false, 2));
    assert(!manualAlive(true, 42, 42, 0x1000, 0x1000,
        179, 179, true, true, true, false, 2));
    assert(!manualAlive(true, 42, 42, 0x1000, 0x1000,
        179, 179, true, true, false, true, 2));
    assert(!manualAlive(true, 42, 42, 0x1000, 0x1000,
        179, 179, true, true, false, false, 1));
    int probesBeforeWorldGap = 1;
    Liveness::InvalidateManualAliveEvidence(probesBeforeWorldGap);
    assert(probesBeforeWorldGap == 0);
    assert(!manualAlive(true, 42, 42, 0x1000, 0x1000,
        179, 179, true, true, false, false,
        probesBeforeWorldGap + 1));
    assert(!manualAlive(false, 42, 42, 0x1000, 0x1000,
        179, 179, true, true, false, false, 2));
    assert(!manualAlive(true, 42, 43, 0x1000, 0x1000,
        179, 179, true, true, false, false, 2));
    assert(!manualAlive(true, 42, 42, 0x1000, 0x2000,
        179, 179, true, true, false, false, 2));
    assert(!manualAlive(true, 0, 42, 0x1000, 0x1000,
        179, 179, true, true, false, false, 2));

    // A changed max HP and a relocated player object do not invalidate a
    // positive same-GUID resurrection; no old pointer is reused.
    assert(manualAlive(true, 42, 42, 0x2000, 0x2000,
        179, 179, true, true, false, false, 2));
    assert(!manualAlive(true, 42, 42, 0x2000, 0x2000,
        179, 0, true, true, false, false, 2));

    // Normal post-RetrieveCorpse completion retains its separate command
    // requirement and two fresh alive probes.
    assert(!Bot::DeathRecoveryPolicy::AliveAfterCorpseRun(
        true, false, 179, 179, true, false, 2));
    assert(!Bot::DeathRecoveryPolicy::AliveAfterCorpseRun(
        true, false, 179, 179, true, true, 1));
    assert(Bot::DeathRecoveryPolicy::AliveAfterCorpseRun(
        true, false, 179, 179, true, true, 2));
    static_assert(Liveness::MaximumEpisodeAgeMs == 300000);
    static_assert(Liveness::MaximumRouteAttempts == 18);
    static_assert(Bot::DeathRecoveryPolicy::AliveConfirmationProbes == 2);
}
