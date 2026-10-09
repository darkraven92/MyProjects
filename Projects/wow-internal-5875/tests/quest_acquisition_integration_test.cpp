#include "../src/Bot/QuestAcquisitionPolicy.h"
#include "../src/Bot/QuestPlanner.h"
#include <cassert>
#include <fstream>
#include <sstream>
static std::string Source(const char* path)
{ std::ifstream file(path); assert(file); std::ostringstream text; text<<file.rdbuf(); return text.str(); }
int main()
{
    using namespace Bot;
    const QuestRevisitBoundary owners[]={{true,false,false,false,false},{false,true,false,false,false},
        {false,false,true,false,false},{false,false,false,true,false},{false,false,false,false,true}};
    for(const auto& owner:owners)
    {
        assert(!QuestPlanner::MayReplaceSelection(owner));
        assert(!QuestAcquisitionPolicy::ReauditDue(true,true,CanEvaluateQuestRevisit(owner),1000,900,20,20));
    }
    assert(!QuestPlanner::MayReplaceSelection({},true)); // death ownership
    assert(QuestPlanner::MayReplaceSelection({}));
    const auto pickup=Source("src/Bot/GenericQuestDiscoveryController.h");
    assert(pickup.find("QuestFrameDetailPanel:IsVisible() and t==")!=std::string::npos);
    assert(pickup.find("AcceptQuest()")!=std::string::npos);
    assert(pickup.find("VerifiedAcceptance")!=std::string::npos);
    assert(pickup.find("AcceptanceExpired")!=std::string::npos);
    assert(pickup.find("defense_.Update(world,combat,giverNavigator_.get()")!=std::string::npos);
    assert(pickup.find("MaximumInteractionAttempts = 2")!=std::string::npos);
    assert(pickup.find("QuestAcquisitionPolicy::Evaluate")!=std::string::npos);
    const auto turnin=Source("src/Bot/GenericQuestTurnInExecutor.h");
    assert(turnin.find("QuestFrameRewardPanel:IsVisible() and t==expected")!=std::string::npos);
    assert(turnin.find("GetQuestReward(0)")!=std::string::npos);
    assert(turnin.find("IsQuestCompletable()")!=std::string::npos);
    assert(turnin.find("rewardController_.Update(tick)")!=std::string::npos);
    assert(turnin.find("VerifiedTurnIn(true,questRemovalObserved_,RewardVerified())")!=std::string::npos);
    assert(turnin.find("profile.turnInIsGameObject ||")!=std::string::npos);
    const auto runtime=Source("src/Bot/QuestPlannerRuntimeController.h");
    assert(runtime.find("!objectiveDirector_.OwnsControl() && !startAttempted_")!=std::string::npos);
    assert(runtime.find("objectiveRetryAfterTick_==0 && turnInProfile_==nullptr")!=std::string::npos);
    assert(runtime.find("RecordCompletedQuest(*completedProfile)")!=std::string::npos);
    assert(runtime.find("candidate.prerequisiteAlternatives")!=std::string::npos);
    assert(runtime.find("TitleStillPresent(profile,snapshot_)")!=std::string::npos);
    assert(QuestFocusPolicy::ParseConfiguration("0").questId==0);
    assert(!QuestFocusPolicy::Allows(10001,10002));
    assert(QuestDeferPolicy::ObjectiveFailureThreshold==3);
}
