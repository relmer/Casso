#include "Pch.h"

#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Apple2cPlus/MigChip.h"




static constexpr Word  s_kIwmReset     = 0xCC40;
static constexpr Word  s_kDriveOnLow   = 0xCC80;
static constexpr Word  s_kDriveOnHigh  = 0xCC9F;
static constexpr Word  s_kDriveOffLow  = 0xCCC0;
static constexpr Word  s_kDriveOffHigh = 0xCCDF;
static constexpr Word  s_kWindowLow    = 0xCE00;
static constexpr Word  s_kWindowHigh   = 0xCE1F;
static constexpr Word  s_kAdvanceLow   = 0xCE20;
static constexpr Word  s_kAdvanceHigh  = 0xCE3F;
static constexpr Word  s_kHeadZeroLow  = 0xCE40;
static constexpr Word  s_kHeadZeroHigh = 0xCE5F;
static constexpr Word  s_kHeadOneLow   = 0xCE60;
static constexpr Word  s_kHeadOneHigh  = 0xCE7F;
static constexpr Word  s_kPageReset    = 0xCEA0;
static constexpr Word  s_kWindowMask   = 0x1F;





////////////////////////////////////////////////////////////////////////////////
//
//  OnRomWindowAccess
//
////////////////////////////////////////////////////////////////////////////////

bool MigChip::OnRomWindowAccess (Word address, bool isWrite, Byte value, Byte & outValue)
{
    bool  handled = HandlesPage (address >> 8);



    Sync();

    if (!handled || !m_wasActive)
    {
        return false;
    }

    return Access (address, isWrite, value, outValue);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void MigChip::Reset()
{
    m_pointer    = 0;
    m_internalOn = false;
    m_sel35      = false;
    m_headOne    = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsActive
//
////////////////////////////////////////////////////////////////////////////////

bool MigChip::IsActive() const
{
    return m_bank == nullptr || m_bank->GetCurrentBank() == kAlternate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sync
//
//  Switching the main bank back in clears the pointer, the internal drive
//  and 3.5 select.
//
////////////////////////////////////////////////////////////////////////////////

void MigChip::Sync()
{
    bool  active = IsActive();



    if (m_wasActive && !active)
    {
        m_pointer    = 0;
        m_internalOn = false;
        m_sel35      = false;
    }

    m_wasActive = active;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Access
//
//  Returns false for an address that reads the floating bus, so the owner
//  supplies it.
//
////////////////////////////////////////////////////////////////////////////////

bool MigChip::Access (Word address, bool isWrite, Byte value, Byte & outValue)
{
    bool  inWindow  = address >= s_kWindowLow  && address <= s_kWindowHigh;
    bool  isAdvance = address >= s_kAdvanceLow && address <= s_kAdvanceHigh;
    Word  at        = (Word) (m_pointer + (address & s_kWindowMask));



    if (inWindow || isAdvance)
    {
        if (isWrite)
        {
            m_ram[at] = value;
        }

        outValue = m_ram[at];

        if (isAdvance)
        {
            m_pointer = (Word) ((m_pointer + kPageStep) & kPointerMask);
        }

        return true;
    }

    if (address == s_kPageReset)
    {
        m_pointer = 0;
    }
    else if (address >= s_kHeadZeroLow && address <= s_kHeadZeroHigh)
    {
        m_headOne = isWrite ? m_headOne : false;
        m_sel35   = isWrite ? false : m_sel35;
    }
    else if (address >= s_kHeadOneLow && address <= s_kHeadOneHigh)
    {
        m_headOne = isWrite ? m_headOne : true;
        m_sel35   = isWrite ? true : m_sel35;
    }
    else if (isWrite && address == s_kIwmReset && m_iwmReset)
    {
        m_iwmReset();
    }
    else if (isWrite && address >= s_kDriveOnLow && address <= s_kDriveOnHigh)
    {
        m_internalOn = true;
    }
    else if (isWrite && address >= s_kDriveOffLow && address <= s_kDriveOffHigh)
    {
        m_internalOn = false;
    }

    return false;
}
