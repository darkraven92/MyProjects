#pragma once

#include "QuestObjectiveDispatchPolicy.h"
#include "QuestSourceSemanticPolicy.h"
#include <cmath>

namespace Bot
{
    struct QuestClassification
    {
        QuestObjectiveType semantic = QuestObjectiveType::Unknown;
        QuestRuntimeSupport support = QuestRuntimeSupport::MissingData;
        QuestClassificationConfidence confidence = QuestClassificationConfidence::Unresolved;
        QuestExecutorKind executor = QuestExecutorKind::None;
        const char* reason = "missing_semantic_or_identity";
    };

    struct QuestClassificationPolicy
    {
        static const char* SupportName(QuestRuntimeSupport support)
        {
            switch (support)
            {
                case QuestRuntimeSupport::KnownExecutable: return "KnownExecutable";
                case QuestRuntimeSupport::KnownSemanticButUnsupported: return "KnownSemanticButUnsupported";
                case QuestRuntimeSupport::Ambiguous: return "Ambiguous";
                default: return "MissingData";
            }
        }

        static bool UsableDestination(const ObjectiveDestination& d)
        {
            return d.valid && std::isfinite(d.x) && std::isfinite(d.y) &&
                std::isfinite(d.z) && std::isfinite(d.arrivalDistance) && d.arrivalDistance > 0;
        }

        static QuestClassification ClassifyStep(const QuestProfile& base, int index)
        {
            const auto p = MaterializeObjectiveStep(base, index);
            QuestClassification result;
            result.semantic = p.objective.type;
            result.executor = QuestObjectiveDispatchPolicy::Executor(p);
            if (p.semanticAmbiguous)
            {
                result.support = QuestRuntimeSupport::Ambiguous;
                result.reason = "unresolved_semantic_evidence";
                return result;
            }
            if (p.questId <= 0 || !p.title || !*p.title || result.semantic == QuestObjectiveType::Unknown)
                return result;
            if (p.databaseDerived && !p.handwrittenOverride && p.sourceMetadata &&
                (p.sourceMetadata->method == 1 || p.giverIsGameObject || p.turnInIsGameObject))
            {
                result.support = QuestRuntimeSupport::KnownSemanticButUnsupported;
                result.reason = "disabled_or_gameobject_quest_dialogue";
                return result;
            }
            if (p.databaseDerived && !p.handwrittenOverride && p.sourceMetadata &&
                p.sourceMetadata->specialFlags && (*p.sourceMetadata->specialFlags & 2) &&
                result.semantic != QuestObjectiveType::ExploreOrAreaTrigger)
            {
                result.support = QuestRuntimeSupport::KnownSemanticButUnsupported;
                result.reason = "additional_scripted_event_not_modeled";
                return result;
            }
            // The legacy exporter labels an empty objective list TravelReport.
            // This pack omits event/area-trigger flags, so absence of rows is
            // not proof of a report-only mechanic. Confirmed live completion
            // can still authorize turn-in in the eligibility policy.
            if (p.databaseDerived && !p.handwrittenOverride &&
                result.semantic == QuestObjectiveType::TravelReport && base.objectives.empty() &&
                !QuestSourceSemanticPolicy::PlainReport(p))
            {
                result.support = QuestRuntimeSupport::Ambiguous;
                result.reason = "empty_rows_do_not_prove_report_semantic";
                return result;
            }
            result.confidence = p.databaseDerived && !p.handwrittenOverride
                ? QuestClassificationConfidence::StructuredData : QuestClassificationConfidence::ExplicitProfile;
            if (result.executor == QuestExecutorKind::None)
            {
                result.support = QuestRuntimeSupport::KnownSemanticButUnsupported;
                result.reason = "no_executor";
                return result;
            }
            if (result.semantic == QuestObjectiveType::TalkToNpc && result.executor != QuestExecutorKind::TurnIn &&
                p.objective.targetEntry && p.gossipCreditText.empty())
            {
                result.support = QuestRuntimeSupport::KnownSemanticButUnsupported;
                result.reason = "no_proven_generic_gossip_credit";
                return result;
            }
            if (result.semantic == QuestObjectiveType::ExploreOrAreaTrigger &&
                p.areaTriggerIds.empty() && p.areaTriggers.empty())
            {
                result.support = QuestRuntimeSupport::KnownSemanticButUnsupported;
                result.reason = "event_has_no_direct_area_trigger_model";
                return result;
            }
            if (!QuestObjectiveDispatchPolicy::SupportsMaterialized(p))
            {
                result.reason = "missing_executor_metadata";
                return result;
            }
            if (result.semantic == QuestObjectiveType::CollectWorldItem && !p.objective.itemId)
            {
                result.reason = "missing_collection_item_identity";
                return result;
            }
            // Supports() can operate on a visible GO/unit without a static
            // seed. Static auto-classification cannot promise it is visible.
            if (result.executor != QuestExecutorKind::TurnIn &&
                !UsableDestination(p.destination))
            {
                result.reason = "missing_usable_search_location";
                return result;
            }
            if (!p.automatable)
            {
                result.reason = "catalogue_metadata_incomplete";
                return result;
            }
            result.support = QuestRuntimeSupport::KnownExecutable;
            result.reason = "dispatchable_metadata";
            return result;
        }

        static QuestClassification Classify(const QuestProfile& p)
        {
            if (p.objectives.empty()) return ClassifyStep(p, -1);
            auto result = ClassifyStep(p, 0);
            for (std::size_t i = 0; i < p.objectives.size(); ++i)
            {
                auto step = ClassifyStep(p, static_cast<int>(i));
                if (step.support != QuestRuntimeSupport::KnownExecutable) return step;
            }
            return result;
        }
    };
}
