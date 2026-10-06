#include "../src/Navigation/PathValidationDiagnosticPolicy.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

int main()
{
    using namespace Navigation;
    const auto detail = PathValidationDiagnosticPolicy::UnsafeTerrainNoAlternative(
        11, 22, 4.5f, 9.0f, 1, 17, 2);
    assert(detail.reason == PathValidationSubreason::UnsafeTerrainNoAlternative);
    assert(std::string_view(PathValidationDiagnosticPolicy::Name(detail.reason)) ==
        "unsafe_terrain_no_alternative");
    assert(detail.fromPoly == 11 && detail.toPoly == 22);
    assert(detail.verticalDelta == 9.0f && detail.alternativeAttempt == 2);
    assert(!PathValidationDiagnosticPolicy::ChangesSafetyDecision(detail));
    std::ifstream input("src/Navigation/GenericNavMeshPathFollower.h");
    assert(input);
    const std::string source{std::istreambuf_iterator<char>(input),{}};
    assert(source.find("PATH VALIDATION FAILED reason=")!=std::string::npos);
    assert(source.find("PathValidationDiagnosticPolicy::UnsafeTerrainNoAlternative(")!=
        std::string::npos);
    assert(source.find("lastPlanFailure_ = directedNoAlternative")!=
        std::string::npos);
    assert(source.find("provider_.ValidateTerrainRoute(PlayerPoint(player), path)")!=
        std::string::npos);
}
