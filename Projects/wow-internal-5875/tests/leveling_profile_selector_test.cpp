#include "../src/Bot/LevelingProfileObservation.h"

#include <cassert>
#include <limits>

using namespace Bot;
using Status = LevelingSelectionStatus;
using Transition = LevelingProfileTransition;

// All IDs, levels, positions, facts and maps below are synthetic test inputs.
// They are not an Orc route, production catalogue or live evidence.
static LevelingProfileSegment Segment(const char* id, unsigned minimum, unsigned maximum)
{
    LevelingProfileSegment s;
    s.id = id;
    s.minimumLevel = minimum;
    s.maximumLevel = maximum;
    s.expectedMap = 1;
    return s;
}

static std::vector<LevelingProfile> Catalogue()
{
    LevelingProfile p;
    p.id = "fixture";
    p.segments = {Segment("early", 1, 5), Segment("late", 6, 10)};
    p.segments[0].nextSegment = "late";
    return {p};
}

static ProfileWorldEvidence Evidence()
{
    ProfileWorldEvidence e;
    e.identity.playerGuid = 100;
    e.identity.monitorSession = 1;
    e.freshForPlayer = true;
    e.playerLevel = 1;
    e.mapId = 1;
    return e;
}

static bool HasReason(const LevelingSelectionResult& r, const std::string& reason)
{
    if (r.reason == reason) return true;
    for (const auto& c : r.candidates)
        if (std::find(c.reasons.begin(), c.reasons.end(), reason) != c.reasons.end()) return true;
    return false;
}

static void BoundariesAndScope()
{
    auto profiles = Catalogue();
    auto e = Evidence();
    const auto select = [&] { return LevelingProfileSelector::Select(profiles, e); };
    for (unsigned level = 1; level <= 11; ++level)
    {
        e.playerLevel = level;
        const auto r = select();
        if (level <= 10)
        {
            assert(r.status == Status::Selected);
            assert(r.selected->segmentId == (level <= 5 ? "early" : "late"));
            assert(r.nextSegment.has_value() == (level <= 5));
            if (r.nextSegment) assert(r.nextSegment->segmentId == "late");
        }
        else
        {
            assert(r.status == Status::NoMatch); // Never implicit completion.
            assert(!r.selected && !r.nextSegment);
        }
    }
    profiles[0].segments[0].minimumLevel = 2;
    e.playerLevel = 1;
    assert(select().status == Status::NoMatch);
    e.playerLevel = 2;
    assert(select().status == Status::Selected);
    e.playerLevel = 0;
    assert(select().status == Status::InvalidEvidence);
    e.playerLevel = 61;
    assert(select().status == Status::InvalidEvidence);
    e.playerLevel = std::numeric_limits<unsigned>::max();
    assert(select().status == Status::InvalidEvidence);
    e.playerLevel.reset();
    assert(select().status == Status::EvidenceUnavailable && HasReason(select(), "level_unknown"));
    e = Evidence();
    profiles = Catalogue();
    profiles[0].segments[0].maximumLevel = 4;
    e.playerLevel = 5;
    assert(select().status == Status::NoMatch); // Gap is not nearest-segment selection.
    profiles[0].segments.push_back(Segment("final", 20, 60));
    e.playerLevel = 60;
    assert(select().selected->segmentId == "final"); // Direct jump, no chain walking.
    assert(LevelingProfileSelector::Select({}, e).status == Status::NoMatch);
    assert(LevelingProfileSelector::Select(profiles, e, "missing").status == Status::UnknownProfile);

    auto other = profiles[0];
    other.id = "other";
    profiles.push_back(other);
    assert(select().status == Status::Ambiguous);
    const auto scoped = LevelingProfileSelector::Select(profiles, e, "fixture");
    assert(scoped.status == Status::Selected && scoped.selected->profileId == "fixture");
    e.mapId = 0;
    assert(LevelingProfileSelector::Select(profiles, e, "fixture").status == Status::NoMatch);
}

