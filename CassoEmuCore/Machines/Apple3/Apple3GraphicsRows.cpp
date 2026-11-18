#include "Pch.h"

#include "Apple3GraphicsRows.h"





static constexpr int   s_kGroupLines    = 8;
static constexpr int   s_kThirdGroups   = 8;
static constexpr Word  s_kLineStride    = 0x400;
static constexpr Word  s_kGroupStride   = 0x80;
static constexpr Word  s_kThirdStride   = 0x28;





////////////////////////////////////////////////////////////////////////////////
//
//  GetLineAddress
//
//  Smooth scroll moves each line within its 8-line group.
//
////////////////////////////////////////////////////////////////////////////////

Word Apple3GraphicsRows::GetLineAddress (int line, Word buffer, int scrollOffset)
{
    int  source = (line & ~(s_kGroupLines - 1)) | ((line + scrollOffset) % s_kGroupLines);
    int  group  = source / s_kGroupLines;



    return (Word) (buffer + (source % s_kGroupLines) * s_kLineStride
                          + (group % s_kThirdGroups) * s_kGroupStride
                          + (group / s_kThirdGroups) * s_kThirdStride);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  With no graphics source, as in a text-only test, every byte reads 0.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3GraphicsRows::Read (const Apple3VideoInput & input, Word address)
{
    return (input.readGraphics != nullptr) ? (*input.readGraphics) (address) : 0;
}