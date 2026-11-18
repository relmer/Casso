#include "Pch.h"

#include "Apple3System.h"
#include "Core/InterruptController.h"
#include "Core/MemoryBus.h"
#include "Core/Prng.h"





static constexpr Word  s_kFirstIo = 0xC000;
static constexpr Word  s_kLastIo  = 0xCFFF;
static constexpr Word  s_kGraphicsBase = 0x2000;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3System
//
////////////////////////////////////////////////////////////////////////////////

Apple3System::Apple3System (MemoryBus & bus)
    : m_bus   (bus),
      m_vias  (m_mmu),
      m_slots (bus, m_vias),
      m_video  (m_characterSet),
      m_analog (m_timeBase),
      m_acia   (kAciaAddress),
      m_io     (bus, m_mmu, m_keyboard, m_video, m_slots, m_disk, m_analog, m_acia, m_clock)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Builds the memory map over the bus, puts the I/O space and the VIAs
//  behind it, and wires the interrupt sources.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3System::Initialize (InterruptController & interrupts, size_t ramBytes, const std::vector<Byte> & rom)
{
    HRESULT  hr = S_OK;



    m_interrupts = &interrupts;

    hr = m_mmu.Initialize (&m_bus, ramBytes, rom);
    CHR (hr);

    m_mmu.AttachIo   (&m_io);
    m_mmu.AttachVias (&m_vias);
    m_bus.AddDevice  (&m_mmu);

    hr = m_vias.AttachInterruptController (&interrupts);
    CHR (hr);

    hr = m_acia.AttachInterruptController (&interrupts);
    CHR (hr);

    m_acia.SetCpuClockHz (kViaClockHz);

    m_keyboard.AttachVias (&m_vias);
    m_video.AttachVias    (&m_vias);
    m_video.SetSystemReader   ([this] (Word address) { return m_mmu.PeekSystem (address); });
    m_video.SetGraphicsReader ([this] (Word address) { return m_mmu.PeekPhysical (0, (Word) (address - s_kGraphicsBase)); });
    m_video.SetScrollSource   ([this] () { return (int) m_disk.GetDiskII().GetLines().GetPhases(); });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetFetchAccess
//
//  What most of an instruction's cycles access: its code and, usually, its
//  data. The ROM when it runs from the selected ROM, the I/O space when it
//  runs there, and RAM otherwise.
//
////////////////////////////////////////////////////////////////////////////////

Apple3Access Apple3System::GetFetchAccess (Word pc) const
{
    Byte          environment = m_mmu.GetEnvironment();
    bool          isRom       = (environment & Apple3Mmu::kEnvRomSelect) != 0 && pc >= kRomStart;
    bool          isIo        = (environment & Apple3Mmu::kEnvIoEnable) != 0 && pc >= s_kFirstIo && pc <= s_kLastIo;
    Apple3Access  access      = Apple3Access::Ram;



    if (isRom)
    {
        access = Apple3Access::Rom;
    }
    else if (isIo)
    {
        access = Apple3Access::Io;
    }

    return access;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnInstruction
//
//  The cycles that accessed the VIAs, the 6551 or the clock chip run at
//  1 MHz; the rest are charged as the instruction's fetch region.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Apple3System::OnInstruction (uint32_t cycles, Word pc)
{
    Byte      environment = m_mmu.GetEnvironment();
    uint32_t  slow        = std::min (cycles, m_vias.TakeAccessCount() + m_io.TakeSlowAccessCount());
    uint32_t  ticks       = 0;



    m_timeBase.SetOneMhz   ((environment & Apple3Mmu::kEnvSlowClock) != 0);
    m_disk.SetOneMhz       ((environment & Apple3Mmu::kEnvSlowClock) != 0);
    m_timeBase.SetScreenOn ((environment & Apple3Mmu::kEnvScreen) != 0);

    ticks  = m_timeBase.ChargeCycles (cycles - slow, GetFetchAccess (pc));
    ticks += m_timeBase.ChargeCycles (slow, Apple3Access::Fspace);

    SyncDevices();

    return ticks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncDevices
//
////////////////////////////////////////////////////////////////////////////////

void Apple3System::SyncDevices()
{
    uint64_t  master    = m_timeBase.GetMasterClocks();
    uint64_t  viaCycles = (master - m_viaMasterClocks) / kMasterPerViaCycle;



    m_viaMasterClocks += viaCycles * kMasterPerViaCycle;

    if (viaCycles > 0)
    {
        m_vias.Tick ((uint32_t) viaCycles);
        m_acia.Tick ((uint32_t) viaCycles);
    }

    m_disk.SyncToTicks (m_timeBase.GetTicks());

    m_video.SetScreenOn ((m_mmu.GetEnvironment() & Apple3Mmu::kEnvScreen) != 0);
    m_video.OnBeamLine  (m_timeBase.GetLine());

    m_keyboard.UpdateInterruptLine();

    // The clock chip's interrupt is E VIA CA1, active on a falling edge.
    m_vias.GetE().SetCa1 (!m_clock.IsInterruptAsserted());

    if (m_interrupts != nullptr)
    {
        m_vias.SetInterruptLine (m_interrupts->IsAnyAsserted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerOn
//
//  RAM and the character set come up with random contents, and the reset
//  line resets the VIAs, which selects the ROM.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3System::PowerOn (Prng & prng)
{
    m_mmu.PowerCycle (prng);
    m_characterSet.PowerOn (prng);
    m_disk.PowerOn (prng);
    m_clock.PowerOn();

    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Apple3System::Reset()
{
    m_vias.Reset();
    m_acia.Reset();
    m_disk.Reset();
    m_keyboard.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStateParts
//
////////////////////////////////////////////////////////////////////////////////

void Apple3System::GetStateParts (std::vector<IMachineState *> & outParts)
{
    outParts.push_back (&m_mmu);
    outParts.push_back (&m_vias);
    outParts.push_back (&m_slots);
    outParts.push_back (&m_timeBase);
    outParts.push_back (&m_characterSet);
    outParts.push_back (&m_video);
    outParts.push_back (&m_keyboard);
    outParts.push_back (&m_disk);
    outParts.push_back (&m_analog);
    outParts.push_back (&m_acia);
    outParts.push_back (&m_clock);
}