static void UnknownAndIdentity()
{
    auto profiles = Catalogue();
    auto e = Evidence();
    const auto select = [&] { return LevelingProfileSelector::Select(profiles, e); };
    assert(LevelingProfileSelector::Select(profiles, {}).status == Status::EvidenceUnavailable);
    e.freshForPlayer = false;
    assert(select().status == Status::EvidenceUnavailable);
    e = Evidence(); e.identity.playerGuid.reset();
    assert(select().status == Status::EvidenceUnavailable);
    e.identity.playerGuid = 0;
    assert(select().status == Status::EvidenceUnavailable);
    e = Evidence(); e.identity.monitorSession.reset();
    assert(select().status == Status::EvidenceUnavailable);
    e.identity.monitorSession = 0;
    assert(select().status == Status::EvidenceUnavailable);
    e = Evidence();
    assert(!e.identity.worldGeneration);
    assert(select().status == Status::Selected); // Session is not a world generation.
    e.mapId.reset();
    assert(select().status == Status::EvidenceUnavailable && HasReason(select(), "map_unknown"));
    profiles[0].segments[0].expectedMap.reset();
    assert(select().status == Status::Selected); // Unconstrained really is unconstrained.
    profiles[0].segments[0].expectedMap = 0;
    assert(select().status == Status::EvidenceUnavailable);
    e.mapId = 0;
    assert(select().status == Status::Selected);
    e.mapId = 1;
    assert(select().status == Status::NoMatch && HasReason(select(), "map_mismatch"));

    profiles = Catalogue();
    auto& s = profiles[0].segments[0];
    s.expectedZone = 77;
    assert(select().status == Status::EvidenceUnavailable);
    e.zoneId = 77;
    assert(select().status == Status::EvidenceUnavailable);
    e.zoneId = 78;
    assert(select().status == Status::EvidenceUnavailable); // No qualified zone reader.
    e.mapId = 0;
    assert(select().status == Status::NoMatch); // Known contradiction dominates unknown.
    e = Evidence(); s.expectedZone.reset();
    s.expectedFactionGroup = ProfileFactionGroup::Horde;
    assert(select().status == Status::EvidenceUnavailable);
    e.factionGroup = ProfileFactionGroup::Horde;
    assert(select().status == Status::EvidenceUnavailable);
    e.factionGroup = ProfileFactionGroup::Alliance;
    assert(select().status == Status::EvidenceUnavailable);
    s.expectedFactionGroup.reset();

    s.allowedRaces = 2; s.allowedClasses = 1;
    assert(HasReason(select(), "race_unknown") && HasReason(select(), "class_unknown"));
    e.raceMask = 2; e.classMask = 1;
    assert(select().status == Status::Selected);
    e.raceMask = 1;
    assert(select().status == Status::NoMatch && HasReason(select(), "race_mismatch"));
    e.raceMask = 2; e.classMask = 4;
    assert(select().status == Status::NoMatch && HasReason(select(), "class_mismatch"));
    for (unsigned invalid : {0u, 3u, 256u})
    {
        e.raceMask = invalid; e.classMask = 1;
        assert(select().status == Status::EvidenceUnavailable);
    }
    for (unsigned invalid : {0u, 3u, 32u})
    {
        e.raceMask = 2; e.classMask = invalid;
        assert(select().status == Status::EvidenceUnavailable);
    }
    e = Evidence(); e.raceMask = 2; e.classMask = 1; e.freshForPlayer = false;
    assert(select().status == Status::EvidenceUnavailable); // Cached tokens do not qualify.
}

