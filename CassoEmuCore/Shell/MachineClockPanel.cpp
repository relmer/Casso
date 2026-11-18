#include "Pch.h"

#include "Shell/MachineClockPanel.h"
#include "Core/MachineConfig.h"
#include "Debugger/DiagnosticsSnapshot.h"
#include "Devices/Acia6551.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "Machines/Apple2/Common/UthernetCard.h"
#include "Shell/MachineConnectors.h"
#include "Shell/MachineParts.h"
#include "Shell/MachineTape.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineClockPanel::MachineClockPanel
//
////////////////////////////////////////////////////////////////////////////////

MachineClockPanel::MachineClockPanel (
    const MachineParts       & parts,
    const MachineConfig      & config,
    const MachineConnectors  & connectors,
    const MachineTape        & tape) :
    m_parts      (parts),
    m_config     (config),
    m_connectors (connectors),
    m_tape       (tape)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineClockPanel::GetProviders
//
//  A 6551 that is a port by itself, as each of the //c's is, has a panel; a
//  card's is in the card's panel.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<const IDiagnosticsProvider *> MachineClockPanel::GetProviders() const
{
    const MachineRefs                        & refs      = m_parts.refs;
    std::vector<const IDiagnosticsProvider *>  providers = { this };
    const IDiagnosticsProvider               * devices[] =
    {
        refs.keyboard,
        refs.softSwitches,
        m_parts.mmu.get(),
        refs.diskController,
        refs.mockingboard,
        refs.superSerialCard,
        refs.uthernet,
    };



    for (const IDiagnosticsProvider * device : devices)
    {
        if (device != nullptr)
        {
            providers.push_back (device);
        }
    }

    for (const PrinterCard * card : m_connectors.GetParallelCards())
    {
        if (card != nullptr)
        {
            providers.push_back (card);
        }
    }

    for (const Acia6551 * acia : refs.serialChips)
    {
        if (!acia->GetDiagnosticsId().empty())
        {
            providers.push_back (acia);
        }
    }

    if (refs.cassettePort != nullptr)
    {
        providers.push_back (&m_tape.GetDiagnostics());
    }

    return providers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineClockPanel::GetDiagnostics
//
////////////////////////////////////////////////////////////////////////////////

void MachineClockPanel::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    static constexpr const char * kSpeeds[] = { "authentic", "double", "maximum" };
    const EmuCpu                * cpu       = m_parts.cpu.get();
    const VideoTiming           * timing    = m_parts.videoTiming.get();
    DiagnosticsGroup              cpuGroup  { "CPU", {} };
    DiagnosticsGroup              video     { "Video", {} };



    cpuGroup.rows.push_back (MakeTextRow ("Cycles", std::format ("{}", (cpu != nullptr) ? cpu->GetTotalCycles() : 0)));
    cpuGroup.rows.push_back (MakeTextRow ("Clock",  std::format ("{} Hz", m_config.clockSpeed)));
    cpuGroup.rows.push_back (MakeTextRow ("Speed",  kSpeeds[(size_t) m_speedMode]));
    snapshot.groups.push_back (std::move (cpuGroup));

    if (timing != nullptr)
    {
        video.rows.push_back (MakeTextRow ("Scanline",       std::format ("{}", timing->GetCurrentScanline())));
        video.rows.push_back (MakeTextRow ("Cycle in line",  std::format ("{}", timing->GetHorizontalPos())));
        video.rows.push_back (MakeTextRow ("Cycle in frame", std::format ("{}", timing->GetCycleInFrame())));
        snapshot.groups.push_back (std::move (video));
    }
}
