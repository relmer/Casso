#include "Pch.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"

#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





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

    // AttachInternalCxxxRom re-points the $C100-$CFFF page table; SetRomData
    // swapped the LC ROM buffer, so re-point its $D000-$FFFF read window too
    // (the page-table entries otherwise reference the previous bank's bytes).
    m_lc.RebindWindow();

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





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cRomBank::TryPatch
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cRomBank::TryPatch (Word address, Byte value)
{
    HRESULT         hr      = S_OK;
    bool            patched = false;
    bool            inCxxx  = address >= kCxxxStart  && address <  kCxxxStart + kCxxxSize;
    bool            inLcRom = address >= kLcRomStart;
    size_t          offset  = 0;
    vector<Byte>  & image   = m_bank[m_current];



    BAIL_OUT_IF (!inCxxx && !inLcRom, S_OK);

    offset = inCxxx ? kCxxxOffset  + (size_t) (address - kCxxxStart)
                    : kLcRomOffset + (size_t) (address - kLcRomStart);

    BAIL_OUT_IF (offset >= image.size(), S_OK);

    image[offset] = value;
    patched       = inCxxx ? m_mmu.GetCxxxRouter()->TryPatch (address, value)
                           : m_lc.TryPatchRom (address, value);

Error:
    return patched;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple2cRomBank::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte (static_cast<Byte> (m_current));

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  Fails with ERROR_INVALID_DATA for a bank other than 0 or 1.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple2cRomBank::LoadState (StateReader & reader)
{
    constexpr Byte  kBankCount = static_cast<Byte> (ARRAYSIZE (m_bank));
    HRESULT         hr         = S_OK;
    uint16_t        version    = 0;
    Byte            bank       = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte (bank);

    hr = reader.EndSection();
    CHR (hr);

    CBREx (bank < kBankCount, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    ApplyBank (bank);

Error:
    return hr;
}
