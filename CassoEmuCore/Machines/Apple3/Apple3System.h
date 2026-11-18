#pragma once

#include "Pch.h"

#include "Apple3AnalogInputs.h"
#include "Apple3CharacterSet.h"
#include "Apple3DiskController.h"
#include "Apple3IoSpace.h"
#include "Apple3Keyboard.h"
#include "Apple3Mmu.h"
#include "Apple3Slots.h"
#include "Apple3Sound.h"
#include "Apple3SystemVias.h"
#include "Apple3TimeBase.h"
#include "Apple3Video.h"
#include "Devices/Acia6551.h"
#include "Devices/Mm58167.h"

class IMachineState;
class InterruptController;
class MemoryBus;
class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3System
//
//  The Apple ///'s parts and their wiring: the memory map, the two VIAs, the
//  slots, the time base, the character set, the display, the keyboard, the
//  floppy controller, the A/D and joystick switches, the 6551 and the I/O
//  space that routes $C000-$CFFF to them.
//
//  After each instruction the CPU's cycles are charged to the time base,
//  in ticks, and the devices that count time are brought to it: the VIAs at
//  one count per 14 master clock periods, the 6551 at the same rate, the
//  floppy controller, the beam
//  line for vertical blanking, the keyboard's interrupt line, and the IRQ
//  line, which E VIA port A bit 7 reads back.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3System : public IMachineState
{
public:
    static constexpr uint32_t  kMasterPerViaCycle = 14;
    static constexpr Word      kRomStart          = 0xF000;
    static constexpr Word      kAciaAddress       = 0xC0F0;
    static constexpr uint32_t  kViaClockHz        = 1022727;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'S', 'Y');
    static constexpr uint16_t  kStateVersion = 1;

    explicit Apple3System (MemoryBus & bus);

    HRESULT   Initialize         (InterruptController & interrupts, size_t ramBytes, const std::vector<Byte> & rom);

    //  The instruction just run at `pc` took `cycles` CPU cycles. Returns
    //  their cost in ticks and brings the devices to the new time.
    uint32_t  OnInstruction      (uint32_t cycles, Word pc);

    void      PowerOn            (Prng & prng);
    void      Reset              ();

    //  Control with Reset is a system reset while environment bit 4 is set.
    bool      IsResetKeyEnabled  () const { return (m_mmu.GetEnvironment() & Apple3Mmu::kEnvResetEnable) != 0; }

    void      GetStateParts      (std::vector<IMachineState *> & outParts);

    //  The master clock count the VIAs and the 6551 were last run to. The
    //  parts are separate sections.
    HRESULT   SaveState          (StateWriter & writer) const override;
    HRESULT   LoadState          (StateReader & reader) override;

    Apple3Mmu             & GetMmu            () { return m_mmu; }
    Apple3SystemVias      & GetVias           () { return m_vias; }
    Apple3Slots           & GetSlots          () { return m_slots; }
    Apple3TimeBase        & GetTimeBase       () { return m_timeBase; }
    Apple3CharacterSet    & GetCharacterSet   () { return m_characterSet; }
    Apple3Video           & GetVideo          () { return m_video; }
    Apple3Keyboard        & GetKeyboard       () { return m_keyboard; }
    Apple3DiskController  & GetDiskController () { return m_disk; }
    Apple3AnalogInputs    & GetAnalogInputs   () { return m_analog; }
    Acia6551              & GetAcia           () { return m_acia; }
    Mm58167               & GetClock          () { return m_clock; }
    Apple3Sound           & GetSound          () { return m_sound; }
    Apple3IoSpace         & GetIoSpace        () { return m_io; }

private:
    void          SyncDevices    ();
    Apple3Access  GetFetchAccess (Word pc) const;

    MemoryBus             & m_bus;
    Apple3Mmu               m_mmu;
    Apple3SystemVias        m_vias;
    Apple3Slots             m_slots;
    Apple3TimeBase          m_timeBase;
    Apple3CharacterSet      m_characterSet;
    Apple3Video             m_video;
    Apple3Keyboard          m_keyboard;
    Apple3DiskController    m_disk;
    Apple3AnalogInputs      m_analog;
    Acia6551                m_acia;
    Mm58167                 m_clock;
    Apple3Sound             m_sound;
    Apple3IoSpace           m_io;
    InterruptController   * m_interrupts      = nullptr;
    uint64_t                m_viaMasterClocks = 0;
};
