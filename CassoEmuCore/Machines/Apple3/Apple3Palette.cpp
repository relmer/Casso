#include "Pch.h"

#include "Apple3Palette.h"
#include "Machines/Apple2/Common/NtscColorTable.h"





static constexpr Byte      s_kColorMask   = 0x0F;
static constexpr uint32_t  s_kGrayStep    = 0x11;
static constexpr uint32_t  s_kOpaque      = 0xFF000000;
static constexpr int       s_kGreenShift  = 8;
static constexpr int       s_kRedShift    = 16;





////////////////////////////////////////////////////////////////////////////////
//
//  GetPixel
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Apple3Palette::GetPixel (Byte color, bool isMonochrome)
{
    Byte      index = (Byte) (color & s_kColorMask);
    uint32_t  gray  = index * s_kGrayStep;



    return isMonochrome ? (s_kOpaque | (gray << s_kRedShift) | (gray << s_kGreenShift) | gray)
                        : kAppleColors[index];
}