static void PositionAssociation()
{
    auto profiles = Catalogue();
    auto e = Evidence();
    ProfileGrindRegion region;
    region.minimum = {-1, -1, -1};
    region.maximum = {1, 1, 1};
    region.constrainSelection = true;
    profiles[0].segments[0].grindRegion = region;
    const auto select = [&] { return LevelingProfileSelector::Select(profiles, e); };
    assert(select().status == Status::EvidenceUnavailable);
    e.position = ProfilePosition{0, 0, 0};
    assert(HasReason(select(), "position_map_unknown"));
    e.positionMapId = 1;
    assert(select().status == Status::Selected); // Zero coordinates are not missing.
    for (auto point : {ProfilePosition{-1, -1, -1}, ProfilePosition{1, 1, 1}})
    {
        e.position = point;
        assert(select().status == Status::Selected);
    }
    e.position = ProfilePosition{2, 0, 0};
    assert(select().status == Status::NoMatch && HasReason(select(), "outside_region"));
    e.positionMapId = 0;
    assert(select().status == Status::EvidenceUnavailable); // Foreign coordinates cannot reject.
    e.mapId.reset();
    assert(select().status == Status::EvidenceUnavailable);
    e = Evidence(); e.positionMapId = 1;
    e.position = ProfilePosition{std::numeric_limits<float>::quiet_NaN(), 0, 0};
    assert(select().status == Status::EvidenceUnavailable);
    e.position = ProfilePosition{0, std::numeric_limits<float>::infinity(), 0};
    assert(select().status == Status::EvidenceUnavailable);
    e.position = ProfilePosition{0, 0, -std::numeric_limits<float>::infinity()};
    assert(select().status == Status::EvidenceUnavailable);
    e.position = ProfilePosition{0, 0, 0}; e.mapId = 0;
    assert(select().status == Status::NoMatch);
    profiles[0].segments[0].grindRegion->constrainSelection = false;
    profiles[0].segments[0].expectedMap.reset();
    e = Evidence(); e.mapId.reset();
    assert(select().status == Status::Selected); // Metadata alone never constrains.
}

static void AmbiguityAndOrdering()
{
    auto profiles = Catalogue();
    auto e = Evidence();
    profiles[0].segments.push_back(Segment("overlap", 1, 3));
    auto other = profiles[0];
    other.id = "other";
    other.segments[0].expectedMap = 0;
    profiles.push_back(other);
    const auto expected = LevelingProfileSelector::Select(profiles, e);
    assert(expected.status == Status::Ambiguous && !expected.selected && !expected.nextSegment);
    std::reverse(profiles.begin(), profiles.end());
    for (auto& p : profiles) std::reverse(p.segments.begin(), p.segments.end());
    assert(LevelingProfileSelector::Select(profiles, e) == expected);

    profiles = Catalogue();
    auto contender = Segment("unresolved", 1, 5);
    contender.expectedZone = 77;
    profiles[0].segments.push_back(contender);
    auto r = LevelingProfileSelector::Select(profiles, e);
    assert(r.status == Status::EvidenceUnavailable && !r.selected);
    std::reverse(profiles[0].segments.begin(), profiles[0].segments.end());
    assert(LevelingProfileSelector::Select(profiles, e) == r);
    profiles[0].segments[0].expectedMap = 0;
    assert(LevelingProfileSelector::Select(profiles, e).status == Status::Selected);

    profiles = Catalogue();
    profiles[0].segments[0].preparationRequirements = {"ready"};
    profiles[0].segments.push_back(Segment("other", 1, 5));
    assert(LevelingProfileSelector::Select(profiles, e).status == Status::Ambiguous);
    // Preparation readiness is not an implicit priority among overlapping bands.
}

