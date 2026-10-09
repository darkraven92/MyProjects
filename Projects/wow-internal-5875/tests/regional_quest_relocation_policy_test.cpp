#include "../src/Bot/RegionalQuestRelocationPolicy.h"
#include <cassert>
#include <fstream>
#include <iterator>
int main()
{
    using namespace Bot;
    QuestProfile p; p.questId=10001; p.title="Fixture"; p.minimumLevel=1;
    p.giverEntry=17; p.turnInEntry=18;
    p.objective={QuestObjectiveType::KillMob,19,0,0,1,"fixture"};
    p.destination={true,1,400,0,0,5,"fixture"}; p.giverDestination=p.destination;
    p.sourceMetadata=QuestSourceMetadata{};
    std::vector<QuestProfile> profiles{p};
    QuestPlannerSnapshot snapshot; snapshot.valid=true;
    QuestEligibilityContext context; context.level=1; context.raceMask=2; context.classMask=1;
    std::map<std::uint32_t,std::uint64_t> backoff;
    const auto candidates=[&](std::uint64_t tick=1) {
        QuestGraph graph(profiles);
        return RegionalQuestRelocationPolicy::Candidates(profiles,graph,context,snapshot,1,0,0,0,tick,backoff);
    };
    assert(candidates().size()==1 && candidates()[0].destination.x==400);
    profiles[0].giverDestination.x=200; assert(candidates().empty());
    profiles[0].giverDestination.x=2001; assert(candidates().empty());
    profiles[0].giverDestination.x=400;
    profiles[0].giverDestination.mapId=0; assert(candidates().empty());
    profiles[0].giverDestination.mapId=1;
    profiles[0].minimumLevel=2; assert(candidates().empty()); profiles[0].minimumLevel=1;
    profiles[0].previousQuestId=10002; assert(candidates().empty()); profiles[0].previousQuestId=0;
    profiles[0].semanticAmbiguous=true; assert(candidates().empty()); profiles[0].semanticAmbiguous=false;
    profiles[0].sourceMetadata.reset(); assert(candidates().empty()); profiles[0]=p;
    context.completed.insert(p.questId); assert(candidates().empty()); context.completed.clear();
    snapshot.quests.push_back({"Fixture",false,1,{false}}); assert(candidates().empty()); snapshot.quests.clear();
    backoff[17]=900; assert(candidates(899).empty()); assert(candidates(900).size()==1); backoff.clear();
    auto other=p; other.questId++; other.giverEntry++; profiles.push_back(other);
    assert(candidates()[0].giverEntry==17 && candidates()[0].usefulQuests==2);
    std::reverse(profiles.begin(),profiles.end()); assert(candidates()[0].giverEntry==17);
    assert(!RegionalQuestRelocationPolicy::IdleAction(QuestPlannerAction::ExecuteObjective));
    assert(!RegionalQuestRelocationPolicy::IdleAction(QuestPlannerAction::TurnIn));
    assert(!RegionalQuestRelocationPolicy::IdleAction(QuestPlannerAction::TemporarilyBlocked));
    assert(RegionalQuestRelocationPolicy::IdleAction(QuestPlannerAction::DiscoverPickup));
    // Native builds cannot instantiate the Windows controller. Check actual
    // boundary wiring in addition to the executable pure policy tests above.
    std::ifstream file("src/Bot/QuestPlannerRuntimeController.h"); assert(file);
    const std::string source((std::istreambuf_iterator<char>(file)),{});
    assert(source.find("focusQuestId_==0 && valleyDiscoverySweepComplete_")!=std::string::npos);
    assert(source.find("discoveryRetryAfterTick_ != 0 || relocation_.Active()")!=std::string::npos);
    assert(source.find("relocation_.Cancel();")!=std::string::npos);
}
