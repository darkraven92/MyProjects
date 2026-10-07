#pragma once

#include "WaterEvidence5875.h"
#include "../Control/RuntimeControl.h"
#include "../Debug/Logger.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <windows.h>

namespace Bot
{
    // Deliberately separate from the gameplay controller loop. Only reads the
    // existing world snapshot and native movement word; no action owner exists.
    class WaterEvidenceObserve5875
    {
        static std::string Hex(std::uint32_t value)
        {
            std::ostringstream out;
            out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
            return out.str();
        }

    public:
        static void Run(Control::RuntimeControlChannel& control,
                        Control::BotMode guiMode)
        {
            Debug::Logger::Info("WATER OBSERVE CONFIG mode=observe_only commands=none");
            WaterEvidenceTracker5875 tracker;
            std::uint64_t heartbeat=0;
            while (control.IsOpen() && control.RunRequested(false) &&
                   !control.UnloadRequested(false))
            {
                control.MarkRuntimeAttached(guiMode);
                control.MarkRuntimeState(Control::BotRunState::Running,
                    static_cast<LONG>((++heartbeat)&0x7fffffffULL));
                Objects::WorldState world;
                Objects::WorldReadStage stage=Objects::WorldReadStage::Complete;
                const bool valid=Objects::WorldStateReader::Read(world,&stage);
                const auto evidence=valid ? WaterEvidence5875::Read(world)
                                          : WaterEvidenceSnapshot5875{};
                const auto state=tracker.Update(evidence);
                const auto now=GetTickCount64();
                if (tracker.ShouldEmit(state,now))
                {
                    const bool swimming=evidence.movementKnown &&
                        (evidence.movementFlags&WaterEvidenceTracker5875::SwimmingMask);
                    Debug::Logger::Info(std::string("WATER EVIDENCE ")+
                        "swimmingKnown="+(evidence.movementKnown ? "yes" : "no")+
                        " swimming="+(evidence.movementKnown ? (swimming ? "yes" : "no") : "unknown")+
                        " movementFlags="+(evidence.movementKnown ? Hex(evidence.movementFlags) : "unknown")+
                        " breathKnown=no breathActive=unknown breathCurrent=unknown"
                        " breathMaximum=unknown breathScale=unknown breathPaused=unknown"
                        " fatigueKnown=no fatigueActive=unknown submergedKnown=no"
                        " submerged=unknown surfaceKnown=no groundContactKnown=no"
                        " classification="+WaterObservationClassName(state)+
                        " source="+(evidence.movementKnown ? "native_movement_word_5875" :
                                     valid ? "movement_read_or_signature_unavailable" :
                                             Objects::WorldReadStageName(stage))+
                        " sourceVerified="+(evidence.movementKnown ? "yes" : "no")+
                        " runtimeObserved="+(evidence.movementKnown ? "yes" : "no")+
                        " inferred=no");
                }
                Sleep(250);
            }
            // No input was ever acquired, hence no synthetic key release or
            // movement-stop command is issued on teardown either.
            if (control.IsOpen())
            {
                control.MarkRuntimeState(Control::BotRunState::Unloading,
                    static_cast<LONG>(heartbeat&0x7fffffffULL));
                control.MarkRuntimeDetached();
            }
            Debug::Logger::Info("WATER OBSERVE END reason=stop_or_unload commands=none");
        }
    };
}
