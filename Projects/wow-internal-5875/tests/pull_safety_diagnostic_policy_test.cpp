#include "../src/Bot/PullSafetyDiagnosticPolicy.h"

#include <cassert>
#include <cstdint>
#include <string_view>
#include <vector>

namespace
{
    Bot::PullSafetyUnit Unit(
        std::uint64_t guid, float x, float playerDistance,
        bool eligible = true)
    {
        Bot::PullSafetyUnit unit{};
        unit.guid = guid;
        unit.entry = 3256;
        unit.x = x;
        unit.playerDistance = playerDistance;
        unit.live = true;
        unit.eligible = eligible;
        unit.nearbyHostile = true;
        return unit;
    }
}

int main()
{
    using Bot::PullSafetyCandidateDecision;
    using Bot::PullSafetyDecision;
    using Bot::PullSafetyDiagnosticPolicy;
    using Bot::PullSafetyPolicy;

    std::vector<Bot::PullSafetyUnit> one{Unit(1, 0.0f, 8.0f)};
    const auto oneSelected = PullSafetyPolicy::Select(one, 95.0f);
    const auto oneRecords = PullSafetyDiagnosticPolicy::Evaluate(
        one, 95.0f, oneSelected);
    assert(oneRecords.size() == 1);
    assert(!PullSafetyDiagnosticPolicy::ShouldEmit(oneSelected, oneRecords));
    assert(oneRecords[0].decision == PullSafetyCandidateDecision::Selected);

    // A single rejected packed target is worth logging even without a
    // second eligible target. Non-eligible neighbors remain risk evidence.
    std::vector<Bot::PullSafetyUnit> packed{
        Unit(1, 0.0f, 8.0f), Unit(8, 5.0f, 13.0f, false),
        Unit(9, -5.0f, 13.0f, false)};
    const auto packedSelection = PullSafetyPolicy::Select(packed, 95.0f);
    const auto packedRecords = PullSafetyDiagnosticPolicy::Evaluate(
        packed, 95.0f, packedSelection);
    assert(packedSelection.decision == PullSafetyDecision::NoSafeCandidate);
    assert(packedRecords.size() == 1);
    assert(packedRecords[0].assessment.predictedAdds == 2);
    assert(packedRecords[0].decision == PullSafetyCandidateDecision::Reject);
    assert(PullSafetyDiagnosticPolicy::ShouldEmit(
        packedSelection, packedRecords));
    assert(PullSafetyDiagnosticPolicy::ReasonName(packedRecords[0]) ==
        std::string_view("too_many_predicted_adds"));

    // The farther isolated GUID still wins. Explaining both candidates must
    // not alter the selected GUID, count, rejection count, or ordering.
    std::vector<Bot::PullSafetyUnit> alternatives{
        Unit(2, 40.0f, 25.0f), Unit(1, 0.0f, 5.0f),
        Unit(8, 5.0f, 12.0f, false), Unit(9, -5.0f, 12.0f, false)};
    const auto before = PullSafetyPolicy::Select(alternatives, 95.0f);
    assert(before.decision == PullSafetyDecision::Voluntary);
    assert(alternatives[before.selectedIndex].guid == 2);
    const auto records = PullSafetyDiagnosticPolicy::Evaluate(
        alternatives, 95.0f, before);
    const auto repeat = PullSafetyDiagnosticPolicy::Evaluate(
        alternatives, 95.0f, before);
    const auto after = PullSafetyPolicy::Select(alternatives, 95.0f);
    assert(PullSafetyDiagnosticPolicy::ShouldEmit(before, records));
    assert(records.size() == 2 && repeat.size() == 2);
    assert(alternatives[records[0].index].guid == 1);
    assert(records[0].assessment.predictedAdds == 2);
    assert(records[0].decision == PullSafetyCandidateDecision::Reject);
    assert(alternatives[records[1].index].guid == 2);
    assert(records[1].assessment.predictedAdds == 0);
    assert(records[1].decision == PullSafetyCandidateDecision::Selected);
    assert(records[0].index == repeat[0].index);
    assert(records[1].index == repeat[1].index);
    assert(before.decision == after.decision);
    assert(before.selectedIndex == after.selectedIndex);
    assert(before.candidateCount == after.candidateCount);
    assert(before.rejectedCount == after.rejectedCount);

    std::vector<Bot::PullSafetyUnit> twoSafe{
        Unit(3, 0.0f, 5.0f), Unit(4, 40.0f, 8.0f)};
    const auto safeSelection = PullSafetyPolicy::Select(twoSafe, 95.0f);
    const auto safeRecords = PullSafetyDiagnosticPolicy::Evaluate(
        twoSafe, 95.0f, safeSelection);
    assert(safeRecords.size() == 2);
    assert(safeRecords[0].decision == PullSafetyCandidateDecision::Selected);
    assert(safeRecords[1].decision == PullSafetyCandidateDecision::Eligible);
    assert(PullSafetyDiagnosticPolicy::ShouldEmit(
        safeSelection, safeRecords));

    // Diagnostic work is bounded by the same evaluated shortlist cap and
    // does not fabricate voluntary decisions during health/defense holds.
    std::vector<Bot::PullSafetyUnit> many;
    for (std::uint64_t guid = 1; guid <= 40; ++guid)
        many.push_back(Unit(guid, static_cast<float>(guid * 40),
            static_cast<float>(guid)));
    const auto manySelection = PullSafetyPolicy::Select(many, 95.0f);
    assert(PullSafetyDiagnosticPolicy::Evaluate(
        many, 95.0f, manySelection).size() ==
        PullSafetyPolicy::MaximumEvaluatedCandidates);
    const auto lowHealth = PullSafetyPolicy::Select(many, 50.0f);
    assert(PullSafetyDiagnosticPolicy::Evaluate(
        many, 50.0f, lowHealth).empty());
    many[0].directAggressor = true;
    const auto defensive = PullSafetyPolicy::Select(many, 10.0f);
    assert(PullSafetyDiagnosticPolicy::Evaluate(
        many, 10.0f, defensive).empty());
}
