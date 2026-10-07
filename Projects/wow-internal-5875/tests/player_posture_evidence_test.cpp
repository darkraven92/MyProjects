#include "../src/Bot/PlayerPostureEvidencePolicy.h"
#include "../src/Bot/RuntimeRobustnessSupervisor.h"
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
    using Bot::PlayerPostureEvidencePolicy;
    static_assert(PlayerPostureEvidencePolicy::DescriptorOffset == 0x228);
    std::array<unsigned char, 0x22c> descriptors{};
    descriptors[0x210] = 51; // The captured native-display-ID byte, NOT posture.
    std::uint32_t bytes = 0;
    std::memcpy(&bytes, descriptors.data() + PlayerPostureEvidencePolicy::DescriptorOffset, 4);
    const auto standing = PlayerPostureEvidencePolicy::Decode(bytes);
    assert(standing.known && standing.state == 0);
    assert(!PlayerPostureEvidencePolicy::Decode(51).known);
    assert(!PlayerPostureEvidencePolicy::Decode(255).known);
    for (unsigned state = 0; state <= 9; ++state)
    {
        const auto decoded = PlayerPostureEvidencePolicy::Decode(0x12340000 | state);
        assert(decoded.known && decoded.state == state);
    }
    Bot::RuntimeRobustnessSample s{};
    s.active = true;
    s.playerHealth = s.playerMaxHealth = 100;
    s.grindState = 1;
    s.combatState = 1;
    s.postureValid = standing.known;
    s.playerStanding = standing.state == 0;
    Bot::RuntimeRobustnessSupervisor supervisor;
    for (std::uint64_t tick = 0; tick < 40; ++tick)
        assert(supervisor.Update(s, tick).reason != Bot::RuntimeRobustnessReason::UnexpectedSeatedIdle);
    // Correct seated evidence still triggers the original posture watchdog.
    Bot::RuntimeRobustnessSupervisor seated;
    s.playerStanding = false;
    bool recovered = false;
    for (std::uint64_t tick = 1; tick < 40; ++tick)
        recovered |= seated.Update(s, tick).reason == Bot::RuntimeRobustnessReason::UnexpectedSeatedIdle;
    assert(recovered);
    std::ifstream in("src/Bot/PlayerPostureController.h");
    assert(in.good());
    const std::string source(std::istreambuf_iterator<char>(in), {});
    assert(source.find("PlayerPostureEvidencePolicy::DescriptorOffset") != std::string::npos);
    assert(source.find("PlayerPostureEvidencePolicy::Decode(bytes1)") != std::string::npos);
}
