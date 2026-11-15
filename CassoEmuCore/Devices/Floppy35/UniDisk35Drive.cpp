#include "Pch.h"

#include "Devices/Floppy35/UniDisk35Drive.h"




static constexpr Word  s_kGateArrayStart = 0x0800;
static constexpr Word  s_kGateArrayEnd   = 0x09FF;
static constexpr Word  s_kIwmStart       = 0x0A00;
static constexpr Word  s_kIwmEnd         = 0x0BFF;
static constexpr Word  s_kIwmMask        = 0x0F;
static constexpr Byte  s_kLastOne        = 0x10;
static constexpr Byte  s_kBusDisabled    = 0x08;
static constexpr Byte  s_kWriteRequest   = 0x04;
static constexpr Byte  s_kGateOff        = 0x02;
static constexpr Byte  s_kSenseBit       = 0x10;
static constexpr Byte  s_kLatch1Bit      = 0x08;
static constexpr Byte  s_kLatch2Bit      = 0x04;
static constexpr Byte  s_kEnabledBit     = 0x02;
static constexpr Byte  s_kPhase0Bit      = 0x01;





////////////////////////////////////////////////////////////////////////////////
//
//  UniDisk35Drive
//
////////////////////////////////////////////////////////////////////////////////

UniDisk35Drive::UniDisk35Drive() :
    m_ram (kRamSize, 0)
{
    HRESULT  hr = S_OK;



    m_iwm.AttachDrive (0, this);

    hr = m_cpu.Initialize (*this);
    IGNORE_RETURN_VALUE (hr, S_OK);

    MapPages();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~UniDisk35Drive
//
////////////////////////////////////////////////////////////////////////////////

UniDisk35Drive::~UniDisk35Drive() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  SetFirmware
//
//  The drive's CPU starts its firmware once the firmware is in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UniDisk35Drive::SetFirmware (std::vector<Byte> firmware)
{
    HRESULT  hr     = S_OK;
    size_t   length = firmware.size();



    m_rom.clear();
    CBREx (length == kRomSize, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    m_rom = std::move (firmware);
    MapPages();
    Reset();

    if (IsIdleLoopInRom())
    {
        m_cpu.SetIdleLoop (kIdleLoop, kIdlePassCycles, [this] (uint64_t cyclesLeft) { return SkipIdleLoop (cyclesLeft); });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte UniDisk35Drive::Read (Word address)
{
    Byte  value = 0xFF;



    if (address < kRamSize)
    {
        value = m_ram[address];
    }
    else if (address >= s_kGateArrayStart && address <= s_kGateArrayEnd)
    {
        value = ReadGateArray (address);
    }
    else if (address >= s_kIwmStart && address <= s_kIwmEnd)
    {
        value = m_iwm.Access (address & s_kIwmMask, false, 0, GetNowNs());
    }
    else if (address >= kRomStart && !m_rom.empty())
    {
        value = m_rom[address - kRomStart];
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void UniDisk35Drive::Write (Word address, Byte value)
{
    if (address < kRamSize)
    {
        m_ram[address] = value;
    }
    else if (address >= s_kGateArrayStart && address <= s_kGateArrayEnd)
    {
        WriteGateArray (address, value);
    }
    else if (address >= s_kIwmStart && address <= s_kIwmEnd)
    {
        m_iwm.Access (address & s_kIwmMask, true, value, GetNowNs());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  The drive's RESET: the gate array's registers and the IWM clear, and the
//  CPU starts at the ROM's reset vector. RAM isn't cleared: the firmware
//  compares it with a signature to choose a warm or a cold start.
//
////////////////////////////////////////////////////////////////////////////////

void UniDisk35Drive::Reset()
{
    m_reg0 = 0;
    m_reg1 = kReg1Reset;
    m_iwm.Reset();
    m_input.Clear();
    m_output.Clear();
    m_cpu.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Connect
//
////////////////////////////////////////////////////////////////////////////////

void UniDisk35Drive::Connect (SmartPortBus * bus, size_t position)
{
    m_bus      = bus;
    m_position = position;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatchUp
//
//  Runs the CPU to the host's time. A drive in reset, or idle for more
//  than a second, skips the time instead: its firmware is waiting for the
//  bus either way.
//
////////////////////////////////////////////////////////////////////////////////

void UniDisk35Drive::CatchUp (uint64_t nowNs, CatchUpKind kind)
{
    uint64_t  target  = nowNs / kNsPerCycle;
    uint64_t  current = m_cpu.GetCycles();



    if (m_rom.empty() || target <= current)
    {
        return;
    }

    if (m_resetHeld)
    {
        m_cpu.SkipTo (target);
        return;
    }

    if (target - current > kMaxCatchUp)
    {
        m_cpu.SkipTo (target - kMaxCatchUp);
    }

    m_cpu.RunUntil (target, kind);

    //  The IWM's shift register moves between the CPU's accesses too, so
    //  its bits up to the host's time are on the bus before the host reads
    //  them. The CPU's last access is before the host's time, so the IWM's
    //  clock never runs ahead of the host's writes. A CPU in its idle loop
    //  short of the host's time takes the IWM only to the CPU's time.
    m_iwm.AdvanceTo (std::min (nowNs, GetNowNs()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SkipIdleLoop
//
//  The firmware's loop at $EC08 polls the bus. With its counter at $6A-$6B
//  zero, a pass writes the gate array's register 1, reads the button
//  latches and reads the IWM's status, the same 109 cycles each time. A
//  pass of 109 cycles since the last one at $EC08 indicates the CPU is in
//  that loop, as does a CPU left at $EC08 by the last skip. The latches and
//  the bus change only between catch-ups, and with the mechanism idle its
//  registers don't change with time, so every pass up to the target is the
//  same.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t UniDisk35Drive::SkipIdleLoop (uint64_t cyclesLeft)
{
    constexpr Word  kCounterLow  = 0x6A;
    constexpr Word  kCounterHigh = 0x6B;
    constexpr Byte  kLatchBits   = 0x0C;
    uint64_t        now          = m_cpu.GetCycles();
    bool            inLoop       = now - m_idleAt == kIdlePassCycles || now == m_idleAt;
    uint64_t        skipped      = 0;



    if (inLoop && m_ram[kCounterLow] == 0 && m_ram[kCounterHigh] == 0 && (ReadGateArray (0x0801) & kLatchBits) == 0 && m_mechanism.IsIdle())
    {
        skipped = cyclesLeft / kIdlePassCycles * kIdlePassCycles;
    }

    m_idleAt = now + skipped;

    return skipped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsIdleLoopInRom
//
//  Whether the ROM's bytes at the idle loop and the routines it calls are
//  the ones SkipIdleLoop is written for.
//
////////////////////////////////////////////////////////////////////////////////

bool UniDisk35Drive::IsIdleLoopInRom() const
{
    struct Piece
    {
        Word                     address;
        std::vector<Byte>        bytes;
    };

    static const Piece  s_kPieces[] =
    {
        { 0xEC08, { 0xA5, 0x6A, 0x05, 0x6B, 0xD0, 0x05, 0x20, 0xEA, 0xE9, 0x80, 0x06, 0xE6, 0x6A, 0xD0, 0x02, 0xE6, 0x6B, 0x20, 0xC3, 0xE5, 0xAD, 0x0D, 0x0A, 0xAD, 0x0E, 0x0A, 0x10, 0xE4 } },
        { 0xE9EA, { 0xA9, 0x03, 0x4C, 0x81, 0xE6 } },
        { 0xE5C3, { 0xAD, 0x01, 0x08, 0x29, 0x0C, 0xF0, 0x23 } },
        { 0xE5ED, { 0x60 } },
        { 0xE681, { 0xDA, 0xA2, 0x01, 0x48, 0x49, 0x0F, 0x09, 0xF0, 0x35, 0x09, 0x95, 0x09, 0x68, 0x4A, 0x4A, 0x4A, 0x4A, 0x15, 0x09, 0x95, 0x09, 0x9D, 0x00, 0x08, 0xFA, 0x60 } },
    };

    bool  matches = m_rom.size() == kRomSize;



    for (const Piece & piece : s_kPieces)
    {
        size_t  offset = (size_t) (piece.address - kRomStart);



        matches = matches && std::equal (piece.bytes.begin(), piece.bytes.end(), m_rom.begin() + (ptrdiff_t) offset);
    }

    return matches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetBusReset
//
//  The drive is in reset while the bus is; the end of the bus's reset
//  starts the firmware.
//
////////////////////////////////////////////////////////////////////////////////

void UniDisk35Drive::SetBusReset (bool isReset)
{
    if (m_resetHeld && !isReset)
    {
        Reset();
    }

    m_resetHeld = isReset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAck
//
//  /ACK follows phase 0 while the IWM is on the bus, and is low while the
//  IWM is on the mechanism.
//
////////////////////////////////////////////////////////////////////////////////

bool UniDisk35Drive::GetAck() const
{
    return IsOnBus() && m_phases[0];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPhases
//
////////////////////////////////////////////////////////////////////////////////

void UniDisk35Drive::SetPhases (bool ph0, bool ph1, bool ph2, bool ph3)
{
    m_phases = { ph0, ph1, ph2, ph3 };

    if (IsMechanismActive())
    {
        m_mechanism.SetPhases (ph0, ph1, ph2, ph3);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSense
//
//  REQ on the bus, the selected register of drive 1, or 1 for a drive that
//  isn't installed.
//
////////////////////////////////////////////////////////////////////////////////

bool UniDisk35Drive::GetSense() const
{
    bool  sense = true;



    if (IsOnBus())
    {
        sense = m_bus != nullptr && m_bus->GetReq() && IsBusEnabled();
    }
    else if (IsMechanismActive())
    {
        sense = m_mechanism.GetSense();
    }

    return sense;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadCell
//
////////////////////////////////////////////////////////////////////////////////

Byte UniDisk35Drive::ReadCell (uint64_t cellEndNs)
{
    Byte  bit = 0;



    if (IsOnBus())
    {
        bit = m_input.Read (cellEndNs, SmartPortBus::kCellNs);
        bit = IsBusEnabled() ? bit : (Byte) 0;
    }
    else if (IsMechanismActive())
    {
        bit = m_mechanism.ReadCell (cellEndNs);
    }

    return bit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetQuietUntil
//
//  On the bus, a cell reads 0 while the bus isn't enabled at the drive or
//  no bits from the Apple are waiting; the Apple's bits for a time are on
//  the line before the drive's IWM reads to that time.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t UniDisk35Drive::GetQuietUntil() const
{
    uint64_t  quiet = kQuietForever;



    if (IsOnBus())
    {
        quiet = (!IsBusEnabled() || m_input.IsEmpty()) ? kQuietForever : 0;
    }
    else if (IsMechanismActive())
    {
        quiet = m_mechanism.GetQuietUntil();
    }

    return quiet;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteCell
//
////////////////////////////////////////////////////////////////////////////////

void UniDisk35Drive::WriteCell (Byte bit, uint64_t cellEndNs)
{
    if (IsOnBus())
    {
        if (IsDrivingBus())
        {
            m_output.Write (bit, cellEndNs, SmartPortBus::kCellNs);
        }
    }
    else if (IsMechanismActive())
    {
        m_mechanism.WriteCell (bit, cellEndNs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCellNs
//
////////////////////////////////////////////////////////////////////////////////

uint32_t UniDisk35Drive::GetCellNs() const
{
    return IsOnBus() ? SmartPortBus::kCellNs : m_mechanism.GetCellNs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBusEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool UniDisk35Drive::IsBusEnabled() const
{
    return m_bus != nullptr && m_bus->IsEnabledAt (m_position);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadGateArray
//
////////////////////////////////////////////////////////////////////////////////

Byte UniDisk35Drive::ReadGateArray (Word address) const
{
    Byte  value  = 0;
    bool  isLast = m_bus == nullptr || m_bus->IsLastAt (m_position);



    if ((address & 1) == 0)
    {
        value = (Byte) ((isLast ? s_kLastOne : 0)                  |
                        (IsBusEnabled() ? 0 : s_kBusDisabled)       |
                        (m_iwm.IsWriting() ? s_kWriteRequest : 0)  |
                        (m_iwm.IsEnabled() ? 0 : s_kGateOff)       |
                        (m_reg0 & kHdSel));
    }
    else
    {
        value = (Byte) ((GetSense() ? s_kSenseBit : 0)        |
                        (m_latch1 ? s_kLatch1Bit : 0)         |
                        (m_latch2 ? s_kLatch2Bit : 0)         |
                        (IsBusEnabled() ? s_kEnabledBit : 0)  |
                        (m_phases[0] ? s_kPhase0Bit : 0));
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteGateArray
//
//  Connecting the IWM to the mechanism sets the mechanism's phases to the
//  IWM's.
//  Deselecting drive 1 writes its changed tracks to the image, as the
//  firmware deselects the drive once a command is done.
//
////////////////////////////////////////////////////////////////////////////////

void UniDisk35Drive::WriteGateArray (Word address, Byte value)
{
    bool  wasActive   = IsMechanismActive();
    bool  wasSelected = (m_reg1 & kDrive1) != 0;



    if ((address & 1) == 0)
    {
        m_reg0 = value;
        m_mechanism.SetSel ((value & kHdSel) != 0);
    }
    else
    {
        m_reg1 = value;

        if ((value & kRstIwm) == 0)
        {
            m_iwm.Reset();
        }

        m_latch1 = m_latch1 && (value & kClear1) != 0;
        m_latch2 = m_latch2 && (value & kClear2) != 0;
    }

    if (!wasActive && IsMechanismActive())
    {
        m_mechanism.SetPhases (m_phases[0], m_phases[1], m_phases[2], m_phases[3]);
    }

    if (wasSelected && (m_reg1 & kDrive1) == 0)
    {
        HRESULT  hr = m_mechanism.Flush();



        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MapPages
//
//  RAM and ROM are read straight from memory; the I/O pages go to the
//  device.
//
////////////////////////////////////////////////////////////////////////////////

void UniDisk35Drive::MapPages()
{
    constexpr int  kRamPages  = (int) (kRamSize / 0x100);
    constexpr int  kRomPage   = kRomStart >> 8;
    constexpr int  kRomPages  = (int) (kRomSize / 0x100);
    Byte         * rom        = m_rom.empty() ? nullptr : m_rom.data();



    m_cpu.MapPages (0,        kRamPages, m_ram.data(), m_ram.data());
    m_cpu.MapPages (kRomPage, kRomPages, rom,          nullptr);
}
