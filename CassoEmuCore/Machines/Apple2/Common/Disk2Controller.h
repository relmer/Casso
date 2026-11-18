#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Core/ObserverSlots.h"
#include "Core/IMachineState.h"
#include "Debugger/IDiagnosticsProvider.h"
#include "Machines/Apple2/Common/Disk2CardSequencer.h"
#include "Machines/Apple2/Common/Disk2Clock.h"
#include "Machines/Apple2/Common/Disk2Drive.h"
#include "Machines/Apple2/Common/Disk2IwmPort.h"
#include "Machines/Apple2/Common/Disk2Lines.h"
#include "Machines/Apple2/Common/Disk2Recorder.h"
#include "Machines/Apple2/Common/Disk2Signals.h"

class IDisk2EventSink;
class IDiskPortHook;
struct DeviceConfig;
class MemoryBus;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller
//
//  A Disk II controller in a slot, or the //c's disk port: its 16-byte page
//  of switches at $C0n0 to $C0nF, two drives, and the lines between them.
//  The switches go to the Disk II card's sequencer, or in IWM mode to the
//  //c's IWM; either front end sets the same lines and reports through the
//  same signals and recorder. The slot ROM is the MMU's, not the
//  controller's.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2Controller : public MemoryDevice, public IDiagnosticsProvider, public IMachineState
{
public:
    static constexpr int  kDriveCount = Disk2Drive::kPerController;

    explicit Disk2Controller (int slot);

    Disk2Controller (const Disk2Controller &)             = delete;
    Disk2Controller & operator= (const Disk2Controller &) = delete;

    Byte  Read       (Word address) override;
    void  Write      (Word address, Byte value) override;
    bool  TryPeekIo  (Word address, Byte & value) override;
    Word  GetStart   () const override { return m_ioStart; }
    Word  GetEnd     () const override { return m_ioEnd; }
    void  Reset      () override;
    void  SoftReset  () override;
    void  PowerCycle (Prng & prng) override;

    // Cycle-driven advance: the CPU pumps the instruction's cycles.
    void  Tick (uint32_t cpuCycles);

    //  A disk change in a drive, noted in the recent nibbles and sent to the
    //  sinks. SetExternalDisk attaches a disk another object has, typically
    //  the disk store's, or the drive's empty one with nullptr; it sends
    //  nothing to the sinks, since the shell reports the change
    //  (Disk2Signals::OnDiskInserted).
    HRESULT  MountDisk       (int drive, const string & path);
    void     EjectDisk       (int drive);
    void     SetExternalDisk (int drive, DiskImage * external);

    //  Sets the slot in the signals and in the address-mark watcher, which
    //  reports its marks through the same sinks. The one-argument form sets
    //  the panel's slot. Caller-owned; nullptr detaches.
    void  SetEventSink (IDisk2EventSink * sink) noexcept { SetEventSink (ObserverSlot::Panel, sink); }
    void  SetEventSink (ObserverSlot slot, IDisk2EventSink * sink) noexcept;

    // Apple //c IWM mode: the switches go to the IWM, whose two drives are
    // this controller's, read and written a cell at a time. Off by default,
    // for a Disk II card.
    void  SetIwmMode (bool isIwm) { m_iwmMode = isIwm; }
    bool  IsIwmMode  () const     { return m_iwmMode; }

    //  A SmartPort chain on the //c's disk port. Each access to the switches
    //  goes to it after the controller, and its byte replaces the
    //  controller's for drive 2 while it has the bus. On the //c Plus it also
    //  enables the drives. Caller-owned.
    void  SetPortHook (IDiskPortHook * hook) { m_portHook = hook; }

    int  GetSlot() const { return m_slot; }

    Disk2Drive                & GetDrive    (int drive)       { return m_drives[(size_t) std::clamp (drive, 0, kDriveCount - 1)]; }
    const Disk2Drive          & GetDrive    (int drive) const { return m_drives[(size_t) std::clamp (drive, 0, kDriveCount - 1)]; }
    Disk2Lines                & GetLines    ()                { return m_lines; }
    const Disk2Lines          & GetLines    () const          { return m_lines; }
    Disk2Signals              & GetSignals  ()                { return m_signals; }
    const Disk2Signals        & GetSignals  () const          { return m_signals; }
    Disk2Recorder             & GetRecorder ()                { return m_recorder; }
    const Disk2Recorder       & GetRecorder () const          { return m_recorder; }
    Disk2Clock                & GetClock    ()                { return m_clock; }
    const Disk2Clock          & GetClock    () const          { return m_clock; }
    Disk2CardSequencer        & GetCard     ()                { return m_card; }
    const Disk2CardSequencer  & GetCard     () const          { return m_card; }
    Disk2IwmPort              & GetIwmPort  ()                { return m_iwmPort; }
    const Disk2IwmPort        & GetIwmPort  () const          { return m_iwmPort; }

    // The Disk II panel: drive, motor, head, phase magnets and the read state.
    std::string  GetDiagnosticsId    () const override { return "disk"; }
    std::string  GetDiagnosticsTitle () const override { return "Disk II"; }
    void         GetDiagnostics      (DiagnosticsSnapshot & snapshot) const override;

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    // IMachineState: the lines, both heads, the spin-up and spin-down
    // counters, the IWM mode register, the idle callback's window, the last
    // catch-up cycle, both engines, the watcher, the recent nibbles and the
    // IWM. The disks are not in this section: their media is saved through
    // DiskImage or DiskTrackSnapshot and must be loaded first. Sinks,
    // callbacks, the cycle source and the IWM-mode flag are wiring; the flag
    // is saved only to check it matches. A load doesn't fire a sink or a
    // callback.
    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

    static constexpr uint32_t  kStateTag                 = IMachineState::MakeTag ('D', 'S', 'K', '2');
    static constexpr uint16_t  kStateVersion             = 5;
    static constexpr uint16_t  kWatcherStateVersion      = 2;   // the first with the watcher's section
    static constexpr uint16_t  kPerDriveHeadStateVersion = 3;   // the first with a head position per drive
    static constexpr uint16_t  kNibbleRecordStateVersion = 3;   // the first with the recent nibbles
    static constexpr uint16_t  kIwmStateVersion          = 5;   // the first with the //c's IWM
    static constexpr uint16_t  kUnreleasedStateVersion   = 4;   // never released, its IWM stamps absolute

private:
    HRESULT  LoadParts (StateReader & reader, uint16_t version, Byte quarterTrack);

    int                  m_slot     = 0;
    Word                 m_ioStart  = 0;
    Word                 m_ioEnd    = 0;
    bool                 m_iwmMode  = false;
    IDiskPortHook      * m_portHook = nullptr;

    //  The lines, the card and the IWM port have references to the parts
    //  declared above them.
    Disk2Drives          m_drives;
    Disk2Signals         m_signals;
    Disk2Recorder        m_recorder;
    Disk2Clock           m_clock;
    Disk2Lines           m_lines    { m_drives, m_recorder, m_signals };
    Disk2CardSequencer   m_card     { m_lines, m_drives, m_signals, m_recorder, m_clock };
    Disk2IwmPort         m_iwmPort  { m_lines, m_drives, m_signals, m_recorder, m_clock };
};
