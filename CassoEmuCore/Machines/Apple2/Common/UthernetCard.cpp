#include "Pch.h"

#include "Machines/Apple2/Common/UthernetCard.h"
#include "Debugger/DiagnosticsSnapshot.h"
#include "Config/NetworkCardSettings.h"
#include "Core/MachineConfig.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/Events/ICommEventSink.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Shell/HostInputGate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::UthernetCard
//
//  Slot n's four registers are $C084 + n * $10 through $C087 + n * $10.
//
////////////////////////////////////////////////////////////////////////////////

UthernetCard::UthernetCard (int slot)
    : m_bridge (m_chip, m_network)
{
    m_slot    = slot;
    m_ioStart = (Word) (kSlotIoBase + slot * kSlotIoStride + kRegisterOffset);
    m_ioEnd   = (Word) (m_ioStart + kRegisterCount - 1);

    m_chip.SetBackend (this);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::~UthernetCard
//
//  The chip's backend goes before the chip does.
//
////////////////////////////////////////////////////////////////////////////////

UthernetCard::~UthernetCard()
{
    m_chip.SetBackend (nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::Create
//
//  ComponentRegistry factory; slot 3 when the configuration has none.
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> UthernetCard::Create (
    const DeviceConfig  & config,
    MemoryBus           & bus)
{
    int  slot = config.hasSlot ? config.slot : kDefaultSlot;



    (void) bus;

    return make_unique<UthernetCard> (slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::Read
//
////////////////////////////////////////////////////////////////////////////////

Byte UthernetCard::Read (Word address)
{
    return m_chip.ReadIndirect (address - m_ioStart);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::Write
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::Write (
    Word  address,
    Byte  value)
{
    m_chip.WriteIndirect (address - m_ioStart, value);
    NoteStates();
    UpdateIrq();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::Reset
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::Reset()
{
    m_chip.HardwareReset();
    m_cyclesSincePump = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::SoftReset
//
//  Ctrl-Reset has no effect on the chip. The machine clears every interrupt
//  source after resetting the bus, so the next pump interval asserts the line
//  again while the chip has an unmasked interrupt pending.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::SoftReset()
{
    m_irqAsserted = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::PowerCycle
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::PowerCycle (Prng & prng)
{
    (void) prng;

    Reset();
    m_irqAsserted = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UthernetCard::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    m_chip.SaveState (writer);
    writer.WriteUInt32 (m_cyclesSincePump);
    writer.WriteUInt64 (m_cycles);
    writer.WriteUInt64 (m_callIndex);

    hr = writer.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::LoadState
//
//  The interrupt line follows the loaded chip.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UthernetCard::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    hr = m_chip.LoadState (reader);
    CHR (hr);

    reader.ReadUInt32 (m_cyclesSincePump);
    reader.ReadUInt64 (m_cycles);
    reader.ReadUInt64 (m_callIndex);

    m_replayed.clear();

    hr = reader.EndSection();
    CHR (hr);

    UpdateIrq();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::AttachInterruptController
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UthernetCard::AttachInterruptController (IInterruptController * ic)
{
    HRESULT  hr = S_OK;



    CBRAEx (ic, E_INVALIDARG);

    hr = ic->RegisterSource (m_irqSource);
    CHR (hr);

    m_ic       = ic;
    m_irqBound = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::AttachNetwork
//
//  With a null network the card has no PC connectivity: connects time out
//  and datagrams are dropped, but DHCP and the virtual gateway work.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::AttachNetwork (
    IHostNetwork               * network,
    const NetworkCardSettings  & settings)
{
    m_bridge.SetHostNetwork (network);
    m_network.SetHostNetwork (network);

    ApplySettings (settings);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::ApplySettings
//
//  LAN access and port forwards, on a running card as at power-on. Open TCP
//  connections stay open; UDP flows close, and the port forwards reopen.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::ApplySettings (const NetworkCardSettings & settings)
{
    m_bridge.SetLanAccess (settings.lanAccess);
    m_network.Configure (settings.lanAccess, settings.portForwards);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::OnPumpInterval
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::OnPumpInterval (uint32_t cycles)
{
    uint64_t  cycle = (m_inputJournal != nullptr) ? m_inputJournal->GetCycle() - cycles : 0;



    m_cycles          += m_cyclesSincePump;
    m_cyclesSincePump  = 0;

    CallBackend (BackendCall::Pump, 0, 0, cycle);
    UpdateIrq();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::OnCommand
//
//  A command the guest wrote to a socket's command register, during the
//  instruction that wrote it.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::OnCommand (
    int   socket,
    Byte  command)
{
    CallBackend (BackendCall::Command, socket, command, (m_inputJournal != nullptr) ? m_inputJournal->GetCycle() : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::OnReset
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::OnReset()
{
    CallBackend (BackendCall::Reset, 0, 0, (m_inputJournal != nullptr) ? m_inputJournal->GetCycle() : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::CallBackend
//
//  Live, calls the bridge and journals the changes it made to the chip,
//  stamped with the cycle the instruction began at and numbered by the
//  count of calls. Behind live, applies the journaled changes for this
//  call instead. Every call is counted either way, a pump interval with no
//  socket open included, so the numbers match between a replay and live.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::CallBackend (
    BackendCall  call,
    int          socket,
    Byte         command,
    uint64_t     cycle)
{
    uint64_t  index = m_callIndex++;



    if (m_inputGate != nullptr && m_inputGate->IsHeld())
    {
        ApplyReplayed (index);
        return;
    }

    m_changeLog.clear();
    m_chip.SetChangeLog ((m_inputJournal != nullptr || m_commSink != nullptr) ? &m_changeLog : nullptr);

    switch (call)
    {
    case BackendCall::Command:
        m_bridge.OnCommand (socket, command);
        break;

    case BackendCall::Reset:
        m_bridge.OnReset();
        break;

    case BackendCall::Pump:
        if (m_bridge.IsActive())
        {
            m_bridge.Pump (m_cycles);
        }

        break;
    }

    m_chip.SetChangeLog (nullptr);

    if (m_inputJournal != nullptr && !m_changeLog.empty())
    {
        m_inputJournal->RecordObserved (cycle, InputKind::NetworkCall, 0, 0, index, m_changeLog);
    }

    EmitChanges (m_changeLog);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::ApplyReplayed
//
//  The changes journaled for call `index`, if any. A replay applies each
//  record just before the instruction it was made during, so the record
//  for this call is at the front.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::ApplyReplayed (uint64_t index)
{
    HRESULT  hr = S_OK;



    while (!m_replayed.empty() && m_replayed.front().index < index)
    {
        ASSERT (false && "a journaled network call was not replayed");
        m_replayed.pop_front();
    }

    if (!m_replayed.empty() && m_replayed.front().index == index)
    {
        hr = m_chip.ApplyChanges (m_replayed.front().changes);
        IGNORE_RETURN_VALUE (hr, S_OK);
        ASSERT (SUCCEEDED (hr));

        EmitChanges (m_replayed.front().changes);

        m_replayed.pop_front();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::ApplyInput
//
//  A journaled call goes ahead of the instruction it was made during, for
//  ApplyReplayed to apply when the replay makes the same call.
//
////////////////////////////////////////////////////////////////////////////////

bool UthernetCard::ApplyInput (const InputRecord & record)
{
    bool  isApplied = record.kind == InputKind::NetworkCall;



    if (isApplied)
    {
        m_replayed.push_back ({ record.data, record.payload });
    }

    return isApplied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::DropHostConnections
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::DropHostConnections()
{
    int  socket = 0;



    m_bridge.CloseAll();

    for (socket = 0; socket < W5100::kSocketCount; socket++)
    {
        if (m_chip.GetSocketStatus (socket) != W5100Registers::kSockClosed)
        {
            m_chip.SetSocketStatus      (socket, W5100Registers::kSockClosed);
            m_chip.RaiseSocketInterrupt (socket, W5100Registers::kSnIrDisconnect);
        }
    }

    m_replayed.clear();
    UpdateIrq();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::SetCommEventSink
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::SetCommEventSink (ICommEventSink * sink)
{
    int  socket = 0;



    m_commSink = sink;

    for (socket = 0; socket < W5100::kSocketCount; socket++)
    {
        m_notedStates[(size_t) socket] = m_chip.GetSocketStatus (socket);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::EmitChanges
//
//  The data in a call's changes, sent and received, then any socket state
//  the call changed.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::EmitChanges (std::string_view changes)
{
    HRESULT              hr     = S_OK;
    size_t               at     = 0;
    W5100::LoggedChange  logged;
    IoEvent              event;
    bool                 isData = false;



    BAIL_OUT_IF (m_commSink == nullptr, S_OK);

    while (at < changes.size())
    {
        hr = W5100::ReadChange (changes, at, logged);
        CHR (hr);

        isData = logged.change == W5100::Change::CommitTx || logged.change == W5100::Change::AppendRx;

        if (isData)
        {
            event         = IoEvent();
            event.kind    = (logged.change == W5100::Change::CommitTx) ? IoEventKind::SocketSent : IoEventKind::SocketReceived;
            event.address = m_ioStart;
            event.chip    = (Byte) logged.socket;
            event.length  = (uint16_t) logged.bytes.size();

            m_commSink->OnCommEvent (event, logged.bytes);
        }
    }

    NoteStates();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::NoteStates
//
//  A network log entry for each socket whose state differs from its last one.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::NoteStates()
{
    int      socket = 0;
    Byte     status = 0;
    IoEvent  event;



    if (m_commSink == nullptr)
    {
        return;
    }

    for (socket = 0; socket < W5100::kSocketCount; socket++)
    {
        status = m_chip.GetSocketStatus (socket);

        if (status != m_notedStates[(size_t) socket])
        {
            event         = IoEvent();
            event.kind    = IoEventKind::SocketState;
            event.address = m_ioStart;
            event.chip    = (Byte) socket;
            event.value   = status;
            event.reg     = m_notedStates[(size_t) socket];

            m_notedStates[(size_t) socket] = status;
            m_commSink->OnCommEvent (event, {});
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::UpdateIrq
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::UpdateIrq()
{
    bool  pending = m_chip.IsInterruptPending();



    if (!m_irqBound || pending == m_irqAsserted)
    {
        return;
    }

    if (pending)
    {
        m_ic->Assert (m_irqSource);
    }
    else
    {
        m_ic->Clear (m_irqSource);
    }

    m_irqAsserted = pending;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::GetDiagnostics
//
//  The registers are read from the chip's memory, never through the bus, so
//  the address pointer does not move.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    DiagnosticsGroup                               card    { "Card", {} };
    DiagnosticsGroup                               chip    { "W5100", {} };
    DiagnosticsGroup                               sockets { "Sockets", {} };
    std::array<Byte, W5100Registers::kMacLength>   mac     = {};
    int                                            socket  = 0;



    m_chip.GetSourceMac (mac);

    card.rows.push_back (MakeTextRow ("Slot",   std::format ("{}", m_slot)));
    card.rows.push_back (MakeFlagRow ("In use", IsActive(), "a socket or a host connection is open"));

    chip.rows.push_back (MakeByteRow ("Mode",        m_chip.PeekCommon (W5100Registers::kMr), { "RST", "", "", "PB", "PPPoE", "", "AI", "IND" }));
    chip.rows.push_back (MakeTextRow ("MAC",         std::format ("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5])));
    chip.rows.push_back (MakeTextRow ("IP address",  FormatIp (W5100Registers::kSipr)));
    chip.rows.push_back (MakeTextRow ("Gateway",     FormatIp (W5100Registers::kGar)));
    chip.rows.push_back (MakeTextRow ("Subnet mask", FormatIp (W5100Registers::kSubr)));
    chip.rows.push_back (MakeByteRow ("Interrupt",   m_chip.PeekCommon (W5100Registers::kIr), { "CONFLICT", "UNREACH", "PPPoE", "", "S3", "S2", "S1", "S0" }));

    for (socket = 0; socket < W5100::kSocketCount; socket++)
    {
        sockets.rows.push_back (MakeTextRow (std::format ("Socket {}", socket), FormatSocket (socket)));
    }

    snapshot.groups.push_back (std::move (card));
    snapshot.groups.push_back (std::move (chip));
    snapshot.groups.push_back (std::move (sockets));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::AddIoRegisters
//
//  The W5100's four indirect-mode registers, under the card's name and
//  slot.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::AddIoRegisters (std::vector<DiagnosticsIoRegister> & registers) const
{
    std::string  owner = std::format ("{}, slot {}", GetDiagnosticsTitle(), m_slot);



    registers.push_back ({ (Word) (m_ioStart + W5100Registers::kIndirectMode),        owner + ": W5100 mode register" });
    registers.push_back ({ (Word) (m_ioStart + W5100Registers::kIndirectAddressHigh), owner + ": W5100 address register, high byte" });
    registers.push_back ({ (Word) (m_ioStart + W5100Registers::kIndirectAddressLow),  owner + ": W5100 address register, low byte" });
    registers.push_back ({ (Word) (m_ioStart + W5100Registers::kIndirectData),        owner + ": W5100 data register: the byte at the address register's address" });
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::FormatIp
//
////////////////////////////////////////////////////////////////////////////////

std::string UthernetCard::FormatIp (Word address) const
{
    return std::format ("{}.{}.{}.{}",
                        m_chip.PeekCommon (address),
                        m_chip.PeekCommon ((Word) (address + 1)),
                        m_chip.PeekCommon ((Word) (address + 2)),
                        m_chip.PeekCommon ((Word) (address + 3)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::FormatSocket
//
//  The socket's state by its W5100 name, and its port once it is open.
//
////////////////////////////////////////////////////////////////////////////////

std::string UthernetCard::FormatSocket (int socket) const
{
    Byte         status = m_chip.GetSocketStatus (socket);
    std::string  name;



    switch (status)
    {
    case W5100Registers::kSockClosed:      name = "closed";       break;
    case W5100Registers::kSockInit:        name = "TCP, open";    break;
    case W5100Registers::kSockListen:      name = "TCP, listening"; break;
    case W5100Registers::kSockSynSent:     name = "TCP, connecting"; break;
    case W5100Registers::kSockSynReceived: name = "TCP, accepting"; break;
    case W5100Registers::kSockEstablished: name = "TCP, connected"; break;
    case W5100Registers::kSockFinWait:
    case W5100Registers::kSockClosing:
    case W5100Registers::kSockTimeWait:
    case W5100Registers::kSockCloseWait:
    case W5100Registers::kSockLastAck:     name = "TCP, closing"; break;
    case W5100Registers::kSockUdp:         name = "UDP";          break;
    case W5100Registers::kSockIpRaw:       name = "IP raw";       break;
    case W5100Registers::kSockMacRaw:      name = "MAC raw";      break;
    default:                               name = std::format ("status ${:02X}", status); break;
    }

    if (status == W5100Registers::kSockClosed || status == W5100Registers::kSockMacRaw)
    {
        return name;
    }

    return std::format ("{}, port {}", name, m_chip.GetSourcePort (socket));
}
