#include "Pch.h"

#include "Shell/MachineOutputMute.h"
#include "Audio/IDriveAudioSink.h"
#include "Devices/Acia6551.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Shell/MachineConnectors.h"
#include "Shell/MachineParts.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineOutputMute::MachineOutputMute
//
////////////////////////////////////////////////////////////////////////////////

MachineOutputMute::MachineOutputMute (MachineParts & parts, MachineConnectors & connectors) :
    m_parts      (parts),
    m_connectors (connectors)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineOutputMute::SetMuted
//
//  The speaker and the printer cards have a flag each. The Disk II's drive
//  sounds go out through its audio sink, which is stored here and detached
//  while muted. The serial chips are muted with the printer, since what they
//  send goes to a file or another machine. Muting twice, or unmuting twice,
//  changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void MachineOutputMute::SetMuted (bool isSoundMuted, bool isPrinterMuted)
{
    MachineRefs      & refs       = m_parts.refs;
    Disk2Controller  * controller = refs.diskController;



    for (PrinterCard * card : m_connectors.GetParallelCards())
    {
        if (card != nullptr)
        {
            card->SetMuted (isPrinterMuted);
        }
    }

    m_isPrinterMuted = isPrinterMuted;

    for (Acia6551 * acia : refs.serialChips)
    {
        acia->SetOutputMuted (isPrinterMuted);
    }

    if (isSoundMuted == m_isSoundMuted)
    {
        return;
    }

    m_isSoundMuted = isSoundMuted;

    if (refs.speaker != nullptr)
    {
        refs.speaker->SetMuted (isSoundMuted);
    }

    if (controller == nullptr)
    {
        return;
    }

    if (isSoundMuted)
    {
        m_mutedDiskAudio   = controller->GetSignals().GetAudioSink();
        m_wasMotorOnAtMute = controller->GetLines().IsMotorOn();
        controller->GetSignals().SetAudioSink (nullptr);
        return;
    }

    controller->GetSignals().SetAudioSink (m_mutedDiskAudio);

    // The motor's hum follows the drive, so a motor that started or stopped
    // while muted goes to the sink now.
    if (m_mutedDiskAudio != nullptr && controller->GetLines().IsMotorOn() != m_wasMotorOnAtMute)
    {
        if (m_wasMotorOnAtMute)
        {
            m_mutedDiskAudio->OnMotorDisengaged();
        }
        else
        {
            m_mutedDiskAudio->OnMotorEngaged();
        }
    }

    m_mutedDiskAudio = nullptr;
}
