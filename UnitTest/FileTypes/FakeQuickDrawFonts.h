#pragma once

#include "Pch.h"

#include "FileTypes/IQuickDrawFonts.h"
#include "FileTypes/QuickDrawFont.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeQuickDrawFonts
//
//  One font, found for its family at any size.
//
////////////////////////////////////////////////////////////////////////////////

class FakeQuickDrawFonts : public IQuickDrawFonts
{
public:
    QuickDrawFont  font;

    const QuickDrawFont * Find (Word family, int size) const override
    {
        UNREFERENCED_PARAMETER (size);

        return (family == font.GetFamily()) ? &font : nullptr;
    }
};
