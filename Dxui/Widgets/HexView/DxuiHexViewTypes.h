#pragma once

#include "Pch.h"

#include "Widgets/DxuiScrollbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewTypes
//
//  The types DxuiHexView and its parts share. DxuiHexView derives from it, so
//  each is also DxuiHexView::<type>.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiHexViewTypes
{
    enum class Column { None, Hex, Text };

    //  Which byte a point landed on, and in which column. `hit` is false for
    //  a point in the offset gutter or past the last row.
    struct HitResult
    {
        bool      hit    = false;
        uint64_t  offset = 0;
        Column    column = Column::None;
    };

    //  How the text column decodes a byte. Apple II text has the high bit set
    //  for ordinary characters and clear for inverse and flashing ones, so
    //  both values decode to the same character. DOS and Windows text decode
    //  bytes above 0x7F through code pages 437 and 1252, and DOS text decodes
    //  its control codes to code page 437's symbols for them.
    enum class TextEncoding { Ascii, AppleHighBit, Dos, Windows };

    //  A color for a marked byte, or false to leave it the ordinary one.
    using MarkColorFn = std::function<bool (uint8_t mark, uint32_t & outArgb)>;

    //  Asked for a context menu at a point in the same DIPs as the bounds.
    using ContextMenuFn = std::function<void (POINT atDip)>;

    //  How the value column reads each value: its bytes as one little-endian
    //  integer, in hex or in decimal. Without values only the text column
    //  shows.
    enum class ValueFormat { Hex, Signed, Unsigned };

    //  The view's own context menu, shown on a right-click when no handler
    //  above replaces it: how values show (text only, 1-, 2- or 4-byte
    //  integers), their format, the columns, and Copy. A host adds its own
    //  items through the build hook, and hears of a change to the view's
    //  settings through the settings hook, to keep them.
    using BuildMenuFn = std::function<void (std::vector<DxuiPopupMenuItem> & items)>;

    using SettingsFn  = std::function<void ()>;

    static constexpr int  s_kFixedRowWidth      = -1;
};
