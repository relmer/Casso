#include "Pch.h"
#include "Apple2cRomBank.h"

#include "LanguageCard.h"
#include "Apple2eMmu.h"




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

    if (image.size() < kBankImageSize)
    {
        return;
    }

    vector<Byte>   cxxx  (image.begin() + kCxxxOffset,  image.begin() + kCxxxOffset  + kCxxxSize);
    vector<Byte>   lcRom (image.begin() + kLcRomOffset, image.begin() + kLcRomOffset + kLcRomSize);

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
