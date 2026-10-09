#pragma once
#include "ServiceHubSelectionPolicy.h"
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Bot
{
    class ServiceHubCatalogue
    {
        struct Actor { unsigned faction=0, flags=0; };
        struct Faction { unsigned id=0, our=0, friendly=0, hostile=0, enemies[4]{}; };
        struct Spawn { unsigned entry=0, guid=0, map=0; float x=0,y=0,z=0; };
        std::map<unsigned, Actor> actors_;
        std::map<unsigned, Faction> factions_;
        std::vector<Spawn> spawns_;
        bool Friendly(unsigned actor, unsigned player) const
        {
            auto a=factions_.find(actor), b=factions_.find(player);
            if(a==factions_.end() || b==factions_.end()) return false;
            for(auto e:a->second.enemies) if(e && e==b->second.id) return false;
            for(auto e:b->second.enemies) if(e && e==a->second.id) return false;
            if((a->second.hostile & b->second.our) || (b->second.hostile & a->second.our)) return false;
            return (a->second.friendly & b->second.our) || (b->second.friendly & a->second.our);
        }
    public:
        std::size_t ActorCount() const { return actors_.size(); }
        std::size_t SpawnCount() const { return spawns_.size(); }
        bool Load(std::istream& input)
        {
            actors_.clear(); factions_.clear(); spawns_.clear(); std::string line;
            if(!std::getline(input,line) || line!="# SERVICE_HUBS_V1\tbuild=5875\tpatch=10") return false;
            try
            {
                while(std::getline(input,line))
                {
                    if(line.empty()) continue;
                    std::vector<std::string> f; std::istringstream row(line); std::string part;
                    while(std::getline(row,part,'\t')) f.push_back(part);
                    const auto n=[&](unsigned i) { std::size_t p=0; auto v=std::stoul(f.at(i),&p);
                        if(p!=f.at(i).size() || v>0xffffffffu || f.at(i).front()=='-') throw 0;
                        return static_cast<unsigned>(v); };
                    if(f[0]=="N" && f.size()==4)
                    {
                        auto entry=n(1); Actor a{n(2),n(3)};
                        if(!entry || !(a.flags & 4u) || !actors_.emplace(entry,a).second) throw 0;
                    }
                    else if(f[0]=="F" && f.size()==10)
                    {
                        Faction a{n(2),n(3),n(4),n(5),{n(6),n(7),n(8),n(9)}};
                        if(!factions_.emplace(n(1),a).second) throw 0;
                    }
                    else if(f[0]=="P" && f.size()==7)
                    {
                        const auto position=[&](unsigned i) { std::size_t p=0; float v=std::stof(f.at(i),&p);
                            if(p!=f.at(i).size() || !std::isfinite(v)) throw 0;
                            return v; };
                        Spawn s{n(1),n(2),n(3),position(4),position(5),position(6)};
                        if(!s.guid) throw 0;
                        spawns_.push_back(s);
                    }
                    else throw 0;
                }
                for(const auto& s:spawns_)
                    if(!actors_.contains(s.entry) || !factions_.contains(actors_.at(s.entry).faction)) throw 0;
            }
            catch(...) { actors_.clear(); factions_.clear(); spawns_.clear(); return false; }
            return !spawns_.empty();
        }
        // Entry-keyed registry: choose its nearest source spawn deterministically.
        std::vector<ServiceHubCandidate> Candidates(unsigned map, unsigned playerFaction,
            const ServiceSelectionOrigin& origin, bool requireRepair) const
        {
            std::map<unsigned, std::pair<unsigned, ServiceHubCandidate>> best;
            for(const auto& s:spawns_)
            {
                const auto& actor=actors_.at(s.entry);
                if(s.map!=map || !Friendly(actor.faction,playerFaction) ||
                    (requireRepair && !(actor.flags & 0x4000u))) continue;
                ServiceHubCandidate hub;
                hub.entry=s.entry; hub.source=ServiceHubSource::SourceBacked;
                hub.x=s.x; hub.y=s.y; hub.z=s.z; hub.positionKnown=true;
                // Static flags never claim sell/repair Available.
                hub.euclideanDistance=ServiceHubSelectionPolicy::DistanceFrom(origin,hub);
                auto it=best.find(s.entry);
                if(it==best.end() || hub.euclideanDistance<it->second.second.euclideanDistance ||
                    (hub.euclideanDistance==it->second.second.euclideanDistance && s.guid<it->second.first))
                    best[s.entry]={s.guid,hub};
            }
            std::vector<ServiceHubCandidate> result;
            for(const auto& [entry,value]:best) { (void)entry; result.push_back(value.second); }
            return result;
        }
        bool LoadDefault()
        {
            std::vector<std::string> paths;
            if(const char* path=std::getenv("WOW_INTERNAL_VENDOR_CATALOG")) paths.emplace_back(path);
            else
            {
                for(const char* prefix:{"","../","../../"})
                    paths.push_back(std::string(prefix)+"data/questdb/runtime/service_hubs.tsv");
#ifdef _WIN32
                static const int anchor=0; HMODULE module=nullptr; char modulePath[MAX_PATH]{};
                if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(&anchor),&module) && GetModuleFileNameA(module,modulePath,MAX_PATH))
                {
                    std::string p(modulePath); p=p.substr(0,p.find_last_of("/\\")); p=p.substr(0,p.find_last_of("/\\"));
                    paths.push_back(p+"/data/questdb/runtime/service_hubs.tsv");
                }
#endif
            }
            for(const auto& path:paths) { std::ifstream in(path); if(in && Load(in)) return true; }
            return false;
        }
        static const ServiceHubCatalogue& Instance()
        {
            static const ServiceHubCatalogue catalogue=[] { ServiceHubCatalogue c; c.LoadDefault(); return c; }();
            return catalogue;
        }
    };
}
