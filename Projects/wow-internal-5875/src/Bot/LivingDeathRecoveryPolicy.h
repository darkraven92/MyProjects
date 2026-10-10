#pragma once

#include <cmath>
#include <cstdint>

namespace Bot
{
    // Disengagement and displacement, never an assertion of hostile-free space.
    class LivingDeathRecoveryPolicy
    {
    public:
        struct Point { float x=0, y=0, z=0; };
        enum class State { Idle, Waiting, Planning, Routing, Settling, Complete, Blocked };
        enum class Action { None, Hold, Water, Defense, Plan, Navigate, Recover, Release, Block };
        struct Sample
        {
            std::uint64_t now=0, guid=0;
            std::uint32_t map=0;
            bool identity=false, alive=false, evidenceKnown=false;
            bool combat=false, aggressor=false, defense=false, water=false;
            bool movementKnown=false, swimming=false;
            std::uint32_t hp=0, maxHp=0;
            Point position{};
        };
        static constexpr std::uint64_t DeadlineMs=90000, QuietMs=2000, ProbeSpacingMs=250;
        static constexpr unsigned MaximumCandidates=4, MaximumReplans=4;
        static constexpr float CandidateDistance=18.0f, MinimumDisplacement=12.0f;
        // Engineering displacement bounds; neither is an aggro/safety radius.
        static float Distance(Point a, Point b)
        { return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z); }
        static bool Finite(Point p)
        { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
        bool Begin(bool automatic, std::uint64_t guid, std::uint32_t map, Point p, std::uint64_t now)
        {
            *this={};
            if (!automatic) return false;
            guid_=guid; map_=map; origin_=p; began_=lastNow_=quietSince_=now;
            state_=State::Waiting;
            if (!guid || !Finite(p)) Block("invalid_resurrection_identity_position");
            return true;
        }
        void Reset() { *this={}; }
        bool Owns() const { return state_!=State::Idle && state_!=State::Complete; }
        State Current() const { return state_; }
        const char* Reason() const { return reason_; }
        Point Origin() const { return origin_; }
        unsigned Attempts() const { return attempts_; }
        std::uint32_t LastHealth() const { return lastHp_; }
        bool SamePlayer(std::uint64_t guid) const { return guid && guid==guid_; }
        bool Complete()
        {
            if (state_!=State::Settling || proofs_<3) return false;
            state_=State::Complete;
            return true;
        }
        void Block(const char* reason)
        {
            if (!Owns() || state_==State::Blocked) return;
            state_=State::Blocked; reason_=reason; proofs_=0;
        }
        void WorldGap() { Block("world_gap_manual_recovery"); }
        void ObserveDeadline(std::uint64_t now)
        {
            if (Owns() && (now<lastNow_ || now<began_ || now-began_>=DeadlineMs))
                Block("living_recovery_deadline");
            lastNow_=now;
        }
        void Interrupt()
        {
            if (state_==State::Planning || state_==State::Routing)
                state_=State::Waiting;
            proofs_=0;
        }
        void RouteFailed()
        {
            if (!Owns() || state_==State::Blocked) return;
            state_=State::Waiting;
            if (attempts_>=MaximumCandidates) Block("egress_candidates_exhausted");
        }
        bool PlanReady(bool reached, Point projected)
        {
            if (state_!=State::Planning) return false;
            if (!reached || !Finite(projected) || Distance(origin_,projected)<MinimumDisplacement)
            { RouteFailed(); return false; }
            state_=State::Routing; return true;
        }
        void Arrived(Point actual)
        {
            if (state_!=State::Routing) return;
            if (!Finite(actual) || Distance(origin_,actual)<MinimumDisplacement)
            { RouteFailed(); return; }
            state_=State::Settling; proofs_=0;
        }
        // Every fresh read, including a command guard, participates in pressure
        // history. It cannot spend route slots or advance completion proofs.
        bool Observe(const Sample& s)
        {
            if (!Owns()) return false;
            ObserveDeadline(s.now);
            if (!s.identity || s.guid!=guid_ || s.map!=map_)
            { Block("living_identity_lost"); return false; }
            if (s.hp && lastHp_ && s.hp<lastHp_) quietSince_=s.now;
            if (s.hp) lastHp_=s.hp;
            if (!s.alive || s.water || s.swimming || s.combat || s.aggressor || s.defense ||
                !s.evidenceKnown || !s.movementKnown || !Finite(s.position))
            { Interrupt(); quietSince_=s.now; return false; }
            if (s.now<quietSince_ || s.now-quietSince_<QuietMs)
            { Interrupt(); return false; }
            return state_!=State::Blocked;
        }
        Action Update(const Sample& s)
        {
            if (!Owns()) return Action::None;
            const bool quiet=Observe(s);
            if (!s.identity || s.guid!=guid_ || s.map!=map_) return Action::Block;
            if (!s.alive) return state_==State::Blocked ? Action::Block : Action::Hold;
            if (s.water || s.swimming) return Action::Water;
            if (s.combat || s.aggressor || s.defense) return Action::Defense;
            if (state_==State::Blocked) return Action::Block;
            if (!quiet) return Action::Hold;
            if (state_==State::Waiting)
            {
                if (attempts_>=MaximumCandidates)
                { Block("egress_candidates_exhausted"); return Action::Block; }
                ++attempts_; state_=State::Planning; return Action::Plan;
            }
            if (state_==State::Planning || state_==State::Routing) return Action::Navigate;
            if (state_==State::Settling)
            {
                if (Distance(origin_,s.position)<MinimumDisplacement)
                { Block("egress_displacement_lost"); return Action::Block; }
                if (!s.maxHp || double(s.hp)/s.maxHp<0.95)
                { proofs_=0; return Action::Recover; }
                if (!proofs_ || s.now-lastProof_>=ProbeSpacingMs)
                { ++proofs_; lastProof_=s.now; }
                if (proofs_>=3)
                    return Action::Release; // owner validates dispatch/world before committing
                return Action::Recover;
            }
            return Action::Hold;
        }
    private:
        State state_=State::Idle;
        std::uint64_t guid_=0, began_=0, lastNow_=0, quietSince_=0, lastProof_=0;
        std::uint32_t map_=0, lastHp_=0;
        Point origin_{};
        unsigned attempts_=0, proofs_=0;
        const char* reason_="none";
    };
}
