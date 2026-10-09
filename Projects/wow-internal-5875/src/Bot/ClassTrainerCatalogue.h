#pragma once
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Bot
{
    struct TrainerService
    {
        unsigned entry=0, classId=0, service=0, spell=0, chain=0, rank=0, previous=0, required=0, level=0, cost=0;
        std::string name, rankText;
    };
    struct TrainerSpellIdentity { unsigned id=0, chain=0, rank=0; std::string name, rankText; };
    struct TrainerSpawn { unsigned entry=0, guid=0, map=0; float x=0,y=0,z=0; };
    struct TrainerActor { std::string name; unsigned faction=0; };
    struct TrainerFaction { unsigned faction=0, our=0, friendly=0, hostile=0; unsigned enemies[4]{}; };
    class ClassTrainerCatalogue
    {
    public:
        std::vector<TrainerService> services;
        std::vector<TrainerSpawn> spawns;
        std::map<unsigned, TrainerSpellIdentity> spells;
        std::map<unsigned, TrainerActor> actors;
        std::map<unsigned, TrainerFaction> factions;
        bool Friendly(unsigned npc, unsigned player) const
        {
            const auto a=factions.find(npc), b=factions.find(player);
            if(a==factions.end() || b==factions.end()) return false;
            for(auto enemy:a->second.enemies) if(enemy && enemy==b->second.faction) return false;
            for(auto enemy:b->second.enemies) if(enemy && enemy==a->second.faction) return false;
            if((a->second.hostile & b->second.our) || (b->second.hostile & a->second.our)) return false;
            return (a->second.friendly & b->second.our) || (b->second.friendly & a->second.our);
        }
        bool Load(std::istream& in)
        {
            services.clear(); spawns.clear(); spells.clear(); actors.clear(); factions.clear();
            std::string line;
            if (!std::getline(in,line) || line!="# CLASS_TRAINERS_V1\tbuild=5875\tpatch=10") return false;
            try
            {
                while (std::getline(in,line))
                {
                    if (line.empty()) continue;
                    std::vector<std::string> f; std::istringstream row(line); std::string part;
                    while (std::getline(row,part,'\t')) f.push_back(part);
                    const auto n=[&](std::size_t i) { std::size_t p=0; auto v=std::stoul(f.at(i),&p);
                        if(p!=f.at(i).size() || v>0xffffffffu || f.at(i).front()=='-') throw 0;
                        return static_cast<unsigned>(v); };
                    if(f[0]=="N" && f.size()==4)
                    {
                        if(!actors.emplace(n(1),TrainerActor{f[2],n(3)}).second) throw 0;
                    }
                    else if(f[0]=="F" && f.size()==10)
                    {
                        TrainerFaction a{n(2),n(3),n(4),n(5),{n(6),n(7),n(8),n(9)}};
                        if(!factions.emplace(n(1),a).second) throw 0;
                    }
                    else if(f[0]=="T" && f.size()==13)
                    {
                        TrainerService s; s.entry=n(1); s.classId=n(2); s.service=n(3); s.spell=n(4);
                        s.name=f[5]; s.rankText=f[6]; s.chain=n(7); s.rank=n(8); s.previous=n(9);
                        s.required=n(10); s.level=n(11); s.cost=n(12);
                        if(!s.entry || !s.spell || s.name.empty() || s.classId>11 || !s.classId) throw 0;
                        services.push_back(s);
                    }
                    else if(f[0]=="S" && f.size()==6)
                    {
                        TrainerSpellIdentity s{n(1),n(4),n(5),f[2],f[3]};
                        if(!s.id || s.name.empty() || spells.contains(s.id)) throw 0;
                        spells.emplace(s.id,s);
                    }
                    else if(f[0]=="P" && f.size()==7)
                    {
                        TrainerSpawn s{n(1),n(2),n(3),std::stof(f[4]),std::stof(f[5]),std::stof(f[6])};
                        if(!std::isfinite(s.x)||!std::isfinite(s.y)||!std::isfinite(s.z)) throw 0;
                        spawns.push_back(s);
                    }
                    else throw 0;
                }
                for(const auto& s:services)
                    if(!actors.contains(s.entry) || !spells.contains(s.spell) || (s.previous && !spells.contains(s.previous)) ||
                       (s.required && !spells.contains(s.required))) throw 0;
            }
            catch(...) { services.clear(); spawns.clear(); spells.clear(); actors.clear(); factions.clear(); return false; }
            return !services.empty() && !spawns.empty();
        }
        bool LoadDefault()
        {
            std::vector<std::string> paths;
            if(const char* p=std::getenv("WOW_INTERNAL_TRAINER_CATALOG")) paths.emplace_back(p);
            else
            {
                for(const char* prefix:{"", "../", "../../"})
                    paths.push_back(std::string(prefix)+"data/questdb/runtime/class_trainers.tsv");
#ifdef _WIN32
                static const int anchor=0; HMODULE module=nullptr; char path[MAX_PATH]{};
                if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(&anchor),&module) && GetModuleFileNameA(module,path,MAX_PATH))
                {
                    std::string p(path); p=p.substr(0,p.find_last_of("/\\"));
                    p=p.substr(0,p.find_last_of("/\\"));
                    paths.push_back(p+"/data/questdb/runtime/class_trainers.tsv");
                }
#endif
            }
            for(const auto& p:paths) { std::ifstream in(p); if(in && Load(in)) return true; }
            return false;
        }
    };
}
