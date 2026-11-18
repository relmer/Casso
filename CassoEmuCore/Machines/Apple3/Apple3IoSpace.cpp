#include "Pch.h"

#include "Apple3IoSpace.h"
#include "Apple3AnalogInputs.h"
#include "Apple3DiskController.h"
#include "Apple3Keyboard.h"
#include "Apple3Mmu.h"
#include "Apple3Sound.h"
#include "Apple3Slots.h"
#include "Apple3Video.h"
#include "Core/MemoryBus.h"
#include "Devices/Acia6551.h"
#include "Devices/Mm58167.h"





static constexpr Word  s_kLastKeyboard   = 0xC01F;
static constexpr Word  s_kFirstSwitchReg = 0xC060;
static constexpr Word  s_kLastSwitchReg  = 0xC06F;
static constexpr Word  s_kSlot2Request   = 0xC064;
static constexpr Word  s_kSlot1Request   = 0xC065;
static constexpr Word  s_kAdTimeout      = 0xC066;
static constexpr Word  s_kAciaMask       = 0x0003;
static constexpr Word  s_kFirstSpeaker   = 0xC030;
static constexpr Word  s_kLastSpeaker    = 0xC03F;
static constexpr Word  s_kFirstBeeper    = 0xC040;
static constexpr Word  s_kLastBeeper     = 0xC04D;
static constexpr Word  s_kFirstClock     = 0xC070;
static constexpr Word  s_kLastClock      = 0xC07F;
static constexpr Word  s_kFirstAcia      = 0xC0F0;
static constexpr Word  s_kLastAcia       = 0xC0FF;
static constexpr Byte  s_kBit7           = 0x80;
static constexpr int   s_kSlot1          = 1;
static constexpr int   s_kSlot2          = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3IoSpace
//
////////////////////////////////////////////////////////////////////////////////

Apple3IoSpace::Apple3IoSpace (MemoryBus & bus, Apple3Mmu & mmu, Apple3Keyboard & keyboard, Apple3Video & video, Apple3Slots & slots, Apple3DiskController & disk,
                              Apple3AnalogInputs & analog, Acia6551 & acia, Mm58167 & clock, Apple3Sound & sound)
    : m_bus      (bus),
      m_mmu      (mmu),
      m_keyboard (keyboard),
      m_video    (video),
      m_slots    (slots),
      m_disk     (disk),
      m_analog   (analog),
      m_acia     (acia),
      m_clock    (clock),
      m_sound    (sound)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3IoSpace::Read (Word address)
{
    return Access (address, false, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void Apple3IoSpace::Write (Word address, Byte value)
{
    Access (address, true, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Access
//
//  Routes a read or a write to the device at the address. A switch changes
//  on either; a write returns nothing.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3IoSpace::Access (Word address, bool isWrite, Byte value)
{
    Byte  result = m_bus.ReadFloatingBus();



    if (IsSlowAddress (address))
    {
        m_slowAccesses++;
    }

    if (address <= s_kLastKeyboard)
    {
        if (isWrite)
        {
            m_keyboard.Write (address, value);
        }
        else
        {
            result = m_keyboard.Read (address);
        }
    }
    else if (m_video.IsSwitchAddress (address))
    {
        m_video.HandleSwitch (address);
    }
    else if (m_analog.IsSwitchAddress (address))
    {
        m_analog.HandleSwitch (address);
    }
    else if (address >= s_kFirstAcia && address <= s_kLastAcia)
    {
        Word  register_ = (Word) (s_kFirstAcia | (address & s_kAciaMask));



        if (isWrite)
        {
            m_acia.Write (register_, value);
        }
        else
        {
            result = m_acia.Read (register_);
        }
    }
    else if (address >= s_kFirstSpeaker && address <= s_kLastSpeaker)
    {
        (void) m_sound.GetSpeaker().Read (address);
    }
    else if (address >= s_kFirstBeeper && address <= s_kLastBeeper)
    {
        m_sound.TriggerBeep();
    }
    else if (address >= s_kFirstClock && address <= s_kLastClock)
    {
        Byte  reg = (Byte) (m_mmu.GetZeroPage() & (Mm58167::kRegisterCount - 1));



        if (isWrite)
        {
            m_clock.Write (reg, value);
        }
        else
        {
            result = m_clock.Read (reg);
        }
    }
    else if (m_disk.IsControllerAddress (address))
    {
        if (isWrite)
        {
            m_disk.Write (address, value);
        }
        else
        {
            Byte  disk = m_disk.Read (address);



            result = (address >= Apple3DiskController::kFirstRegister) ? disk : result;
        }
    }
    else if (m_slots.IsSlotAddress (address))
    {
        if (isWrite)
        {
            m_slots.Write (address, value);
        }
        else
        {
            result = m_slots.Read (address);
        }
    }
    else if (address >= s_kFirstSwitchReg && address <= s_kLastSwitchReg)
    {
        result = ReadSwitch (address);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadSwitch
//
//  The switch register returns its input in bit 7: $C060-$C063 the joystick
//  switches, 1 while pressed; $C064 and $C065, 0 while slot 2 or slot 1
//  requests an interrupt; and $C066, 1 while the A/D ramp runs.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3IoSpace::ReadSwitch (Word address) const
{
    Byte  value     = (Byte) (m_bus.ReadFloatingBus() & ~s_kBit7);
    bool  isRequest = address == s_kSlot2Request || address == s_kSlot1Request;
    bool  isLow     = (address == s_kSlot2Request && m_slots.IsRequesting (s_kSlot2))
                   || (address == s_kSlot1Request && m_slots.IsRequesting (s_kSlot1));
    bool  isHigh    = (isRequest && !isLow)
                   || m_analog.IsButtonPressed (address)
                   || (address == s_kAdTimeout && m_analog.IsTiming());



    return isHigh ? (Byte) (value | s_kBit7) : value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSlowAddress
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3IoSpace::IsSlowAddress (Word address) const
{
    bool  isClock = address >= s_kFirstClock && address <= s_kLastClock;
    bool  isAcia  = address >= s_kFirstAcia  && address <= s_kLastAcia;



    return isClock || isAcia;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekIo
//
//  The keyboard and the switch register read without side effects; the
//  rest change state on any access, so a peek reads nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3IoSpace::TryPeekIo (Word address, Byte & value)
{
    bool  isPeeked = false;



    if (address <= s_kLastKeyboard)
    {
        isPeeked = m_keyboard.TryPeekIo (address, value);
    }
    else if (address >= s_kFirstSwitchReg && address <= s_kLastSwitchReg)
    {
        value    = ReadSwitch (address);
        isPeeked = true;
    }
    else if (address >= s_kFirstClock && address <= s_kLastClock)
    {
        value    = m_clock.Peek ((Byte) (m_mmu.GetZeroPage() & (Mm58167::kRegisterCount - 1)));
        isPeeked = true;
    }

    return isPeeked;
}
