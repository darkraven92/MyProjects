#pragma once
#include "QuestAcquisitionPolicy.h"

namespace Bot
{
    // A title in the server UI is not yet an identified quest. In particular,
    // it cannot satisfy every same-title candidate's unknown prerequisites.
    struct QuestOfferResolutionPolicy
    {
        static constexpr int EmptyCacheVersion = 2;
        static QuestAcquisitionResult Evaluate(const QuestProfile& profile,
            const QuestGraphNode* node, QuestEligibilityContext context)
        {
            context.liveOffer = false;
            return QuestAcquisitionPolicy::Evaluate(profile, node, context, false, true);
        }
        static bool ConfirmEmpty(std::size_t liveOfferCount)
        { return liveOfferCount == 0; }
        static bool RestoreEmptyCache(int version)
        { return version == EmptyCacheVersion; }

        std::size_t candidateCount = 0;
        std::size_t eligibleCount = 0;
        const QuestProfile* eligible = nullptr;
        void Observe(const QuestProfile& profile, QuestAcquisitionResult result, bool inScope = true)
        {
            ++candidateCount;
            if (inScope && result == QuestAcquisitionResult::EligibleForPickup)
            { ++eligibleCount; eligible = &profile; }
        }
        const QuestProfile* Resolved() const
        { return eligibleCount == 1 ? eligible : nullptr; }
    };
}
