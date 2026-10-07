#include "../src/Bot/DeathRecoveryEvidencePolicy.h"
#include "../src/Bot/DeathRecoveryAnchorStore.h"
#include "../src/Navigation/NavigationInitTelemetryPolicy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

int main()
{
    using P = Bot::DeathRecoveryEvidencePolicy;
    assert(P::AnchorWait(0, false) == P::Wait::Pending);
    assert(P::AnchorWait(P::AnchorWaitMs - 1, false) == P::Wait::Pending);
    assert(P::AnchorWait(P::AnchorWaitMs, false) == P::Wait::Expired);
    assert(P::AnchorWait(P::AnchorWaitMs, true) == P::Wait::Ready);
    assert(P::ServerAnchorEligible(true, true, 42, 42, 1, 1, 1, {1,2,3}));
    assert(!P::ServerAnchorEligible(false, true, 42, 42, 1, 1, 1, {1,2,3}));
    assert(!P::ServerAnchorEligible(true, false, 42, 42, 1, 1, 1, {1,2,3}));
    assert(!P::ServerAnchorEligible(true, true, 42, 43, 1, 1, 1, {1,2,3}));
    assert(!P::ServerAnchorEligible(true, true, 0, 0, 1, 1, 1, {1,2,3}));
    assert(!P::ServerAnchorEligible(true, true, 42, 42, 1, -1, -1, {0,0,0}));
    assert(!P::ServerAnchorEligible(true, true, 42, 42, 1, 1, 2, {1,2,3}));
    assert(!P::ServerAnchorEligible(true, true, 42, 42, 1, 1, 1,
        {std::numeric_limits<float>::quiet_NaN(),2,3}));
    // Loading is bounded by the unchanged episode/no-progress deadlines,
    // not by destructive precision-reroute churn at 160 ticks.
    assert(!P::MayRestartForStall(true));
    assert(P::MayRestartForStall(false));
    assert(!P::CanReclaim(false, true, true, 0, 7));
    assert(!P::CanReclaim(true, false, true, 0, 7));
    assert(!P::CanReclaim(true, true, true, 1, 7));
    assert(P::CanReclaim(true, true, true, 0, 7));
    assert(!P::CanReclaim(true, true, true, 0, 33));
    assert(!P::CanReclaim(true, true, true, -1, 7));
    using Death = Bot::DeathRecoveryPolicy;
    assert(Death::EntryFor(false,true,false) == Death::EntryState::AwaitingCorpseAnchor);
    assert(Death::EntryFor(true,false,false) == Death::EntryState::ReleasingSpirit);
    assert(!Death::AliveAfterCorpseRun(true,false,10,100,false,true,2));
    assert(!Death::AliveAfterCorpseRun(true,false,10,100,true,false,2));
    assert(!Death::AliveAfterCorpseRun(true,false,10,100,true,true,1));
    assert(Death::AliveAfterCorpseRun(true,false,10,100,true,true,2));
    using Anchor = Bot::DeathRecoveryAnchorPolicy;
    assert(Anchor::Eligible(42,1,42,1,10,true,{1,2,3}));
    assert(!Anchor::Eligible(42,1,43,1,10,true,{1,2,3}));
    assert(!Anchor::Eligible(42,1,42,2,10,true,{1,2,3}));
    assert(!Anchor::Eligible(42,1,42,1,Anchor::MaximumAgeSeconds+1,true,{1,2,3}));
    assert(!Anchor::Eligible(42,1,42,1,10,false,{1,2,3}));
    using Live = Bot::DeathRecoveryLivenessPolicy;
    static_assert(Live::MaximumEpisodeAgeMs == 300000);
    static_assert(Live::MaximumNoProgressAgeMs == 180000);
    static_assert(Live::MaximumRouteAttempts == 18);
    static_assert(Live::MaximumFullMapFallbackAttempts == 1);
    assert(Live::Evaluate(179999,179999,1,0,false) == Live::FailureReason::None);
    assert(Live::Evaluate(180000,180000,1,0,false) == Live::FailureReason::NoPhysicalProgress);
    using Nav = Navigation::NavigationInitTelemetryPolicy;
    using Tier = Navigation::NavigationInitTier;
    assert(Nav::RetainIntentForFallback(Tier::Route,true));
    assert(Nav::RetainIntentForFallback(Tier::Expanded,true));
    assert(!Nav::RetainIntentForFallback(Tier::FullMap,true));

    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto read=[](const auto& path) {
        std::ifstream file(path); assert(file);
        return std::string(std::istreambuf_iterator<char>(file),{});
    };
    const auto controller=read(root/"src/Bot/DeathRecoveryController.h");
    assert(controller.find("DeathRecoveryEvidencePolicy::MayRestartForStall(") != std::string::npos);
    assert(controller.find("corpseNavigator_ && corpseNavigator_->InitializationPending()") != std::string::npos);
    assert(controller.find("currentServerAnchor_, freshProbe") != std::string::npos);
    assert(controller.find("corpse_anchor_pending_timeout") != std::string::npos);
    assert(controller.find("totalRetrieveAttempts_ >= DeathRecoveryPolicy::MaximumRetrieveAttemptsPerCycle") != std::string::npos);
    assert(controller.find("releaseAttempts_ = 0;",controller.find("release retry cycle")) == std::string::npos);
    const auto reader=read(root/"src/Bot/CorpseLocation5875.h");
    assert(reader.find("Core::Memory::Write") == std::string::npos);
    assert(reader.find("ClickToMove") == std::string::npos);
    const auto monitor=read(root/"src/Bot/WorldMonitor.h");
    assert(monitor.find("!deathRecoveryOwnedTick && TemporaryGrindModeEnabled") != std::string::npos);
    assert(monitor.find("!deathRecoveryOwnedTick &&\n                    questPlannerRuntime") != std::string::npos);
    assert(monitor.find("case DeathRecoveryState::RoutingToCorpse: afkDeathGap=AfkDeathGap::RoutingToCorpse") != std::string::npos);
}
