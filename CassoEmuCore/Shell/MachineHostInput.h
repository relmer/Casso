#pragma once

#include "Pch.h"

#include "Debugger/Reverse/InputJournal.h"
#include "Shell/HostInputGate.h"

class HeldInputWatch;
class IMachineState;
struct MachineParts;
class StateWriter;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostInput
//
//  The input the host writes into the machine: the keyboard, the game port,
//  the //e paddles and buttons, the mouse, the Joyport, and the bytes a
//  serial port or the network card takes from the host. The journal of what
//  was applied, the gate that stops the host's input while reverse execution
//  has the machine behind live, and the watch on what that input would
//  change. The journal outlives a machine switch.
//
////////////////////////////////////////////////////////////////////////////////

class MachineHostInput
{
public:
    MachineHostInput (MachineParts & parts, const uint64_t * position);

    MachineHostInput (const MachineHostInput &)             = delete;
    MachineHostInput & operator= (const MachineHostInput &) = delete;

    InputJournal          & GetJournal ()       noexcept { return m_journal; }
    const InputJournal    & GetJournal () const noexcept { return m_journal; }
    HostInputGate         & GetGate    ()       noexcept { return m_gate; }

    //  Stamps a host input with the CPU's cycle; with the journal off it
    //  does nothing.
    void  Record (InputKind kind, Byte value, uint16_t detail, std::string_view payload);

    //  Turn the journal on or off and point the devices whose reads see host
    //  input at it (see InputJournal::RecordObserved). AttachJournal runs
    //  again after a machine switch builds new devices.
    void  SetJournalOn  (bool isOn);
    void  AttachJournal ();

    //  For a replay: one record a device made at a read, applied to that
    //  device again. False for a record no device here has.
    bool  ApplyDeviceInput (const InputRecord & record);

    //  A state file replaced the machine: the connections its devices have on
    //  the host are from the state replaced, so each is closed.
    void  DropConnections();

    //  The machine is live again at a point in the past, the journal's entry
    //  `cursor` the first after it: each device brings its connections on the
    //  host back in step, and what the host sent after the point goes to the
    //  guest again.
    void  ResyncConnections (size_t cursor);

    //  At the start of an execution slice, with the journal on: each
    //  host-written input that changed since it was last seen goes in the
    //  journal at this position.
    void  Sample();

    //  The watch on the input behind live, or null. Set, the devices whose
    //  reads see host input report a read that input would change, and
    //  the run ends after the instruction that made it.
    void                    SetHeldInputWatch (HeldInputWatch * watch);
    const HeldInputWatch  * GetHeldInputWatch () const noexcept { return m_heldInputWatch; }
    HeldInputWatch        * GetHeldInputWatch ()       noexcept { return m_heldInputWatch; }

    //  The saved state of every device whose state another thread writes, as
    //  one blob, and back; see InputKind::HostState.
    HRESULT  SaveState (StateWriter & writer) const;
    HRESULT  LoadState (std::string_view blob);

private:
    static bool  IsSerialInput (InputKind kind);

    void  GetParts (std::vector<IMachineState *> & outParts) const;

    MachineParts                          & m_parts;
    const uint64_t                        * m_position       = nullptr;
    InputJournal                            m_journal;
    HostInputGate                           m_gate;
    HeldInputWatch                        * m_heldInputWatch = nullptr;

    // The part list the state functions fill, a member so that its capacity
    // is reused.
    mutable std::vector<IMachineState *>    m_stateParts;
};
