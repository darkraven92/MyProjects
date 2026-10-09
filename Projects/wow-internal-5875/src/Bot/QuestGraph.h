#pragma once

#include "QuestClassificationPolicy.h"
#include <algorithm>
#include <functional>
#include <map>
#include <set>

namespace Bot
{
    enum class QuestGraphEdgeKind { RequiresQuest, LeadsToQuest, UnresolvedSignedPrerequisite,
        RequiresActiveQuest, RequiresAllRewarded, RequiresAllActive, RequiresOneOfRewarded,
        ExclusiveWith, BreadcrumbTo };
    struct QuestGraphEdge { int from; int to; QuestGraphEdgeKind kind; };
    struct QuestGraphNode
    {
        const QuestProfile* profile = nullptr; // immutable catalogue lifetime
        QuestClassification classification{};
        bool structurallyValid = true;
    };
    enum class QuestGraphIssueKind { InvalidIdentity, DuplicateIdentity, InvalidLevel, InvalidMask,
        MissingReference, SelfPrerequisite, PrerequisiteCycle, OrphanGenerated, UnresolvedSignedRelationship };
    struct QuestGraphIssue { int questId; QuestGraphIssueKind kind; };

    class QuestGraph
    {
        std::map<int, QuestGraphNode> nodes_;
        std::vector<QuestGraphEdge> edges_;
        std::vector<QuestGraphIssue> issues_;
    public:
        explicit QuestGraph(const std::vector<QuestProfile>& profiles)
        {
            for (const auto& p : profiles)
            {
                if (p.questId <= 0 || !p.title || !*p.title)
                { issues_.push_back({p.questId, QuestGraphIssueKind::InvalidIdentity}); continue; }
                if (!nodes_.emplace(p.questId, QuestGraphNode{&p, QuestClassificationPolicy::Classify(p)}).second)
                    issues_.push_back({p.questId, QuestGraphIssueKind::DuplicateIdentity});
                if (p.minimumLevel < 0 || p.minimumLevel > 60 || p.questLevel < -1 || p.questLevel > 63 ||
                    (p.maximumLevel && (*p.maximumLevel < 0 ||
                        (*p.maximumLevel > 0 && *p.maximumLevel < p.minimumLevel))))
                    issues_.push_back({p.questId, QuestGraphIssueKind::InvalidLevel});
                // Vanilla race bits 1..8; class IDs 1..5,7..9,11.
                if ((p.requiredRaceMask && (*p.requiredRaceMask & ~0xffu)) ||
                    (p.requiredClassMask && (*p.requiredClassMask & ~0x5dfu)))
                    issues_.push_back({p.questId, QuestGraphIssueKind::InvalidMask});
                if (p.databaseDerived && !p.giverEntry && !p.turnInEntry)
                    issues_.push_back({p.questId, QuestGraphIssueKind::OrphanGenerated});
                if (p.relationshipMetadataKnown)
                {
                    for (const auto& clause : p.prerequisiteAlternatives)
                        for (int id : clause.allOf)
                            edges_.push_back({p.questId, id, !clause.resolved
                                ? QuestGraphEdgeKind::UnresolvedSignedPrerequisite
                                : clause.requiresActive
                                    ? (clause.allOf.size() > 1 ? QuestGraphEdgeKind::RequiresAllActive : QuestGraphEdgeKind::RequiresActiveQuest)
                                    : (clause.allOf.size() > 1 ? QuestGraphEdgeKind::RequiresAllRewarded : QuestGraphEdgeKind::RequiresOneOfRewarded)});
                    for (int id : p.exclusivePeers) edges_.push_back({p.questId, id, QuestGraphEdgeKind::ExclusiveWith});
                    if (p.sourceMetadata && p.sourceMetadata->breadcrumb && *p.sourceMetadata->breadcrumb > 0)
                        edges_.push_back({p.questId, *p.sourceMetadata->breadcrumb, QuestGraphEdgeKind::BreadcrumbTo});
                }
                else if (p.previousQuestId > 0)
                    edges_.push_back({p.questId, p.previousQuestId, QuestGraphEdgeKind::RequiresQuest});
                else if (p.previousQuestId < 0)
                {
                    edges_.push_back({p.questId, p.previousQuestId, QuestGraphEdgeKind::UnresolvedSignedPrerequisite});
                    issues_.push_back({p.questId, QuestGraphIssueKind::UnresolvedSignedRelationship});
                }
                if (p.nextInChainQuestId > 0)
                    edges_.push_back({p.questId, p.nextInChainQuestId, QuestGraphEdgeKind::LeadsToQuest});
                if (p.nextQuestId && *p.nextQuestId > 0 && *p.nextQuestId != p.nextInChainQuestId)
                    edges_.push_back({p.questId, *p.nextQuestId, QuestGraphEdgeKind::LeadsToQuest});
            }
            std::map<int, std::vector<int>> prerequisites;
            for (const auto& edge : edges_)
            {
                if (edge.kind == QuestGraphEdgeKind::UnresolvedSignedPrerequisite) continue;
                if (!Find(edge.to)) issues_.push_back({edge.from, QuestGraphIssueKind::MissingReference});
                if (edge.kind != QuestGraphEdgeKind::RequiresQuest) continue;
                if (edge.from == edge.to) issues_.push_back({edge.from, QuestGraphIssueKind::SelfPrerequisite});
                prerequisites[edge.from].push_back(edge.to);
            }
            // Only RequiresQuest edges participate. A follow-up loop does
            // not by itself establish an impossible prerequisite cycle.
            std::map<int, int> colors;
            std::vector<int> stack;
            std::function<void(int)> visit = [&](int id) {
                if (colors[id] == 1)
                {
                    // Every member is invalid, not only the back-edge target.
                    const auto first = std::find(stack.begin(), stack.end(), id);
                    for (auto member = first; member != stack.end(); ++member)
                        issues_.push_back({*member, QuestGraphIssueKind::PrerequisiteCycle});
                    return;
                }
                if (colors[id] == 2) return;
                colors[id] = 1;
                stack.push_back(id);
                for (int previous : prerequisites[id]) visit(previous);
                stack.pop_back();
                colors[id] = 2;
            };
            for (const auto& [id, node] : nodes_) { (void)node; visit(id); }
            for (const auto& issue : issues_)
                if (issue.kind == QuestGraphIssueKind::InvalidLevel || issue.kind == QuestGraphIssueKind::InvalidMask ||
                    issue.kind == QuestGraphIssueKind::DuplicateIdentity || issue.kind == QuestGraphIssueKind::SelfPrerequisite ||
                    issue.kind == QuestGraphIssueKind::PrerequisiteCycle)
                    if (auto node = nodes_.find(issue.questId); node != nodes_.end())
                    {
                        node->second.classification.support = QuestRuntimeSupport::Ambiguous;
                        node->second.classification.reason = "invalid_graph_metadata";
                        node->second.structurallyValid = false;
                    }
        }
        const QuestGraphNode* Find(int id) const
        { const auto found = nodes_.find(id); return found == nodes_.end() ? nullptr : &found->second; }
        const auto& Nodes() const { return nodes_; }
        const auto& Edges() const { return edges_; }
        const auto& Issues() const { return issues_; }
    };

