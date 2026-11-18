#include "Pch.h"

#include "Shell/MachineHostInput.h"
#include "Core/IMachineState.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/Reverse/HeldInputWatch.h"
#include "Devices/Acia6551.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"
#include "Machines/Apple2/Common/UthernetCard.h"
#include "Shell/MachineParts.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::MachineHostInput
//
////////////////////////////////////////////////////////////////////////////////

MachineHostInput::MachineHostInput (MachineParts & parts, const uint64_t * position) :
    m_parts    (parts),
    m_position (position)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::Record
//
//  Called on the CPU thread at an instruction boundary, before or in the
//  same breath as the input is applied, so the stamp is the point the guest
//  first reads it. A machine with no CPU yet stamps zero.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHostInput::Record (
    InputKind         kind,
    Byte              value,
    uint16_t          detail,
    std::string_view  payload)
{
    uint64_t  cycle = 0;



    if (!m_journal.IsOn())
    {
        return;
    }

    if (m_parts.cpu != nullptr)
    {
        cycle = m_parts.cpu->GetTotalCycles();
    }

    m_journal.Record (cycle, kind, value, detail, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::SetJournalOn
//
//  The devices point at the journal, or at nothing, so a device whose reads
//  see host input pays one null test while it is off. CPU thread, between
//  instructions.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHostInput::SetJournalOn (bool isOn)
{
    m_journal.SetOn (isOn);
    AttachJournal();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::AttachJournal
//
//  Points every device whose reads see host input at the journal, or at
//  null when the journal is off. The serial chips and the network
//  card also get the gate, checked before an attachment's bytes are taken,
//  so that behind live they wait.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHostInput::AttachJournal()
{
    MachineRefs   & refs    = m_parts.refs;
    InputJournal  * journal = m_journal.IsOn() ? &m_journal : nullptr;
    EmuCpu        * cpu     = m_parts.cpu.get();
    size_t          i       = 0;



    m_journal.SetCycleSource    ((cpu != nullptr) ? cpu->GetCycleCounterPtr() : nullptr);
    m_journal.SetCycleBase      ((cpu != nullptr) ? cpu->GetCycleBasePtr() : nullptr);
    m_journal.SetPositionSource (m_position);

    if (refs.keyboard != nullptr)
    {
        refs.keyboard->SetInputJournal (journal);
    }

    if (refs.gamePort != nullptr)
    {
        refs.gamePort->SetInputJournal (journal);
    }

    if (refs.iieSoftSwitches != nullptr)
    {
        refs.iieSoftSwitches->SetInputJournal (journal);
    }

    if (m_parts.mouse != nullptr)
    {
        m_parts.mouse->SetInputJournal (journal);
    }

    if (m_parts.joyport != nullptr)
    {
        m_parts.joyport->SetInputJournal (journal);
    }

    for (i = 0; i < refs.serialChips.size(); i++)
    {
        refs.serialChips[i]->SetInputJournal  (journal, (uint16_t) i);
        refs.serialChips[i]->SetHostInputGate (&m_gate);
    }

    if (refs.uthernet != nullptr)
    {
        refs.uthernet->SetInputJournal  (journal);
        refs.uthernet->SetHostInputGate (&m_gate);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::ApplyDeviceInput
//
//  Replay: one journal record a device made at a read goes back to that
//  device, which stores the value so the same read returns the same result.
//  The caller applies each record just before the instruction that begins at
//  its cycle. A reset, power cycle or disk change is no device's, and the
//  replayer dispatches it as a command.
//
////////////////////////////////////////////////////////////////////////////////

bool MachineHostInput::ApplyDeviceInput (const InputRecord & record)
{
    MachineRefs  & refs      = m_parts.refs;
    bool           isApplied = false;



    if (refs.keyboard != nullptr)
    {
        isApplied = refs.keyboard->ApplyInput (record);
    }

    if (!isApplied && refs.iieSoftSwitches != nullptr)
    {
        isApplied = refs.iieSoftSwitches->ApplyInput (record);
    }

    if (!isApplied && refs.gamePort != nullptr)
    {
        isApplied = refs.gamePort->ApplyInput (record);
    }

    if (!isApplied && m_parts.mouse != nullptr)
    {
        isApplied = m_parts.mouse->ApplyInput (record);
    }

    if (!isApplied && m_parts.joyport != nullptr)
    {
        isApplied = m_parts.joyport->ApplyInput (record);
    }

    if (!isApplied && IsSerialInput (record.kind) && record.detail < refs.serialChips.size())
    {
        isApplied = refs.serialChips[record.detail]->ApplyInput (record);
    }

    if (!isApplied && refs.uthernet != nullptr)
    {
        isApplied = refs.uthernet->ApplyInput (record);
    }

    return isApplied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::IsSerialInput
//
////////////////////////////////////////////////////////////////////////////////

bool MachineHostInput::IsSerialInput (InputKind kind)
{
    return kind == InputKind::AciaByte || kind == InputKind::AciaLines || kind == InputKind::AciaConnection;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::DropConnections
//
////////////////////////////////////////////////////////////////////////////////

void MachineHostInput::DropConnections()
{
    if (m_parts.refs.uthernet != nullptr)
    {
        m_parts.refs.uthernet->DropHostConnections();
    }

    for (Acia6551 * acia : m_parts.refs.serialChips)
    {
        acia->DropConnection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::ResyncConnections
//
//  Each serial chip receives again, behind what was restored with the point,
//  the bytes it took after the point, each with its receive errors, and the
//  network card brings its sockets back in step. The serial attachments are
//  not called: each poll sets the 6551's CTS, DSR and DCD from the
//  attachment, so the first poll after the resume corrects the restored
//  levels.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHostInput::ResyncConnections (size_t cursor)
{
    MachineRefs  & refs  = m_parts.refs;
    size_t         index = 0;



    for (index = cursor; index < m_journal.GetEndIndex(); index++)
    {
        const InputRecord  & record = m_journal.GetRecord (index);

        if (record.kind == InputKind::AciaByte && record.detail < refs.serialChips.size())
        {
            refs.serialChips[record.detail]->ReceiveFromHostNow (record.value, (Byte) record.data);
        }
    }

    if (refs.uthernet != nullptr)
    {
        refs.uthernet->ResyncHostConnections (&m_journal, cursor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::Sample
//
//  A key pressed or a paddle moved between two guest reads is in the live
//  machine from the moment the host wrote it, but it goes in the journal
//  only at a guest read. Sampling at each slice boundary puts the change in
//  the journal where the CPU thread first had it, as a boundary record, so
//  every position after it replays with it. Within a slice a change waits
//  for the next read or the next sample, whichever comes first.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHostInput::Sample()
{
    MachineRefs  & refs = m_parts.refs;



    if (!m_journal.IsOn())
    {
        return;
    }

    m_journal.SetSampling (true);

    if (refs.keyboard != nullptr)
    {
        refs.keyboard->SampleHostInputs();
    }

    if (refs.gamePort != nullptr)
    {
        refs.gamePort->SampleHostInputs();
    }

    if (refs.iieSoftSwitches != nullptr)
    {
        refs.iieSoftSwitches->SampleHostInputs();
    }

    if (m_parts.mouse != nullptr)
    {
        m_parts.mouse->SampleHostInputs();
    }

    if (m_parts.joyport != nullptr)
    {
        m_parts.joyport->SampleHostInputs();
    }

    m_journal.SetSampling (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::SetHeldInputWatch
//
//  The devices whose reads see host input, pointed at the watch or at
//  nothing, with the watch given the machine's position.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHostInput::SetHeldInputWatch (HeldInputWatch * watch)
{
    MachineRefs  & refs = m_parts.refs;



    m_heldInputWatch = watch;

    if (watch != nullptr)
    {
        watch->SetPositionSource (m_position);
    }

    if (refs.keyboard != nullptr)
    {
        refs.keyboard->SetHeldInputWatch (watch);
    }

    if (refs.gamePort != nullptr)
    {
        refs.gamePort->SetHeldInputWatch (watch);
    }

    if (refs.iieSoftSwitches != nullptr)
    {
        refs.iieSoftSwitches->SetHeldInputWatch (watch);
    }

    if (m_parts.mouse != nullptr)
    {
        m_parts.mouse->SetHeldInputWatch (watch);
    }

    if (m_parts.joyport != nullptr)
    {
        m_parts.joyport->SetHeldInputWatch (watch);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::SaveState
//
//  Each device's section, back to back, in GetParts order.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHostInput::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    GetParts (m_stateParts);

    for (const IMachineState * part : m_stateParts)
    {
        hr = part->SaveState (writer);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::LoadState
//
//  A blob saved by this machine's devices loads back into the same devices;
//  one from another machine fails on the first section tag that differs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHostInput::LoadState (std::string_view blob)
{
    HRESULT      hr     = S_OK;
    StateReader  reader (reinterpret_cast<const Byte *> (blob.data()), blob.size());



    GetParts (m_stateParts);

    for (IMachineState * part : m_stateParts)
    {
        hr = part->LoadState (reader);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput::GetParts
//
//  The devices whose state the UI and controller threads write, each once:
//  the keyboard through its most derived type, as the machine's state saves
//  it. Filled into the caller's list, whose capacity is reused from call to
//  call.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHostInput::GetParts (std::vector<IMachineState *> & outParts) const
{
    IMachineState  * candidates[] =
    {
        dynamic_cast<IMachineState *> (m_parts.refs.keyboard),
        m_parts.refs.iieSoftSwitches,
        m_parts.refs.gamePort,
        m_parts.mouse.get(),
        m_parts.joyport.get(),
    };



    outParts.clear();

    for (IMachineState * part : candidates)
    {
        if (part != nullptr)
        {
            outParts.push_back (part);
        }
    }
}
