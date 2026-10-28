#pragma once

#include "Pch.h"

#include "Core/IInterruptController.h"
#include "Core/IMachineState.h"
#include "Core/MemoryDevice.h"
#include "Debugger/IDiagnosticsProvider.h"
#include "Devices/Network/VirtualNetwork.h"
#include "Devices/Network/W5100.h"
#include "Devices/Network/W5100SocketBridge.h"

class HostInputGate;
class ICommEventSink;
class IHostNetwork;
class InputJournal;
class MemoryBus;
class NetworkCardSettings;

struct DeviceConfig;
struct InputRecord;





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard
//
//  The a2RetroSystems Uthernet II: a W5100 in indirect bus mode behind
//  $C0n4-$C0n7 (n = slot + 8), with no ROM. $C0n0-$C0n3 are not decoded.
//
//  Host networking runs on the emulation thread: `Tick` counts CPU cycles
//  and, once every kPumpCycles, polls the host sockets if any socket or flow
//  is open and updates the interrupt line. Between those a tick is an add
//  and a compare. A register write updates the interrupt line at once, so a
//  handler that clears the chip's interrupt does not see it again.
//
//  The card is not connected to the Apple II RESET line, so Ctrl-Reset has no
//  effect on the chip; power-on resets it.
//
////////////////////////////////////////////////////////////////////////////////

class UthernetCard : public MemoryDevice, public IMachineState, public IDiagnosticsProvider, private IW5100Backend
{
public:

    static constexpr Word      kSlotIoBase     = 0xC080;
    static constexpr Word      kSlotIoStride   = 0x10;
    static constexpr Word      kRegisterOffset = 0x4;
    static constexpr Word      kRegisterCount  = 4;
    static constexpr uint32_t  kPumpCycles     = 1023;
    static constexpr int       kDefaultSlot    = 3;
    static constexpr uint32_t  kStateTag       = IMachineState::MakeTag ('U', 'T', 'H', '2');
    static constexpr uint16_t  kStateVersion   = 1;

    explicit UthernetCard (int slot);
    ~UthernetCard () override;

    static unique_ptr<MemoryDevice>  Create (const DeviceConfig & config, MemoryBus & bus);

    Byte  Read       (Word address)              override;
    void  Write      (Word address, Byte value)  override;
    Word  GetStart   () const                    override { return m_ioStart; }
    Word  GetEnd     () const                    override { return m_ioEnd; }
    void  Reset      ()                          override;
    void  SoftReset  ()                          override;
    void  PowerCycle (Prng & prng)               override;

    HRESULT  AttachInterruptController (IInterruptController * ic);
    void     AttachNetwork             (IHostNetwork * network, const NetworkCardSettings & settings);
    void     ApplySettings             (const NetworkCardSettings & settings);

    // Port forwards whose host port could not be bound, as sentences.
    const std::vector<std::string> & GetNetworkErrors () const { return m_network.GetErrors(); }

    void  Tick (uint32_t cycles)
    {
        m_cyclesSincePump += cycles;

        if (m_cyclesSincePump >= kPumpCycles)
        {
            OnPumpInterval (cycles);
        }
    }

    int                  GetSlot   () const { return m_slot; }
    W5100              & GetChip   ()       { return m_chip; }
    W5100SocketBridge  & GetBridge ()       { return m_bridge; }
    VirtualNetwork     & GetNetwork()       { return m_network; }

    // A guest socket or a host flow is open.
    bool                 IsActive  () const { return m_bridge.IsActive(); }

    // IDiagnosticsProvider: the slot and whether the card is in use, the
    // W5100's mode, addresses and interrupt register, and each socket.
    std::string  GetDiagnosticsId    () const override { return "uthernet"; }
    std::string  GetDiagnosticsTitle () const override { return "Uthernet II"; }
    void         GetDiagnostics      (DiagnosticsSnapshot & snapshot) const override;
    void         AddIoRegisters      (std::vector<DiagnosticsIoRegister> & registers) const override;
    uint64_t             GetCycles () const { return m_cycles; }

    // IMachineState: the W5100's section, then the pump's count.
    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

    // History: each call to the network side that changes the chip is
    // journaled with the changes. Behind live, when HostInputGate::IsHeld
    // returns true, the journal supplies the changes and the network side is
    // not called, so a replay sends nothing and receives nothing.
    void  SetInputJournal  (InputJournal * journal)         { m_inputJournal = journal; }
    void  SetHostInputGate (const HostInputGate * gate)     { m_inputGate = gate; }
    bool  ApplyInput       (const InputRecord & record);

    // A state file replaced the machine. The host's connections are from the
    // state replaced, so each is closed, and each socket the guest has open
    // reads as closed by the remote end.
    void  DropHostConnections ();

    // The machine is live again at a point in the past. Each host connection
    // that matches a restored socket stays open, and the rest are brought
    // back in step (see W5100SocketBridge::Resync).
    void  ResyncHostConnections ();

    // The debugger's network log: each socket's state changes and the data
    // each sends and receives go to `sink`, or nowhere when it is null.
    // Emulation thread.
    void  SetCommEventSink (ICommEventSink * sink);

private:
    enum class BackendCall
    {
        Command,
        Reset,
        Pump,
    };

    struct ReplayedCall
    {
        uint64_t     index = 0;
        std::string  changes;
    };

    std::string  FormatIp     (Word address) const;
    std::string  FormatSocket (int socket) const;

    // IW5100Backend: the chip's calls, passed to the bridge through CallBackend.
    void  OnCommand      (int socket, Byte command) override;
    void  OnReset        ()                         override;

    void  CallBackend    (BackendCall call, int socket, Byte command, uint64_t cycle);
    void  ApplyReplayed  (uint64_t index);
    void  EmitChanges    (std::string_view changes);
    void  NoteStates     ();
    void  OnPumpInterval (uint32_t cycles);
    void  UpdateIrq      ();

    int                                      m_slot            = 0;
    Word                                     m_ioStart         = 0;
    Word                                     m_ioEnd           = 0;
    W5100                                    m_chip;
    VirtualNetwork                           m_network;
    W5100SocketBridge                        m_bridge;
    IInterruptController                   * m_ic              = nullptr;
    IrqSourceId                              m_irqSource       = 0;
    bool                                     m_irqBound        = false;
    bool                                     m_irqAsserted     = false;
    uint32_t                                 m_cyclesSincePump = 0;
    uint64_t                                 m_cycles          = 0;
    uint64_t                                 m_callIndex       = 0;   // calls to the network side so far
    InputJournal                           * m_inputJournal    = nullptr;
    const HostInputGate                    * m_inputGate       = nullptr;
    std::string                              m_changeLog;
    std::deque<ReplayedCall>                 m_replayed;   // journaled calls a replay applied ahead of their instruction
    ICommEventSink                         * m_commSink        = nullptr;
    std::array<Byte, W5100::kSocketCount>    m_notedStates     = {};   // the socket states in the last network log entries
};
