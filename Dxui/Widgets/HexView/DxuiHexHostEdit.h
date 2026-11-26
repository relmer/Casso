#pragma once

#include "Pch.h"
#include "Widgets/HexView/DxuiHexViewTypes.h"


class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexHostEdit
//
//  Edit mode for a hex view whose host applies the typing. An editable view
//  reserves room for the edit frame inside its bounds, so it doesn't move
//  when editing starts or stops, and draws the edit look: the accent behind
//  the caret byte while the view has the keys, changed bytes in red, and
//  while editing, a light accent tint behind the bytes and an accent frame
//  around them. In edit mode, a hex digit typed in the hex column sets the
//  caret byte's high nibble, the next its low nibble, and the caret moves
//  on; a character typed in the text column replaces the caret byte and the
//  caret moves on.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHexHostEdit
{
public:
    using ByteStyle     = DxuiHexViewTypes::ByteStyle;
    using ByteStyleFn   = DxuiHexViewTypes::ByteStyleFn;
    using Column        = DxuiHexViewTypes::Column;
    using EditColors    = DxuiHexViewTypes::EditColors;
    using EditByteState = DxuiHexViewTypes::EditByteState;

    //  What a typed character did: nothing, the high nibble of the caret
    //  byte, or the whole byte, so the caret moves on.
    enum class TypeResult { NotTaken, HighNibble, Byte };

    static constexpr int    kEditFrameDip = 2;       // the edit frame, reserved whether or not it's drawn
    static constexpr float  kTintShare    = 0.12f;   // the accent's share of the edit tint behind the bytes
    static constexpr float  kMinContrast  = 4.5f;    // the least contrast of a changed byte's ink with its fill

    void  SetEditable  (bool isEditable)               { m_isEditable = isEditable; }
    void  SetEditing   (bool isEditing)                { m_isLowNibble = m_isLowNibble && isEditing == m_isEditing; m_isEditing = isEditing; }
    void  SetOnType    (DxuiHexViewTypes::TypeFn fn)    { m_onType    = std::move (fn); }
    void  SetIsChanged (DxuiHexViewTypes::ChangedFn fn) { m_isChanged = std::move (fn); }
    void  ResetNibble  ()                              { m_isLowNibble = false; }

    bool  IsEditable   () const                        { return m_isEditable; }
    bool  IsEditing    () const                        { return m_isEditing; }
    bool  IsTypingAt   (uint64_t offset) const         { return m_isLowNibble && m_nibbleAt == offset; }

    TypeResult  Type         (uint64_t caret, bool isHexColumn, wchar_t ch);
    bool        GetByteStyle (const IDxuiTheme & theme, const ByteStyleFn & hostStyle, uint64_t offset, uint8_t mark, bool isSelected,
                              Column column, bool isCaret, ByteStyle & outStyle) const;

    static bool      PickEditStyle  (const EditColors & colors, const EditByteState & state, ByteStyle & outStyle);
    static uint32_t  GetReadableInk (uint32_t ink, uint32_t fill);
    static uint32_t  GetChangedInk  (uint32_t background);

private:
    DxuiHexViewTypes::TypeFn     m_onType;
    DxuiHexViewTypes::ChangedFn  m_isChanged;
    uint64_t                     m_nibbleAt    = 0;
    bool                         m_isEditable  = false;
    bool                         m_isEditing   = false;
    bool                         m_isLowNibble = false;
};
