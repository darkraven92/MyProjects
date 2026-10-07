#pragma once

#include "MovementProgressWatchdog.h"

#include <cmath>
#include <cstdint>

namespace Bot
{
    // Combat chase is target-relative. Lateral displacement is not progress,
    // and CTM/replan dispatch is never an observation. Keep the existing
    // 0.65/0.75-yard and 8/16-tick windows private to this owner.
    class ChaseProgressWatchdog
    {
        MovementProgressWatchdogConfig config_{};
        bool initialized_=false;
        float x_=0, y_=0, z_=0, targetX_=0, targetY_=0, targetZ_=0;
        float distance_=0;
        std::uint64_t lastProgressTick_=0;

        static float Length(float x, float y, float z)
        { return std::sqrt(x*x+y*y+z*z); }

        static bool Valid(float x,float y,float z,float tx,float ty,float tz,float distance)
        {
            return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&
                std::isfinite(tx)&&std::isfinite(ty)&&std::isfinite(tz)&&
                std::isfinite(distance)&&distance>=0;
        }
    public:
        explicit ChaseProgressWatchdog(MovementProgressWatchdogConfig config)
            : config_(config) {}

        void Reset(float x,float y,float z,float tx,float ty,float tz,
            float distance,std::uint64_t tick)
        {
            initialized_=Valid(x,y,z,tx,ty,tz,distance);
            x_=x; y_=y; z_=z; targetX_=tx; targetY_=ty; targetZ_=tz;
            distance_=distance; lastProgressTick_=tick;
        }

        void Suspend(float x,float y,float z,float tx,float ty,float tz,
            float distance,std::uint64_t tick)
        { Reset(x,y,z,tx,ty,tz,distance,tick); }

        MovementProgressObservation Update(float x,float y,float z,
            float tx,float ty,float tz,float distance,std::uint64_t tick,
            bool movementExpected)
        {
            MovementProgressObservation result{};
            if (!Valid(x,y,z,tx,ty,tz,distance)) return result;
            result.valid=true;
            if (!initialized_ || !movementExpected || tick<lastProgressTick_)
            { Reset(x,y,z,tx,ty,tz,distance,tick); return result; }

            const float dx=x-x_, dy=y-y_, dz=z-z_;
            const float bearingX=targetX_-x_, bearingY=targetY_-y_, bearingZ=targetZ_-z_;
            const float bearingLength=Length(bearingX,bearingY,bearingZ);
            result.netMovement=Length(dx,dy,dz);
            result.goalGain=distance_-distance;
            const float toward=bearingLength>0.001f
                ? (dx*bearingX+dy*bearingY+dz*bearingZ)/bearingLength : 0.0f;
            if (result.goalGain>=config_.meaningfulGoalGain ||
                toward>=config_.meaningfulNetMovement)
            {
                result.progressed=true;
                Reset(x,y,z,tx,ty,tz,distance,tick);
                return result;
            }
            result.stalledTicks=tick-lastProgressTick_;
            if (result.stalledTicks>=config_.hardStallTicks)
                result.severity=MovementStallSeverity::Hard;
            else if (result.stalledTicks>=config_.suspectedStallTicks)
                result.severity=MovementStallSeverity::Suspected;
            return result;
        }

        void BeginRecoveryWindow(float x,float y,float z,float tx,float ty,float tz,
            float distance,std::uint64_t tick)
        { Reset(x,y,z,tx,ty,tz,distance,tick); }
    };
}
