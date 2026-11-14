#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Devices/Floppy35/DeviceCpu.h"
#include "Devices/Floppy35/IDiskDriveBus.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"
#include "Devices/Floppy35/Iwm.h"
#include "Devices/Floppy35/SmartPortBus.h"
#include "Devices/Floppy35/Sony35Drive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UniDisk35Drive
//
//  The UniDisk 3.5: a 65C02 running Apple's 8 KB drive ROM, a gate array, an
//  IWM and a Sony 800K mechanism, on a SmartPort chain. The CPU's map:
//
//      $0000-$07FF   2 KB of RAM
//      $0800, $0801  the gate array's two registers
//      $0A00-$0A0F   the IWM
//      $E000-$FFFF   the ROM
//
//  Gate array register 0, written: bit 3 ENBUS puts /ACK and the drive's
//  data on the bus, bit 2 PH3EN passes the bus enable to the next device,
//  bit 1 IWMDIR connects the IWM to the bus (1) or the mechanism (0), bit 0
//  HDSEL selects the head. Read: bit 4 set for the last device on the
//  chain, bit 3 clear while the bus is enabled at this device, bit 1 clear
//  while the IWM's drive enable is on, bit 0 HDSEL.
//
//  Register 1, written: bit 4 low resets the IWM, bits 3 and 2 low clear the
//  button latches, bits 1 and 0 select drive 1 and drive 2. Read: bit 4 the
//  sense line, bits 3 and 2 the latches, bit 1 the bus enable, bit 0 phase 0.
//
//  With IWMDIR set, the IWM's phase 0 drives /ACK and its sense line reads
//  REQ, and its data goes to and from the bus at 4 microsecond cells. With
//  it clear and drive 1 selected, the IWM drives the mechanism. Drive 2,
//  for the DuoDisk's second mechanism, reads as not installed.
//
//  The CPU runs at 2 MHz, brought up to the host's time by the bus.
//
////////////////////////////////////////////////////////////////////////////////

class UniDisk35Drive : public MemoryDevice, public ISmartPortDevice, public IDiskDriveBus, public IFloppy35EventSource
{
public:
    static constexpr uint32_t  kClockHz = 2'000'000;
    static constexpr size_t    kRomSize = 0x2000;

    UniDisk35Drive();
    ~UniDisk35Drive() override;

    HRESULT  SetFirmware (std::vector<Byte> firmware);
    bool     HasFirmware () const { return !m_rom.empty(); }

    Sony35Drive  & GetMechanism () { return m_mechanism; }
    DeviceCpu    & GetCpu       () { return m_cpu; }
    Iwm          & GetIwm       () { return m_iwm; }

    // MemoryDevice: the drive CPU's whole map.
    Byte  Read     (Word address) override;
    void  Write    (Word address, Byte value) override;
    Word  GetStart () const override { return 0x0000; }
    Word  GetEnd   () const override { return 0xFFFF; }
    void  Reset    () override;

    // ISmartPortDevice
    void       Connect      (SmartPortBus * bus, size_t position) override;
    void       CatchUp      (uint64_t nowNs) override;
    void       SetBusReset  (bool isReset) override;
    bool       IsDrivingBus () const override { return (m_reg0 & kEnBus) != 0; }
    bool       GetAck       () const override;
    bool       PassesEnable () const override { return (m_reg0 & kPh3En) != 0; }
    BitLine  & GetInput     () override { return m_input; }
    BitLine  & GetOutput    () override { return m_output; }

    // IDiskDriveBus: the IWM's side, to the bus or the mechanism.
    void      SetPhases (bool ph0, bool ph1, bool ph2, bool ph3) override;
    bool      GetSense  () const override;
    Byte      ReadCell  (uint64_t cellEndNs) override;
    void      WriteCell (Byte bit, uint64_t cellEndNs) override;
    void      Advance   (uint64_t elapsedNs) override { m_mechanism.Advance (elapsedNs); }
    uint32_t  GetCellNs () const override;

    // IFloppy35EventSource
    void  SetFloppy35EventSink (IFloppy35EventSink * sink) override { m_mechanism.SetFloppy35EventSink (sink); }

private:
    static constexpr Byte      kEnBus      = 0x08;
    static constexpr Byte      kPh3En      = 0x04;
    static constexpr Byte      kIwmDir     = 0x02;
    static constexpr Byte      kHdSel      = 0x01;
    static constexpr Byte      kRstIwm     = 0x10;
    static constexpr Byte      kClear1     = 0x08;
    static constexpr Byte      kClear2     = 0x04;
    static constexpr Byte      kDrive1     = 0x02;
    static constexpr Byte      kReg1Reset  = 0x1C;
    static constexpr size_t    kRamSize    = 0x800;
    static constexpr Word      kRomStart   = 0xE000;
    static constexpr uint64_t  kNsPerCycle = 1'000'000'000 / kClockHz;
    static constexpr uint64_t  kMaxCatchUp = kClockHz;

    bool      IsOnBus           () const { return (m_reg0 & kIwmDir) != 0; }
    bool      IsMechanismActive () const { return !IsOnBus() && (m_reg1 & kDrive1) != 0; }
    bool      IsBusEnabled      () const;
    uint64_t  GetNowNs          () const { return m_cpu.GetCycles() * kNsPerCycle; }
    Byte      ReadGateArray     (Word address) const;
    void      WriteGateArray    (Word address, Byte value);
    void      MapPages          ();

    DeviceCpu             m_cpu;
    Iwm                   m_iwm;
    Sony35Drive           m_mechanism { 0 };
    std::vector<Byte>     m_ram;
    std::vector<Byte>     m_rom;
    BitLine               m_input;
    BitLine               m_output;
    SmartPortBus        * m_bus       = nullptr;
    size_t                m_position  = 0;
    std::array<bool, 4>   m_phases    = {};
    Byte                  m_reg0      = 0;
    Byte                  m_reg1      = kReg1Reset;
    bool                  m_latch1    = false;
    bool                  m_latch2    = false;
    bool                  m_resetHeld = false;
};
