#pragma once

#include "QuestPlannerTypes.h"
#include "ObjectiveAnchorSelectionPolicy.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Bot
{
    /*
     * Runtime bridge for the offline VMaNGOS -> SQLite -> TSV pipeline.
     *
     * Phase 13A keeps the old Q/G/T/D/S records readable and extends the
     * catalogue with O/OS records for every Vanilla leaderboard objective.
     * The injected DLL still has no sqlite3 dependency.
     */
    class VanillaQuestDatabase
    {
    private:
        struct OwnedProfile
        {
            QuestProfile profile{};
            std::string title{};
            std::string targetName{};
            std::string supportNote{};
            std::deque<std::string> objectiveTargetNames{};
            bool graphMetadataRead = false;
            bool enrichmentOnly = false;
        };

        bool attempted_ = false;
        bool loaded_ = false;
        std::string loadedPath_{};
        std::string lastError_{};
        std::vector<std::unique_ptr<OwnedProfile>> owned_{};
        std::vector<QuestProfile> profiles_{};
        std::unordered_map<int, OwnedProfile*> byQuestId_{};

        static std::string DirectoryName(const std::string& value)
        {
            const auto pos = value.find_last_of("/\\");
            return pos == std::string::npos ? std::string{} : value.substr(0, pos);
        }

        static std::vector<std::string> CandidatePaths()
        {
            std::vector<std::string> paths;
            if (const char* explicitPath = std::getenv("WOW_INTERNAL_QUESTDB_CATALOG"))
            {
                if (explicitPath[0] != '\0')
                    paths.emplace_back(explicitPath);
            }

            // Phase 13B prefers the complete Durotar zone pack. Keep the
            // historic Valley filename as a compatibility fallback for older
            // installs and diagnostics.
            paths.emplace_back("data/questdb/runtime/early_horde.tsv");
            paths.emplace_back("../data/questdb/runtime/early_horde.tsv");
            paths.emplace_back("../../data/questdb/runtime/early_horde.tsv");
            paths.emplace_back("data/questdb/runtime/durotar.tsv");
            paths.emplace_back("../data/questdb/runtime/durotar.tsv");
            paths.emplace_back("../../data/questdb/runtime/durotar.tsv");
            paths.emplace_back("data/questdb/runtime/valley_of_trials.tsv");
            paths.emplace_back("../data/questdb/runtime/valley_of_trials.tsv");
            paths.emplace_back("../../data/questdb/runtime/valley_of_trials.tsv");

#ifdef _WIN32
            static const int moduleAnchor = 0;
            HMODULE module = nullptr;
            if (GetModuleHandleExA(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(&moduleAnchor),
                    &module) &&
                module != nullptr)
            {
                char path[MAX_PATH]{};
                const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
                if (length > 0 && length < MAX_PATH)
                {
                    const std::string modulePath(path, length);
                    const std::string buildDir = DirectoryName(modulePath);
                    const std::string projectDir = DirectoryName(buildDir);
                    if (!projectDir.empty())
                    {
                        paths.push_back(projectDir + "\\data\\questdb\\runtime\\early_horde.tsv");
                        paths.push_back(
                            projectDir +
                            "\\data\\questdb\\runtime\\durotar.tsv");
                        paths.push_back(
                            projectDir +
                            "\\data\\questdb\\runtime\\valley_of_trials.tsv");
                    }
                }
            }
#endif
            return paths;
        }

        static std::vector<std::string> SplitTabs(const std::string& line)
        {
            std::vector<std::string> out;
            std::size_t start = 0;
            while (true)
            {
                const auto pos = line.find('\t', start);
                if (pos == std::string::npos)
                {
                    out.push_back(line.substr(start));
                    break;
                }
                out.push_back(line.substr(start, pos - start));
                start = pos + 1;
            }
            return out;
        }

        static std::string Unescape(const std::string& value)
        {
            std::string out;
            out.reserve(value.size());
            for (std::size_t i = 0; i < value.size(); ++i)
            {
                if (value[i] != '\\' || i + 1 >= value.size())
                {
                    out.push_back(value[i]);
                    continue;
                }

                const char next = value[++i];
                switch (next)
                {
                    case 't': out.push_back('\t'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case '\\': out.push_back('\\'); break;
                    default: out.push_back(next); break;
                }
            }
            return out;
        }

        static int ParseInt(const std::string& value, int fallback = 0)
        {
            char* end = nullptr;
            const long parsed = std::strtol(value.c_str(), &end, 10);
            return end != value.c_str() ? static_cast<int>(parsed) : fallback;
        }

        static float ParseFloat(const std::string& value, float fallback = 0.0f)
        {
            char* end = nullptr;
            const float parsed = std::strtof(value.c_str(), &end);
            return end != value.c_str() ? parsed : fallback;
        }

        static QuestObjectiveType ParseObjectiveType(const std::string& value)
        {
            if (value == "TalkToNpc") return QuestObjectiveType::TalkToNpc;
            if (value == "KillMob") return QuestObjectiveType::KillMob;
            if (value == "CollectItemFromMob") return QuestObjectiveType::CollectItemFromMob;
            if (value == "CollectWorldItem") return QuestObjectiveType::CollectWorldItem;
            if (value == "UseItemOnUnit") return QuestObjectiveType::UseItemOnUnit;
            if (value == "ExploreOrAreaTrigger") return QuestObjectiveType::ExploreOrAreaTrigger;
            if (value == "InteractGameObject") return QuestObjectiveType::InteractGameObject;
            if (value == "TravelReport") return QuestObjectiveType::TravelReport;
            return QuestObjectiveType::Unknown;
        }

        static QuestRouteGroup ParseRouteGroup(const std::string& value)
        {
            if (value == "ValleyExterior") return QuestRouteGroup::ValleyExterior;
            if (value == "BurningBladeCoven") return QuestRouteGroup::BurningBladeCoven;
            if (value == "SenjinRoad") return QuestRouteGroup::SenjinRoad;
            if (value == "SenjinVillage") return QuestRouteGroup::SenjinVillage;
            if (value == "SenjinCoast") return QuestRouteGroup::SenjinCoast;
            if (value == "EchoIsles") return QuestRouteGroup::EchoIsles;
            if (value == "RazorHillRoad") return QuestRouteGroup::RazorHillRoad;
            return QuestRouteGroup::None;
        }

        static ObjectiveDestination DestinationFrom(
            const std::vector<std::string>& fields,
            std::size_t mapField,
            const char* label)
        {
            ObjectiveDestination result{};
            if (fields.size() <= mapField + 4)
                return result;

            result.valid = true;
            result.mapId = static_cast<std::uint32_t>(
                std::max(0, ParseInt(fields[mapField], 1)));
            result.x = ParseFloat(fields[mapField + 1]);
            result.y = ParseFloat(fields[mapField + 2]);
            result.z = ParseFloat(fields[mapField + 3]);
            result.arrivalDistance = ParseFloat(fields[mapField + 4], 8.0f);
            result.label = label;
            return result;
        }

        bool LoadPath(const std::string& path)
        {
            std::ifstream input(path);
            if (!input)
                return false;

            owned_.clear();
            profiles_.clear();
            byQuestId_.clear();

            std::string line;
            int lineNumber = 0;
            while (std::getline(input, line))
            {
                ++lineNumber;
                if (line.empty() || line[0] == '#')
                    continue;

                auto fields = SplitTabs(line);
                for (auto& field : fields)
                    field = Unescape(field);
                if (fields.empty())
                    continue;

                if (fields[0] == "Q")
                {
                    if (fields.size() < 18)
                    {
                        lastError_ =
                            "runtime catalog Q record has too few fields at line " +
                            std::to_string(lineNumber);
                        return false;
                    }

                    const int questId = ParseInt(fields[1]);
                    if (questId <= 0 || byQuestId_.find(questId) != byQuestId_.end())
                    {
                        lastError_ =
                            "runtime catalog has invalid/duplicate quest id at line " +
                            std::to_string(lineNumber);
                        return false;
                    }

                    auto owned = std::make_unique<OwnedProfile>();
                    owned->title = fields[2];
                    owned->targetName = fields[13];
                    auto& p = owned->profile;
                    p.questId = questId;
                    p.giverEntry = static_cast<std::uint32_t>(
                        std::max(0, ParseInt(fields[5])));
                    p.turnInEntry = static_cast<std::uint32_t>(
                        std::max(0, ParseInt(fields[6])));
                    p.minimumLevel = std::max(1, ParseInt(fields[3], 1));
                    p.priority = ParseInt(fields[15], 800);
                    p.optional = false;
                    p.classToken = "";
                    p.expectedObjectiveCount = ParseInt(fields[7], -1);
                    p.routeGroup = ParseRouteGroup(fields[14]);
                    p.support = QuestExecutionSupport::ProfiledPlannerOnly;
                    p.objective.type = ParseObjectiveType(fields[8]);
                    p.objective.targetEntry = static_cast<std::uint32_t>(
                        std::max(0, ParseInt(fields[9])));
                    p.objective.itemId = static_cast<std::uint32_t>(
                        std::max(0, ParseInt(fields[10])));
                    p.objective.objectEntry = static_cast<std::uint32_t>(
                        std::max(0, ParseInt(fields[11])));
                    p.objective.requiredCount = std::max(1, ParseInt(fields[12], 1));
                    p.databaseDerived = true;
                    p.sourceObjectiveCount = p.expectedObjectiveCount;
                    p.gameObjectType = ParseInt(fields[16], -1);
                    p.gameObjectLootId = static_cast<std::uint32_t>(
                        std::max(0, ParseInt(fields[17])));

                    if (fields.size() >= 19)
                        p.hubUnlockQuestId = std::max(0, ParseInt(fields[18]));
                    if (fields.size() >= 20)
                        p.hubExit = ParseInt(fields[19]) != 0;
                    else
                        p.hubExit = p.routeGroup == QuestRouteGroup::SenjinRoad;

                    // Phase 13B appended fields. Older v1/v2 runtime catalogues
                    // remain readable because every field has a conservative
                    // fallback.
                    if (fields.size() >= 21)
                        p.questLevel = ParseInt(fields[20], 0);
                    if (fields.size() >= 22)
                        p.previousQuestId = ParseInt(fields[21], 0);
                    if (fields.size() >= 23)
                        p.nextInChainQuestId = ParseInt(fields[22], 0);
                    if (fields.size() >= 24)
                        p.breadcrumbForQuestId = ParseInt(fields[23], 0);
                    if (fields.size() >= 25)
                        p.automatable = ParseInt(fields[24], 1) != 0;
                    if (fields.size() >= 26)
                    {
                        owned->supportNote = fields[25];
                        p.supportNote = owned->supportNote.c_str();
                    }

                    // Phase 13C.2.1 appends a planner-only late-wave flag.
                    // Reuse QuestProfile::optional as the generic marker: a
                    // late-wave quest may be accepted during the hub sweep,
                    // but it does not block turn-in of completed core-wave
                    // quests. Older catalogues omit this field and preserve
                    // the historical false default.
                    if (fields.size() >= 27)
                        p.optional = ParseInt(fields[26], 0) != 0;

                    OwnedProfile* raw = owned.get();
                    owned_.push_back(std::move(owned));
                    byQuestId_[questId] = raw;
                    continue;
                }

                if (fields.size() < 2)
                    continue;

                const int questId = ParseInt(fields[1]);
                const auto it = byQuestId_.find(questId);
                if (it == byQuestId_.end())
                    continue;

                auto* owned = it->second;
                if (fields[0] == "U")
                {
                    if (fields.size()!=3 || fields[2]!="ambiguous_creature_credit")
                    { lastError_="invalid semantic ambiguity record"; return false; }
                    owned->profile.semanticAmbiguous=true;
                    continue;
                }
                if (fields[0] == "A")
                {
                    if (fields.size()!=9 || owned->profile.areaTriggers.size()>=8)
                    { lastError_="invalid area trigger record"; return false; }
                    for (std::size_t i=2;i<fields.size();++i)
                    {
                        if (i<5)
                        {
                            int value=0;
                            const auto& f=fields[i];
                            const auto parsed=std::from_chars(f.data(),f.data()+f.size(),value);
                            if(parsed.ec!=std::errc{} || parsed.ptr!=f.data()+f.size() || value<0)
                            { lastError_="invalid trigger identity"; return false; }
                        }
                        else
                        {
                            char* end=nullptr;
                            float value=std::strtof(fields[i].c_str(),&end);
                            if(end==fields[i].c_str() || *end || !std::isfinite(value))
                            { lastError_="invalid trigger geometry"; return false; }
                        }
                    }
                    QuestAreaTrigger trigger;
                    trigger.id=ParseInt(fields[2]); trigger.build=ParseInt(fields[3]);
                    trigger.center=DestinationFrom(fields,4,"SQL/DBC verified quest trigger");
                    trigger.radius=ParseFloat(fields[8]);
                    trigger.center.arrivalDistance=std::min(3.5f,trigger.radius*0.5f);
                    trigger.clientGeometryVerified=true;
                    owned->profile.areaTriggers.push_back(trigger);
                    if (owned->profile.objective.type==QuestObjectiveType::ExploreOrAreaTrigger)
                    {
                        if (!owned->profile.destination.valid) owned->profile.destination=trigger.center;
                        owned->profile.searchDestinations.push_back(trigger.center);
                        owned->profile.preferNearestObjectiveAnchor=true;
                    }
                    continue;
                }
                if (fields[0] == "GC" && fields.size() == 4)
                {
                    const int index = ParseInt(fields[2], -1);
                    if (index < 0 || static_cast<std::size_t>(index) >= owned->profile.objectives.size())
                    { lastError_ = "invalid gossip objective index"; return false; }
                    owned->profile.objectives[index].gossipCreditText = fields[3];
                    if (index == 0) owned->profile.gossipCreditText = fields[3];
                    continue;
                }

                if (fields[0] == "H" && fields.size() == 3 && fields[2] == "enrichment_only")
                {
                    owned->enrichmentOnly = true;
                    continue;
                }
                if (fields[0] == "N")
                {
                    if (fields.size() != 16 || owned->profile.sourceMetadata)
                    { lastError_ = "invalid enriched metadata record"; return false; }
                    QuestSourceMetadata m;
                    std::optional<int>* values[] = {&m.method, &m.flags, &m.specialFlags, &m.exclusiveGroup,
                        &m.requiredSkill, &m.requiredSkillValue, &m.startScript, &m.completeScript,
                        &m.reputationObjective, &m.timeLimit, &m.rewardOrRequiredMoney, &m.breadcrumb};
                    for (std::size_t i = 0; i < 12; ++i)
                    {
                        if (fields[i + 2] == "-") continue;
                        int value = 0;
                        const auto& f = fields[i + 2];
                        const auto parsed = std::from_chars(f.data(), f.data() + f.size(), value);
                        if (parsed.ec != std::errc{} || parsed.ptr != f.data() + f.size())
                        { lastError_ = "invalid enriched metadata number"; return false; }
                        *values[i] = value;
                    }
                    owned->profile.sourceMetadata = m;
                    owned->profile.exclusiveGroup = m.exclusiveGroup;
                    owned->profile.relationshipMetadataKnown = true;
                    owned->profile.giverIsGameObject = fields[14] == "gameobject";
                    owned->profile.turnInIsGameObject = fields[15] == "gameobject";
                    continue;
                }
                if (fields[0] == "R" && fields.size() >= 4)
                {
                    if (fields[2] == "exclusive")
                        owned->profile.exclusivePeers.push_back(ParseInt(fields[3]));
                    else if (fields[2] == "area")
                        owned->profile.areaTriggerIds.push_back(ParseInt(fields[3]));
                    else if (fields.size() >= 5 && (fields[2]=="active" || fields[2]=="rewarded"))
                    {
                        QuestPrerequisiteClause clause;
                        clause.requiresActive = fields[2] == "active";
                        clause.resolved = fields[3] == "resolved";
                        for (std::size_t i = 4; i < fields.size(); ++i)
                            clause.allOf.push_back(ParseInt(fields[i]));
                        owned->profile.prerequisiteAlternatives.push_back(std::move(clause));
                    }
                    else { lastError_="unknown relationship record"; return false; }
                    continue;
                }

                if (fields[0] == "M")
                {
                    if (fields.size() != 8 || owned->graphMetadataRead)
                    {
                        lastError_ = "invalid quest metadata record";
                        return false;
                    }
                    int values[6]{};
                    for (std::size_t i = 0; i < 6; ++i)
                    {
                        const auto& field = fields[i + 2];
                        const auto parsed = std::from_chars(field.data(), field.data() + field.size(), values[i]);
                        if (parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size())
                        {
                            lastError_ = "invalid numeric quest metadata";
                            return false;
                        }
                    }
                    owned->graphMetadataRead = true;
                    auto& p = owned->profile;
                    p.requiredRaceMask = static_cast<std::uint32_t>(values[0]);
                    p.requiredClassMask = static_cast<std::uint32_t>(values[1]);
                    p.maximumLevel = values[2];
                    p.zoneOrSort = values[3];
                    p.requiredCondition = values[4];
                    p.nextQuestId = values[5];
                    continue;
                }

                if (fields[0] == "O")
                {
                    if (fields.size() < 13)
                        continue;

                    const int objectiveIndex = ParseInt(fields[2], -1);
                    if (objectiveIndex < 0)
                        continue;

                    if (owned->profile.objectives.size() <=
                        static_cast<std::size_t>(objectiveIndex))
                    {
                        owned->profile.objectives.resize(
                            static_cast<std::size_t>(objectiveIndex) + 1);
                    }

                    owned->objectiveTargetNames.push_back(fields[10]);
                    auto& step = owned->profile.objectives[
                        static_cast<std::size_t>(objectiveIndex)];
                    step.slot = ParseInt(fields[3]);
                    step.leaderboardIndex = ParseInt(fields[4], objectiveIndex);
                    step.objective.type = ParseObjectiveType(fields[5]);
                    step.objective.targetEntry = static_cast<std::uint32_t>(
                        std::max(0, ParseInt(fields[6])));
                    step.objective.itemId = static_cast<std::uint32_t>(
                        std::max(0, ParseInt(fields[7])));
                    step.objective.objectEntry = static_cast<std::uint32_t>(
                        std::max(0, ParseInt(fields[8])));
                    step.objective.requiredCount = std::max(1, ParseInt(fields[9], 1));
                    step.objective.targetName = owned->objectiveTargetNames.back().c_str();
                    step.gameObjectType = ParseInt(fields[11], -1);
                    step.gameObjectLootId = static_cast<std::uint32_t>(
                        std::max(0, ParseInt(fields[12])));
                    continue;
                }

                if (fields[0] == "OS")
                {
                    if (fields.size() < 9)
                        continue;
                    const int objectiveIndex = ParseInt(fields[2], -1);
                    if (objectiveIndex < 0 ||
                        static_cast<std::size_t>(objectiveIndex) >=
                            owned->profile.objectives.size())
                    {
                        continue;
                    }

                    auto destination = DestinationFrom(
                        fields,
                        3,
                        "QuestDB objective step spawn");
                    if (!destination.valid)
                        continue;

                    auto& step = owned->profile.objectives[
                        static_cast<std::size_t>(objectiveIndex)];
                    step.searchDestinations.push_back(destination);
                    if (!step.destination.valid)
                        step.destination = destination;
                    continue;
                }

                if (fields[0] == "G" || fields[0] == "T" ||
                    fields[0] == "D" || fields[0] == "S")
                {
                    if (fields.size() < 7)
                        continue;

                    if (fields[0] == "G")
                    {
                        const auto destination = DestinationFrom(
                            fields,
                            2,
                            "QuestDB giver spawn");
                        if(owned->profile.giverDestinations.size()<ObjectiveAnchorSelectionPolicy::MaximumCandidates)
                            owned->profile.giverDestinations.push_back(destination);
                        if(!owned->profile.giverDestination.valid) owned->profile.giverDestination=destination;
                    }
                    else if (fields[0] == "T")
                    {
                        const auto destination = DestinationFrom(
                            fields,
                            2,
                            "QuestDB turn-in spawn");
                        if(owned->profile.turnInDestinations.size()<ObjectiveAnchorSelectionPolicy::MaximumCandidates)
                            owned->profile.turnInDestinations.push_back(destination);
                        if(!owned->profile.turnInDestination.valid) owned->profile.turnInDestination=destination;
                    }
                    else if (fields[0] == "D")
                    {
                        owned->profile.destination = DestinationFrom(
                            fields,
                            2,
                            "QuestDB primary objective seed");
                    }
                    else
                    {
                        auto destination = DestinationFrom(
                            fields,
                            2,
                            "QuestDB objective spawn");
                        if (destination.valid)
                            owned->profile.searchDestinations.push_back(destination);
                    }
                }
            }

            if (owned_.empty())
            {
                lastError_ = "runtime catalog contains no quest records";
                return false;
            }

            profiles_.reserve(owned_.size());
            for (const auto& owned : owned_)
            {
                owned->profile.title = owned->title.c_str();
                owned->profile.objective.targetName = owned->targetName.c_str();

                if (!owned->profile.destination.valid &&
                    !owned->profile.searchDestinations.empty())
                {
                    owned->profile.destination =
                        owned->profile.searchDestinations.front();
                }

                for (auto& step : owned->profile.objectives)
                {
                    if (!step.destination.valid && !step.searchDestinations.empty())
                        step.destination = step.searchDestinations.front();
                }

                if (!owned->enrichmentOnly) profiles_.push_back(owned->profile);
            }

            loadedPath_ = path;
            loaded_ = true;
            lastError_.clear();
            return true;
        }

        bool Load()
        {
            attempted_ = true;
            for (const auto& path : CandidatePaths())
            {
                if (LoadPath(path))
                    return true;
            }
            if (lastError_.empty())
                lastError_ = "data/questdb/runtime/valley_of_trials.tsv was not found";
            return false;
        }

    public:
        static VanillaQuestDatabase& Instance()
        {
            static VanillaQuestDatabase instance;
            return instance;
        }

        bool EnsureLoaded()
        {
            if (!attempted_)
                Load();
            return loaded_;
        }

        const std::vector<QuestProfile>& Profiles()
        {
            EnsureLoaded();
            return profiles_;
        }

        const QuestProfile* SourceProfile(int questId)
        {
            EnsureLoaded();
            const auto found = byQuestId_.find(questId);
            return found == byQuestId_.end() ? nullptr : &found->second->profile;
        }

        bool Loaded()
        {
            EnsureLoaded();
            return loaded_;
        }

        const std::string& LoadedPath()
        {
            EnsureLoaded();
            return loadedPath_;
        }

        const std::string& LastError()
        {
            EnsureLoaded();
            return lastError_;
        }
    };
}
