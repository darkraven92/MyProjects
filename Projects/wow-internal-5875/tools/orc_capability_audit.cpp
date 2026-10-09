#include "../src/Bot/QuestAcquisitionPolicy.h"
#include "../src/Bot/QuestPlanner.h"
#include <iostream>

int main()
{
    using namespace Bot;
    QuestEligibilityContext context;
    context.raceMask=QuestAcquisitionPolicy::RaceMask("Orc");
    context.classMask=QuestEligibilityPolicy::ClassMask("WARRIOR");
    context.activeHistoryComplete=true; // deliberately empty fresh log, partial reward history
    std::cout<<"quest\ttitle\tminLevel\tquestLevel\tzone\tsemantic\tsupport\tpickupAt1\tpickupAt10\tgiverSpawn\tobjectiveSpawn\truntime\n";
    for(const auto& p:ValleyOfTrialsProfiles::All())
    {
        if(p.minimumLevel>10 || (p.requiredRaceMask && *p.requiredRaceMask && !(*p.requiredRaceMask&2u)) ||
            (p.requiredClassMask && *p.requiredClassMask && !(*p.requiredClassMask&1u))) continue;
        const auto* node=ValleyOfTrialsProfiles::Graph().Find(p.questId);
        context.level=1;
        const auto first=QuestAcquisitionPolicy::Evaluate(p,node,context);
        context.level=10;
        const auto tenth=QuestAcquisitionPolicy::Evaluate(p,node,context);
        std::cout<<p.questId<<'\t'<<p.title<<'\t'<<p.minimumLevel<<'\t'<<p.questLevel<<'\t'
            <<(p.zoneOrSort?std::to_string(*p.zoneOrSort):"unknown")<<'\t'
            <<QuestPlanner::ObjectiveTypeName(p.objective.type)<<'\t'
            <<QuestClassificationPolicy::SupportName(node->classification.support)<<'\t'
            <<QuestAcquisitionPolicy::Name(first)<<'\t'<<QuestAcquisitionPolicy::Name(tenth)<<'\t'
            <<p.giverDestination.valid<<'\t'<<p.destination.valid<<"\tunverified_by_static_audit\n";
    }
}
