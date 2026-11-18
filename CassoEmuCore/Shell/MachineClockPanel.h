#pragma once

#include "Pch.h"

#include "Debugger/IDiagnosticsProvider.h"
#include "Ui/UiCommandTypes.h"

class MachineConnectors;
struct MachineConfig;
struct MachineParts;
class MachineTape;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineClockPanel
//
//  The clock panel: the cycle counters, the video beam, and the speed the
//  CPU thread runs at, which it sets here since the machine doesn't pace
//  itself. And the list of every panel the machine's devices publish.
//
////////////////////////////////////////////////////////////////////////////////

class MachineClockPanel : public IDiagnosticsProvider
{
public:
    MachineClockPanel (const MachineParts      & parts,
                       const MachineConfig     & config,
                       const MachineConnectors & connectors,
                       const MachineTape       & tape);

    MachineClockPanel (const MachineClockPanel &)             = delete;
    MachineClockPanel & operator= (const MachineClockPanel &) = delete;

    void  SetSpeedMode (SpeedMode mode) noexcept { m_speedMode = mode; }

    //  The devices of the machine that publish debugger panels, the clock
    //  first. Only what the machine has: a ][ doesn't list an MMU, and an
    //  empty slot doesn't list a card.
    std::vector<const IDiagnosticsProvider *>  GetProviders() const;

    std::string  GetDiagnosticsId    () const override { return "clock"; }
    std::string  GetDiagnosticsTitle () const override { return "Clock"; }
    void         GetDiagnostics      (DiagnosticsSnapshot & snapshot) const override;

private:
    const MachineParts       & m_parts;
    const MachineConfig      & m_config;
    const MachineConnectors  & m_connectors;
    const MachineTape        & m_tape;
    SpeedMode                  m_speedMode = SpeedMode::Authentic;
};