    inline const char* QuestEligibilityName(QuestEligibility value)
    {
        switch (value)
        {
#define QUEST_ELIGIBILITY_NAME(name) case QuestEligibility::name: return #name;
            QUEST_ELIGIBILITY_NAME(Eligible) QUEST_ELIGIBILITY_NAME(TooLowLevel)
            QUEST_ELIGIBILITY_NAME(TooHighLevel) QUEST_ELIGIBILITY_NAME(RaceMismatch)
            QUEST_ELIGIBILITY_NAME(ClassMismatch) QUEST_ELIGIBILITY_NAME(FactionMismatch)
            QUEST_ELIGIBILITY_NAME(MissingPrerequisite) QUEST_ELIGIBILITY_NAME(PrerequisiteUnknown)
            QUEST_ELIGIBILITY_NAME(ExclusiveConflict) QUEST_ELIGIBILITY_NAME(AlreadyActive)
            QUEST_ELIGIBILITY_NAME(ReadyForTurnIn) QUEST_ELIGIBILITY_NAME(Completed)
            QUEST_ELIGIBILITY_NAME(Deferred) QUEST_ELIGIBILITY_NAME(TemporarilyBlocked)
            QUEST_ELIGIBILITY_NAME(Unsupported) QUEST_ELIGIBILITY_NAME(NonExecutable)
            QUEST_ELIGIBILITY_NAME(AmbiguousMetadata) QUEST_ELIGIBILITY_NAME(UnknownRestrictions)
#undef QUEST_ELIGIBILITY_NAME
        }
        return "UnknownRestrictions";
    }
    struct QuestEligibilityContext
    {
        int level = 0;
        std::optional<std::uint32_t> raceMask{}, classMask{}, factionMask{};
        std::set<int> completed{}, knownMissingPrerequisites{}, activeQuestIds{};
        bool active = false, liveComplete = false, liveOffer = false;
        bool deferred = false, blocked = false, exclusiveConflict = false;
        bool historyComplete = false;
        bool activeHistoryComplete = false;
    };
    struct QuestEligibilityPolicy
    {
        static std::optional<std::uint32_t> ClassMask(const std::string& token)
        {
            const char* tokens[] = {"WARRIOR", "PALADIN", "HUNTER", "ROGUE", "PRIEST", "", "SHAMAN", "MAGE", "WARLOCK", "", "DRUID"};
            for (std::uint32_t i = 0; i < 11; ++i)
                if (token == tokens[i] && !token.empty()) return 1u << i;
            return std::nullopt;
        }
        static QuestEligibility Evaluate(const QuestProfile& p,
            const QuestClassification& classification, const QuestEligibilityContext& c)
        {
            // Live accepted/complete state is stronger than inferred history.
            if (c.active && c.liveComplete)
                return p.turnInIsGameObject ? QuestEligibility::NonExecutable : QuestEligibility::ReadyForTurnIn;
            if (!c.active && c.completed.count(p.questId)) return QuestEligibility::Completed;
            if (classification.support == QuestRuntimeSupport::Ambiguous) return QuestEligibility::AmbiguousMetadata;
            if (classification.support == QuestRuntimeSupport::KnownSemanticButUnsupported) return QuestEligibility::Unsupported;
            if (classification.support == QuestRuntimeSupport::MissingData) return QuestEligibility::NonExecutable;
            if (c.deferred) return QuestEligibility::Deferred;
            if (c.blocked) return QuestEligibility::TemporarilyBlocked;
            if (c.active) return QuestEligibility::AlreadyActive;
            if (c.exclusiveConflict) return QuestEligibility::ExclusiveConflict;
            // An actual server-offered quest is positive eligibility evidence;
            // a static giver seed or focus ID is not an offer.
            if (c.liveOffer) return QuestEligibility::Eligible;
            if (c.level < p.minimumLevel) return QuestEligibility::TooLowLevel;
            if (p.maximumLevel && *p.maximumLevel > 0 && c.level > *p.maximumLevel) return QuestEligibility::TooHighLevel;
            if (p.relationshipMetadataKnown)
            {
                for (int peer : p.exclusivePeers)
                    if (c.activeQuestIds.count(peer) || c.completed.count(peer)) return QuestEligibility::ExclusiveConflict;
                if (p.sourceMetadata && ((p.sourceMetadata->requiredSkill && *p.sourceMetadata->requiredSkill) ||
                    (p.sourceMetadata->breadcrumb && *p.sourceMetadata->breadcrumb)))
                    return QuestEligibility::UnknownRestrictions;
                if (!p.prerequisiteAlternatives.empty())
                {
                    bool satisfied = false, unknown = false;
                    for (const auto& clause : p.prerequisiteAlternatives)
                    {
                        bool all = clause.resolved && !clause.allOf.empty();
                        unknown |= !clause.resolved;
                        for (int id : clause.allOf)
                        {
                            const bool present = clause.requiresActive ? c.activeQuestIds.count(id) : c.completed.count(id);
                            all &= present;
                            if (!present && !clause.requiresActive && !c.historyComplete && !c.knownMissingPrerequisites.count(id)) unknown = true;
                            if (!present && clause.requiresActive && !c.activeHistoryComplete) unknown = true;
                        }
                        satisfied |= all;
                    }
                    if (!satisfied) return unknown ? QuestEligibility::PrerequisiteUnknown : QuestEligibility::MissingPrerequisite;
                }
            }
            if (p.requiredRaceMask && *p.requiredRaceMask && c.raceMask && !(*p.requiredRaceMask & *c.raceMask)) return QuestEligibility::RaceMismatch;
            if (p.requiredClassMask && *p.requiredClassMask && c.classMask && !(*p.requiredClassMask & *c.classMask)) return QuestEligibility::ClassMismatch;
            if (p.requiredFactionMask && *p.requiredFactionMask && c.factionMask && !(*p.requiredFactionMask & *c.factionMask)) return QuestEligibility::FactionMismatch;
            if (p.classToken && *p.classToken)
            {
                const auto required = ClassMask(p.classToken);
                if (!required || !c.classMask) return QuestEligibility::UnknownRestrictions;
                if (!(*required & *c.classMask)) return QuestEligibility::ClassMismatch;
            }
            if ((p.requiredRaceMask && *p.requiredRaceMask && !c.raceMask) ||
                (p.requiredClassMask && *p.requiredClassMask && !c.classMask) ||
                (p.requiredFactionMask && *p.requiredFactionMask && !c.factionMask) ||
                (p.requiredCondition && *p.requiredCondition) ||
                (!p.relationshipMetadataKnown && p.exclusiveGroup && *p.exclusiveGroup)) return QuestEligibility::UnknownRestrictions;
            if (p.databaseDerived && (!p.requiredRaceMask || !p.requiredClassMask))
                return QuestEligibility::UnknownRestrictions;
            if (!p.relationshipMetadataKnown && p.previousQuestId < 0) return QuestEligibility::AmbiguousMetadata;
            if (!p.relationshipMetadataKnown && p.previousQuestId > 0 && !c.completed.count(p.previousQuestId))
                return c.historyComplete || c.knownMissingPrerequisites.count(p.previousQuestId)
                    ? QuestEligibility::MissingPrerequisite : QuestEligibility::PrerequisiteUnknown;
            return QuestEligibility::Eligible;
        }
    };
}