static void PreparationAndCompletion()
{
    auto profiles = Catalogue();
    auto e = Evidence();
    auto& p = profiles[0];
    p.segments[0].preparationRequirements = {"z", "a"};
    const auto select = [&] { return LevelingProfileSelector::Select(profiles, e); };
    auto r = select();
    assert(r.status == Status::PreparationPending && r.selected);
    assert((r.preparation.unknown == std::vector<std::string>{"a", "z"}));
    e.preparationFacts["a"] = std::nullopt; e.preparationFacts["z"] = false;
    r = select();
    assert((r.preparation.unknown == std::vector<std::string>{"a"}));
    assert((r.preparation.unmet == std::vector<std::string>{"z"}));
    e.preparationFacts["a"] = true; e.preparationFacts["z"] = true;
    assert(select().status == Status::Selected);
    e.freshForPlayer = false;
    assert(!WorldPreparationPolicy::Evaluate(p.segments[0], e).Ready());
    assert(select().status == Status::EvidenceUnavailable);
    e.freshForPlayer = true;

    p.completionFact = "profile_done";
    assert(select().status == Status::EvidenceUnavailable && HasReason(select(), "completion_unknown"));
    e.preparationFacts["profile_done"] = false;
    assert(select().status == Status::Selected);
    e.playerLevel = 11;
    assert(select().status == Status::NoMatch); // Level alone never completes.
    e.preparationFacts["profile_done"] = true;
    r = select();
    assert(r.status == Status::Complete && !r.selected && !r.nextSegment);
    assert((r.completedProfiles == std::vector<std::string>{"fixture"}));
    e.freshForPlayer = false;
    assert(select().status == Status::EvidenceUnavailable);
    e = Evidence(); e.preparationFacts["profile_done"] = true;
    auto other = p; other.id = "second"; other.completionFact.reset();
    other.segments[0].preparationRequirements.clear();
    profiles.push_back(other);
    r = select();
    assert(r.status == Status::Selected && r.selected->profileId == "second");
    std::reverse(profiles.begin(), profiles.end());
    assert(select() == r);
}

static void CatalogueValidation()
{
    const auto invalid = [](auto mutate) {
        auto p = Catalogue();
        mutate(p);
        assert(!LevelingProfileValidation::ValidCatalogue(p));
        assert(LevelingProfileSelector::Select(p, Evidence()).status == Status::InvalidProfile);
    };
    invalid([](auto& p) { p.push_back(p[0]); });
    invalid([](auto& p) { p[0].id.clear(); });
    invalid([](auto& p) { p[0].segments.clear(); });
    invalid([](auto& p) { p[0].segments[0].id.clear(); });
    invalid([](auto& p) { p[0].segments.push_back(p[0].segments[0]); });
    invalid([](auto& p) { p[0].segments[0].minimumLevel = 0; });
    invalid([](auto& p) { p[0].segments[0].minimumLevel = 6; });
    invalid([](auto& p) { p[0].segments[0].maximumLevel = 61; });
    invalid([](auto& p) { p[0].segments[0].allowedRaces = 256; });
    invalid([](auto& p) { p[0].segments[0].allowedClasses = 32; });
    invalid([](auto& p) { p[0].segments[0].allowedRaces = 0; });
    invalid([](auto& p) { p[0].segments[0].nextSegment = "missing"; });
    invalid([](auto& p) { p[0].segments[0].nextSegment = "early"; });
    invalid([](auto& p) { p[0].segments[1].nextSegment = "early"; });
    invalid([](auto& p) { p[0].completionFact = ""; });
    invalid([](auto& p) { p[0].segments[0].preparationRequirements = {""}; });
    invalid([](auto& p) { p[0].segments[0].preparationRequirements = {"a", "a"}; });
    invalid([](auto& p) { p[0].segments[0].allowedTargetEntries = {0}; });
    invalid([](auto& p) { p[0].segments[0].allowedTargetEntries = {1, 1}; });
    invalid([](auto& p) { p[0].segments[0].questIds = {0}; });
    invalid([](auto& p) { p[0].segments[0].questIds = {1, 1}; });
    invalid([](auto& p) { p[0].segments[0].allowedTargetClassifications = {"Beast"}; });
    invalid([](auto& p) { p[0].segments[0].allowedTargetClassifications = {"normal", "normal"}; });
    invalid([](auto& p) {
        p[0].segments[0].grindRegion = ProfileGrindRegion{};
        p[0].segments[0].grindRegion->minimum.x = 2;
    });
    invalid([](auto& p) {
        p[0].segments[0].grindRegion = ProfileGrindRegion{};
        p[0].segments[0].grindRegion->maximum.z = std::numeric_limits<float>::infinity();
    });
    invalid([](auto& p) {
        p[0].segments[0].expectedMap.reset(); p[0].segments[0].expectedZone = 77;
    });
    invalid([](auto& p) {
        p[0].segments[0].expectedMap.reset();
        p[0].segments[0].grindRegion = ProfileGrindRegion{};
        p[0].segments[0].grindRegion->constrainSelection = true;
    });
    auto profiles = Catalogue();
    profiles[0].segments[0].nextSegment.reset();
    profiles[0].segments.push_back(Segment("independent", 20, 30));
    assert(LevelingProfileValidation::ValidCatalogue(profiles)); // Multiple terminals.
    profiles[0].segments[0].nextSegment = "late";
    profiles[0].segments[2].nextSegment = "late";
    assert(LevelingProfileValidation::ValidCatalogue(profiles)); // Shared successor, not a level chain.
}

