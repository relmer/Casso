#pragma once

#include "Pch.h"

#include "FileTypes/IQuickDrawFonts.h"
#include "FileTypes/QuickDrawFont.h"





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawFontLibrary
//
//  The $C8 fonts read from IIgs disks. AddVolume adds every font in a
//  ProDOS volume's System/Fonts folder. Find returns the font of the family
//  at the size, or else the nearest size, preferring the smaller of two
//  equally near.
//
////////////////////////////////////////////////////////////////////////////////

class QuickDrawFontLibrary : public IQuickDrawFonts
{
public:
    static constexpr Byte  kFontType = 0xC8;

    bool     Add       (std::span<const Byte> bytes);
    HRESULT  AddVolume (const vector<Byte> & blocks);

    size_t   GetCount  () const { return m_fonts.size(); }

    const QuickDrawFont * Find (Word family, int size) const override;

private:
    std::vector<QuickDrawFont>  m_fonts;
};
