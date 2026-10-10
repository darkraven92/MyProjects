#pragma once
#include <cstdint>

namespace Bot
{
    // Observed engagement is positive evidence; there is deliberately no Safe value.
    enum class LivingDanger { Unknown, Observed };
    struct LivingRecoveryEvidence
    {
        bool identity=false, alive=false, known=false, combat=false;
        bool healthKnown=false, lifeKnown=false, combatKnown=false;
        bool scanComplete=false, aggressor=false;
        bool attackKnown=false, attackActive=false;
        std::uint64_t attacker=0;
        std::uint32_t hp=0, maxHp=0;

        bool Living() const { return identity && healthKnown && lifeKnown && alive; }
        bool Positive() const
        { return identity && ((combatKnown && combat) || aggressor || (attackKnown && attackActive)); }
        LivingDanger Danger() const { return Positive() ? LivingDanger::Observed : LivingDanger::Unknown; }
        bool Complete() const { return Living() && combatKnown && scanComplete && attackKnown; }
        bool QuietObservation() const { return Complete() && !Positive(); }
    };

    // GUID alone survives a same-character world/object replacement.
    struct LivingRecoveryIdentity
    {
        std::uint64_t guid=0;
        std::uint32_t manager=0, player=0, descriptors=0;
        bool Valid() const { return guid && manager && player && descriptors; }
        bool Matches(const LivingRecoveryIdentity& other) const
        {
            return Valid() && other.Valid() && guid==other.guid && manager==other.manager &&
                player==other.player && descriptors==other.descriptors;
        }
    };
}
