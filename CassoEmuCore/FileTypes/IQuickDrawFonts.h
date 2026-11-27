#pragma once

#include "Pch.h"


class QuickDrawFont;





////////////////////////////////////////////////////////////////////////////////
//
//  IQuickDrawFonts
//
//  The fonts a QuickDraw II picture's text is drawn in, found by family
//  number and size. Find returns the font of that family nearest the size,
//  or nullptr when the family isn't available.
//
////////////////////////////////////////////////////////////////////////////////

class IQuickDrawFonts
{
public:
    virtual ~IQuickDrawFonts() = default;

    virtual const QuickDrawFont * Find (Word family, int size) const = 0;
};
