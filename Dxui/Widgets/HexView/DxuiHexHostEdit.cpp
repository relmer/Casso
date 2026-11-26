#include "Pch.h"

#include "Widgets/HexView/DxuiHexHostEdit.h"
#include "Theme/DxuiColor.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexHostEdit::Type
//
//  In edit mode, the host is given the character for the caret byte, with
//  whether it's the high nibble. A character the host doesn't take changes
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexHostEdit::TypeResult DxuiHexHostEdit::Type (uint64_t caret, bool isHexColumn, wchar_t ch)
{
    bool  isHigh = false;



    if (!m_isEditing || !m_onType)
    {
        return TypeResult::NotTaken;
    }

    if (m_isLowNibble && m_nibbleAt != caret)
    {
        m_isLowNibble = false;
    }

    isHigh = isHexColumn && !m_isLowNibble;

    if (!m_onType (caret, isHexColumn, isHigh, ch))
    {
        return TypeResult::NotTaken;
    }

    if (isHigh)
    {
        m_isLowNibble = true;
        m_nibbleAt    = caret;
        return TypeResult::HighNibble;
    }

    m_isLowNibble = false;
    return TypeResult::Byte;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexHostEdit::GetByteStyle
//
//  A view that isn't editable draws the host's style. In an editable one,
//  the caret and a byte being typed are drawn in the edit look. Any other
//  byte the host styles is drawn in the host's style, with the edit tint
//  behind it when the style has no fill, and in the changed-byte ink when
//  the byte is changed.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexHostEdit::GetByteStyle (
    const IDxuiTheme   & theme,
    const ByteStyleFn  & hostStyle,
    uint64_t             offset,
    uint8_t              mark,
    bool                 isSelected,
    Column               column,
    bool                 isCaret,
    ByteStyle          & outStyle) const
{
    ByteStyle      host;
    ByteStyle      edit;
    EditColors     colors = { theme.Accent(), theme.ContentBackground(), theme.Foreground() };
    EditByteState  state;
    bool           isHost = hostStyle && hostStyle (offset, mark, isSelected, column, host);
    bool           isEdit = false;



    if (!m_isEditable)
    {
        outStyle = host;
        return isHost;
    }

    state.isEditing   = m_isEditing;
    state.isCaret     = isCaret;
    state.isTyping    = isCaret && m_isEditing && IsTypingAt (offset);
    state.isHexColumn = column == Column::Hex;
    state.isSelected  = isSelected;
    state.isChanged   = m_isChanged && m_isChanged (offset, mark);
    isEdit            = PickEditStyle (colors, state, edit);

    if (!isHost || state.isCaret)
    {
        outStyle = edit;
        return isEdit;
    }

    outStyle = host;

    if (outStyle.fill == 0)
    {
        outStyle.fill = edit.fill;
    }

    if (state.isChanged && edit.ink != 0)
    {
        outStyle.ink = GetReadableInk (edit.ink, outStyle.fill != 0 ? outStyle.fill : colors.background);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexHostEdit::PickEditStyle
//
//  As the debugger's memory window draws its bytes. The caret byte has the
//  accent behind it, in the ink that reads on the accent, whether or not it
//  is changed or being typed. Selected bytes are the view's selection. A
//  changed byte is drawn in the changed-byte red, and while editing, every
//  other byte has a light accent tint behind it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexHostEdit::PickEditStyle (const EditColors & colors, const EditByteState & state, ByteStyle & outStyle)
{
    uint32_t  tint = DxuiColor::Mix (colors.background, colors.accent, kTintShare);



    outStyle = ByteStyle();

    if (colors.accent == 0)
    {
        return false;
    }

    if (state.isCaret)
    {
        outStyle.fill = colors.accent;
        outStyle.ink  = DxuiColor::GetContrastingInk (colors.accent);
        return true;
    }

    if (state.isSelected || (!state.isEditing && !state.isChanged))
    {
        return false;
    }

    outStyle.fill = state.isEditing ? tint : 0;
    outStyle.ink  = state.isChanged ? GetReadableInk (GetChangedInk (colors.background), state.isEditing ? tint : colors.background) : 0;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexHostEdit::GetChangedInk
//
//  The debugger's memory window color for a changed byte: coral on a dark
//  page and red on a light one, made readable on the page, or on a light
//  page's row shade.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiHexHostEdit::GetChangedInk (uint32_t background)
{
    constexpr uint32_t  kDarkChanged  = 0xFFFF6B68;
    constexpr uint32_t  kLightChanged = 0xFFD00000;
    constexpr uint32_t  kBlack        = 0xFF000000;
    constexpr float     kRowShade     = 0.15f;
    constexpr float     kDarkLimit    = 0.5f;
    bool                isDark        = DxuiColor::ComputeRelativeLuminance (background) < kDarkLimit;



    return GetReadableInk (isDark ? kDarkChanged : kLightChanged, isDark ? background : DxuiColor::Mix (background | kBlack, kBlack, kRowShade));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexHostEdit::GetReadableInk
//
//  The ink, moved toward black or white until it reads on the fill.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiHexHostEdit::GetReadableInk (uint32_t ink, uint32_t fill)
{
    constexpr int    kSteps = 10;
    constexpr float  kStep  = 0.1f;
    uint32_t         toward = DxuiColor::GetContrastingInk (fill);
    uint32_t         result = ink;



    for (int step = 1; step <= kSteps && DxuiColor::ComputeContrastRatio (result, fill) < kMinContrast; step++)
    {
        result = DxuiColor::Mix (ink, toward, step * kStep);
    }

    return result;
}
