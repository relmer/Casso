#pragma once

#include "Pch.h"

struct DxuiPopupMenuItem;





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

    //  A byte the host draws its way in a column: the fill behind it and its
    //  ink, 0 for the view's, whether its ink is bold, and an outline around
    //  the fill, 0 for none. The function returns false to draw the byte as
    //  the view does.
    struct ByteStyle
    {
        uint32_t  fill    = 0;
        uint32_t  ink     = 0;
        bool      isBold  = false;
        uint32_t  outline = 0;
    };

    using ByteStyleFn = std::function<bool (uint64_t offset, uint8_t mark, bool isSelected, Column column, ByteStyle & outStyle)>;

    //  Edit mode. A typed character goes to the host, which returns true when
    //  the byte at `offset` changed; a hex digit sets the high nibble first.
    //  The changed function returns true for a byte the host has edited.
    using TypeFn    = std::function<bool (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch)>;
    using ChangedFn = std::function<bool (uint64_t offset, uint8_t mark)>;

    //  The theme's colors the edit look is made from.
    struct EditColors
    {
        uint32_t  accent     = 0;
        uint32_t  background = 0;
        uint32_t  foreground = 0;
    };

    //  What the edit look draws for a byte in one column.
    struct EditByteState
    {
        bool  isEditing   = false;   // the view is in edit mode
        bool  isCaret     = false;   // the caret byte, with the view focused
        bool  isTyping    = false;   // the byte's high nibble is typed and its low nibble isn't yet
        bool  isHexColumn = false;
        bool  isSelected  = false;
        bool  isChanged   = false;
    };

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
