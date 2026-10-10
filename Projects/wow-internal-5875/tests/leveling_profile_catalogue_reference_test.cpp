#include "../src/Bot/LevelingProfile.h"
#include <cassert>

int main()
{
    using namespace Bot;
    LevelingProfileSegment segment;
    segment.id = "start";
    segment.minimumLevel = 1;
    segment.maximumLevel = 6;
    segment.questIds = {788, 789, 2383};
    LevelingProfile profile;
    profile.id = "synthetic-orc";
    profile.segments = {segment};
    const std::set<std::uint32_t> ids{788, 789, 2383};
    assert(LevelingProfileValidation::ValidCatalogue({profile}, ids));
    assert(LevelingProfileValidation::ValidCatalogue({profile}));
    const auto missing = LevelingProfileValidation::MissingQuestReferences({profile}, {789});
    assert(missing.size() == 2);
    assert(missing[0].questId == 788 && missing[1].questId == 2383);
    assert((missing[0].segment == ProfileSegmentId{"synthetic-orc", "start"}));
    assert(!LevelingProfileValidation::ValidCatalogue({profile}, {789}));
    std::reverse(profile.segments[0].questIds.begin(), profile.segments[0].questIds.end());
    assert(LevelingProfileValidation::MissingQuestReferences({profile}, {789}) == missing);
    profile.segments[0].questIds.push_back(788);
    assert(!LevelingProfileValidation::ValidCatalogue({profile}, ids));
    assert(LevelingProfileValidation::MissingQuestReferences({profile}, {789}) == missing);
    profile.segments[0].questIds = {0};
    assert(!LevelingProfileValidation::ValidCatalogue({profile}, {0}));
    // Reference resolution does not weaken structural level/link validation.
    profile.segments[0].questIds = {788};
    profile.segments[0].nextSegment = "missing";
    assert(!LevelingProfileValidation::ValidCatalogue({profile}, ids));
}
