#pragma once

#include "QuestPlannerTypes.h"
#include <string>

namespace Bot
{
    // Execution mechanics and acquisition restrictions have separate authority.
    // Explicit authored mechanics survive; enriched structured restrictions win.
    struct QuestProfileMergePolicy
    {
        static void EnrichRestrictions(QuestProfile& p, const QuestProfile& source)
        {
            // N records prove the enriched source schema was loaded. Absence
            // never becomes an invented unrestricted value.
            if (!source.sourceMetadata) return;
            const auto number = [&](const char* field, auto& value, auto data) {
                if (value != data)
                    p.metadataConflicts.push_back({field, std::to_string(value),
                        std::to_string(data), "structured_acquisition_restriction"});
                value = data;
            };
            const auto optional = [&](const char* field, auto& value, const auto& data) {
                if (!data) return;
                if (value != data)
                    p.metadataConflicts.push_back({field, value ? std::to_string(*value) : "unknown",
                        std::to_string(*data), "structured_acquisition_restriction"});
                value = data;
            };
            number("minimumLevel", p.minimumLevel, source.minimumLevel);
            number("questLevel", p.questLevel, source.questLevel);
            number("prerequisite", p.previousQuestId, source.previousQuestId);
            number("nextInChain", p.nextInChainQuestId, source.nextInChainQuestId);
            optional("requiredRace", p.requiredRaceMask, source.requiredRaceMask);
            optional("requiredClass", p.requiredClassMask, source.requiredClassMask);
            optional("requiredCondition", p.requiredCondition, source.requiredCondition);
            optional("maximumLevel", p.maximumLevel, source.maximumLevel);
            optional("exclusiveGroup", p.exclusiveGroup, source.exclusiveGroup);
            optional("nextQuest", p.nextQuestId, source.nextQuestId);
            p.zoneOrSort = source.zoneOrSort;
            p.sourceMetadata = source.sourceMetadata;
            p.sourceObjectiveCount = source.sourceObjectiveCount;
            p.relationshipMetadataKnown = source.relationshipMetadataKnown;
            p.prerequisiteAlternatives = source.prerequisiteAlternatives;
            p.exclusivePeers = source.exclusivePeers;
            p.areaTriggerIds = source.areaTriggerIds;
            // Preserve explicit actor identities/anchors. A conflicting source
            // relation cannot authorize pickup using an incompatible anchor.
            if ((p.giverEntry && source.giverEntry && p.giverEntry != source.giverEntry) ||
                (p.turnInEntry && source.turnInEntry && p.turnInEntry != source.turnInEntry))
                p.semanticAmbiguous = true;
            if (p.giverEntry == source.giverEntry) p.giverIsGameObject = source.giverIsGameObject;
            if (p.turnInEntry == source.turnInEntry) p.turnInIsGameObject = source.turnInIsGameObject;
        }

        static QuestProfile Merge(const QuestProfile& manual,
            const QuestProfile& generated, bool authoritative)
        {
            if (manual.questId != generated.questId)
            {
                auto result = manual;
                result.semanticAmbiguous = true;
                return result;
            }
            QuestProfile result = authoritative ? manual : generated;
            const auto conflict = [&](const char* field, int hand, int data) {
                if (hand && data && hand != data)
                    result.metadataConflicts.push_back({field, std::to_string(hand),
                        std::to_string(data), authoritative
                            ? (generated.sourceMetadata && (std::string(field)=="giverEntry" ||
                                std::string(field)=="turnInEntry") ? "actor_conflict_fail_closed" : "handwritten_override")
                            : "generated_over_legacy_sketch"});
            };
            // Type zero is a real semantic, not an absent value.
            if (manual.objective.type != QuestObjectiveType::Unknown &&
                generated.objective.type != QuestObjectiveType::Unknown &&
                manual.objective.type != generated.objective.type)
                result.metadataConflicts.push_back({"objectiveType",
                    std::to_string(static_cast<int>(manual.objective.type)),
                    std::to_string(static_cast<int>(generated.objective.type)),
                    authoritative ? "handwritten_override" : "generated_over_legacy_sketch"});
            conflict("targetEntry", manual.objective.targetEntry, generated.objective.targetEntry);
            conflict("objectEntry", manual.objective.objectEntry, generated.objective.objectEntry);
            conflict("itemId", manual.objective.itemId, generated.objective.itemId);
            conflict("giverEntry", manual.giverEntry, generated.giverEntry);
            conflict("turnInEntry", manual.turnInEntry, generated.turnInEntry);
            if (!generated.sourceMetadata)
                conflict("prerequisite", manual.previousQuestId, generated.previousQuestId);
            if (!authoritative) return result;
            result.handwrittenOverride = true;
            EnrichRestrictions(result, generated);
            if (!result.giverEntry) result.giverEntry = generated.giverEntry;
            if (!result.turnInEntry) result.turnInEntry = generated.turnInEntry;
            if (!result.giverDestination.valid && result.giverEntry == generated.giverEntry)
            {
                result.giverDestination = generated.giverDestination;
                result.giverDestinations = generated.giverDestinations;
            }
            if (!result.turnInDestination.valid && result.turnInEntry == generated.turnInEntry)
            {
                result.turnInDestination = generated.turnInDestination;
                result.turnInDestinations = generated.turnInDestinations;
            }
            if (!result.previousQuestId) result.previousQuestId = generated.previousQuestId;
            if (!result.nextInChainQuestId) result.nextInChainQuestId = generated.nextInChainQuestId;
            if (!result.questLevel) result.questLevel = generated.questLevel;
            if (!result.requiredRaceMask) result.requiredRaceMask = generated.requiredRaceMask;
            if (!result.requiredClassMask) result.requiredClassMask = generated.requiredClassMask;
            if (!result.requiredFactionMask) result.requiredFactionMask = generated.requiredFactionMask;
            if (!result.maximumLevel) result.maximumLevel = generated.maximumLevel;
            if (!result.zoneOrSort) result.zoneOrSort = generated.zoneOrSort;
            if (!result.requiredCondition) result.requiredCondition = generated.requiredCondition;
            if (!result.exclusiveGroup) result.exclusiveGroup = generated.exclusiveGroup;
            if (!result.nextQuestId) result.nextQuestId = generated.nextQuestId;
            if (!result.sourceMetadata) result.sourceMetadata = generated.sourceMetadata;
            if (!result.relationshipMetadataKnown &&
                (!manual.previousQuestId || manual.previousQuestId == generated.previousQuestId))
            {
                result.relationshipMetadataKnown = generated.relationshipMetadataKnown;
                result.prerequisiteAlternatives = generated.prerequisiteAlternatives;
                result.exclusivePeers = generated.exclusivePeers;
            }
            const bool sameObjective = manual.objective.type == generated.objective.type &&
                manual.objective.targetEntry == generated.objective.targetEntry &&
                manual.objective.itemId == generated.objective.itemId &&
                manual.objective.objectEntry == generated.objective.objectEntry;
            if (sameObjective && !generated.semanticAmbiguous)
            {
                if (!result.destination.valid) result.destination = generated.destination;
                if (result.searchDestinations.empty()) result.searchDestinations = generated.searchDestinations;
                if (result.objectives.empty() && generated.objectives.size() == 1)
                {
                    result.objectives = generated.objectives;
                    result.objectives.front().objective = result.objective;
                    result.objectives.front().destination = result.destination;
                    result.objectives.front().searchDestinations = result.searchDestinations;
                }
            }
            return result;
        }
    };
}
