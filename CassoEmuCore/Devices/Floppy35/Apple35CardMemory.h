#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Devices/Floppy35/Iwm.h"
#include "Devices/Floppy35/Sony35Drive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple35CardMemory
//
//  The memory map of the Apple II 3.5 Disk Controller Card's 65C02:
//
//      $0000-$7FFF   32 KB of RAM, shared with the Apple through windows
//      $0A00-$0A0F   the SWIM, in its IWM mode
//      $0A40, $0A41  head select low and high, the drives' SEL line
//      $0A80, $0A81  the diagnostic LED off and on
//      $8000-$FFFF   the 32 KB ROM, 341-0438-A
//
//  The I/O page takes the place of RAM at $0A00-$0AFF. The SWIM's clock is
//  the card CPU's cycle count, at 2.046 MHz.
//
////////////////////////////////////////////////////////////////////////////////

class Apple35CardMemory : public MemoryDevice
{
public:
    static constexpr size_t  kRamSize = 0x8000;
    static constexpr size_t  kRomSize = 0x8000;
    static constexpr int     kDrives  = 2;

    Apple35CardMemory();

    Byte  Read     (Word address) override;
    void  Write    (Word address, Byte value) override;
    Word  GetStart () const override { return 0x0000; }
    Word  GetEnd   () const override { return 0xFFFF; }
    void  Reset    () override;

    HRESULT  SetFirmware (std::vector<Byte> firmware);
    bool     HasFirmware () const { return !m_rom.empty(); }

    void  SetCycleSource (const uint64_t * cycles) { m_cycles = cycles; }

    std::vector<Byte>  & GetRam   ()          { return m_ram; }
    std::vector<Byte>  & GetRom   ()          { return m_rom; }
    Sony35Drive        & GetDrive (int index) { return *m_drives[(size_t) index]; }
    Iwm                & GetIwm   ()          { return m_iwm; }
    bool                 IsLedOn  () const    { return m_led; }

private:
    static constexpr Word  kIoPage     = 0x0A00;
    static constexpr Word  kIoPageEnd  = 0x0AFF;
    static constexpr Word  kSwimEnd    = 0x0A0F;
    static constexpr Word  kHeadLow    = 0x0A40;
    static constexpr Word  kHeadHigh   = 0x0A41;
    static constexpr Word  kLedOff     = 0x0A80;
    static constexpr Word  kLedOn      = 0x0A81;
    static constexpr Word  kRomStart   = 0x8000;

    Byte      AccessIo  (Word address, bool isWrite, Byte value);
    uint64_t  GetNowNs  () const;

    std::vector<Byte>                            m_ram;
    std::vector<Byte>                            m_rom;
    Iwm                                          m_iwm;
    std::array<std::unique_ptr<Sony35Drive>, 2>  m_drives;
    const uint64_t                             * m_cycles = nullptr;
    bool                                         m_led    = false;
};