static void TransitionsAndPurity()
{
    auto profiles = Catalogue();
    auto e = Evidence();
    LevelingProfileObservationTracker tracker;
    auto r = tracker.Observe(profiles, e);
    assert(r.transition == Transition::Initial && r.selection.status == Status::Selected);
    assert(tracker.Observe(profiles, e).transition == Transition::Unchanged);
    e.position = ProfilePosition{100, 200, 300};
    e.playerLevel = 2;
    assert(tracker.Observe(profiles, e).transition == Transition::Unchanged);
    e.playerLevel = 6;
    r = tracker.Observe(profiles, e);
    assert(r.transition == Transition::SelectionChanged && r.selection.selected->segmentId == "late");
    assert(tracker.Observe(profiles, e).transition == Transition::Unchanged);
    e.mapId.reset();
    r = tracker.Observe(profiles, e);
    assert(r.transition == Transition::ContextChanged && !r.selection.selected);
    assert(tracker.Observe(profiles, e).transition == Transition::Unchanged);
    e.mapId = 1;
    assert(tracker.Observe(profiles, e).selection.status == Status::Selected);
    e.freshForPlayer = false;
    r = tracker.Observe(profiles, e);
    assert(r.transition == Transition::SelectionChanged && !r.selection.selected);
    e.freshForPlayer = true;
    assert(tracker.Observe(profiles, e).transition == Transition::SelectionChanged);
    e.identity.playerGuid = 101;
    assert(tracker.Observe(profiles, e).transition == Transition::ContextChanged);
    e.identity.monitorSession = 2;
    assert(tracker.Observe(profiles, e).transition == Transition::ContextChanged);
    assert(!e.identity.worldGeneration); // Never synthesized from session changes.
    e.identity.worldGeneration = 7;
    assert(tracker.Observe(profiles, e).transition == Transition::ContextChanged);
    e.identity.worldGeneration.reset();
    assert(tracker.Observe(profiles, e).transition == Transition::ContextChanged);
    e.identity.playerGuid.reset();
    r = tracker.Observe(profiles, e);
    assert(r.transition == Transition::ContextChanged && !r.selection.selected);
    e = Evidence();
    tracker.Observe(profiles, e);
    assert(tracker.Observe(profiles, e, "fixture").transition == Transition::ContextChanged);
    std::reverse(profiles[0].segments.begin(), profiles[0].segments.end());
    assert(tracker.Observe(profiles, e, "fixture").transition == Transition::Unchanged);
    tracker.Reset();
    assert(tracker.Observe(profiles, e).transition == Transition::Initial);

    const auto originalProfiles = profiles;
    const auto originalEvidence = e;
    const auto expected = LevelingProfileSelector::Select(profiles, e);
    for (int i = 0; i < 10; ++i)
    {
        assert(LevelingProfileSelector::Select(profiles, e) == expected);
        tracker.Observe(profiles, e);
    }
    assert(profiles == originalProfiles && e == originalEvidence);
    // This native executable links only the standard library and these portable
    // policies. There are no controller handles, callbacks, client or input APIs.
}

int main()
{
    BoundariesAndScope();
    UnknownAndIdentity();
    PositionAssociation();
    AmbiguityAndOrdering();
    PreparationAndCompletion();
    CatalogueValidation();
    TransitionsAndPurity();
}
