#include "Pch.h"
#include "Apple2cRomBank.h"

#include "LanguageCard.h"
#include "Apple2eMmu.h"


static constexpr Word    s_kBankImageStart = 0xC000;      // each bank spans $C000-$FFFF
static constexpr size_t  s_kBankImageSize  = 0x4000;      // 16 KiB
static constexpr Word    s_kCxxxStart      = 0xC100;
static constexpr size_t  s_kCxxxSize       = 0x0F00;      // $C100-$CFFF
static constexpr Word    s_kLcRomStart     = 0xD000;
static constexpr size_t  s_kLcRomSize      = 0x3000;      // $D000-$FFFF
static constexpr size_t  s_kCxxxOffset     = s_kCxxxStart  - s_kBankImageStart;   // $0100
static constexpr size_t  s_kLcRomOffset    = s_kLcRomStart - s_kBankImageStart;   // $1000




////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cRomBank
//
////////////////////////////////////////////////////////////////////////////////

Apple2cRomBank::Apple2cRomBank (LanguageCard & lc, Apple2eMmu & mmu)
    : m_lc  (lc),
      m_mmu (mmu)
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetBankImages
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cRomBank::SetBankImages (vector<Byte> bank0, vector<Byte> bank1)
{
    m_bank[0] = std::move (bank0);
    m_bank[1] = std::move (bank1);

    ApplyBank (0);
}




////////////////////////////////////////////////////////////////////////////////
//
//  ApplyBank
//
//  Re-slices the selected 16K image into the CxxxRomRouter ($C100-$CFFF) and
//  the language card ($D000-$FFFF). A bank whose image is missing/short is
//  ignored so partial wiring cannot read past the buffer.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cRomBank::ApplyBank (int bank)
{
    const vector<Byte> &  image = m_bank[bank];

    if (image.size() < s_kBankImageSize)
    {
        return;
    }

    vector<Byte>   cxxx  (image.begin() + s_kCxxxOffset,  image.begin() + s_kCxxxOffset  + s_kCxxxSize);
    vector<Byte>   lcRom (image.begin() + s_kLcRomOffset, image.begin() + s_kLcRomOffset + s_kLcRomSize);

    m_mmu.AttachInternalCxxxRom (std::move (cxxx));
    m_lc.SetRomData             (lcRom);

    m_current = bank;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ToggleRomBank / ResetRomBank
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cRomBank::ToggleRomBank()
{
    ApplyBank (m_current ^ 1);
}


void Apple2cRomBank::ResetRomBank()
{
    ApplyBank (0);
}
