#include "Pch.h"

#include "Widgets/HexView/DxuiHexSourceEdit.h"
#include "Widgets/HexView/DxuiHexPainter.h"
#include "Widgets/HexView/IDxuiHexEditHost.h"
#include "Widgets/DxuiHexView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::TypeChar
//
//  A character goes to the column the caret is in. A view whose values can't
//  be typed into, such as one in line mode, takes nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexSourceEdit::TypeChar (wchar_t ch)
{
    if (!m_isEditable || !m_host.CanEditValues())
    {
        return false;
    }

    return (m_host.GetEditColumn() == DxuiHexViewTypes::Column::Text) ? TypeCharacter (ch) : TypeHexDigit (ch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::TypeHexDigit
//
//  The digits are the value as the column shows it, most significant first;
//  the bytes go to the source low byte first, as the value is read. Decimal
//  formats are not typed into, since a hex digit would not mean what the
//  column shows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexSourceEdit::TypeHexDigit (wchar_t ch)
{
    int  digit = GetDigitValue (ch);



    if (digit < 0 || m_host.GetEditFormat() != DxuiHexViewTypes::ValueFormat::Hex)
    {
        return false;
    }

    if (m_pending.empty() && !m_isEditing)
    {
        m_editStart = m_host.SnapEditToValue (m_host.GetEditCaret());
    }

    m_isEditing = true;
    m_pending.push_back (DxuiHexPainter::GetHexDigit (digit));

    if (m_pending.size() >= m_host.GetEditGroupBytes() * 2)
    {
        m_isEditing = false;
        WriteValue();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::BeginEdit
//
//  An edit of the value under the caret, with no digits yet. The hex values
//  are the only ones edited this way.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexSourceEdit::BeginEdit()
{
    if (!m_isEditable || !m_host.CanEditValues() || m_host.GetEditColumn() == DxuiHexViewTypes::Column::Text ||
        m_host.GetEditFormat() != DxuiHexViewTypes::ValueFormat::Hex)
    {
        return false;
    }

    m_pending.clear();
    m_editStart = m_host.SnapEditToValue (m_host.GetEditCaret());
    m_isEditing = true;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::CommitEdit
//
//  The digits typed so far are the value, as a short number typed into a
//  register is; with none typed the value is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexSourceEdit::CommitEdit()
{
    m_isEditing = false;

    if (!m_pending.empty())
    {
        WriteValue();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::CancelEdit
//
//  Escape drops a value typed only in part. With nothing typed the key isn't
//  the edit's, so a dialog can still take it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexSourceEdit::CancelEdit()
{
    bool  isUnderWay = !m_pending.empty() || m_isEditing;



    Drop();

    return isUnderWay;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::Drop
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSourceEdit::Drop()
{
    m_pending.clear();
    m_isEditing = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::NotePress
//
//  A second press on the same byte within the double-click time begins an
//  edit of its value.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSourceEdit::NotePress (uint64_t offset, uint64_t nowMs, uint64_t doubleClickMs)
{
    if (m_pressOffset == offset && nowMs - m_pressMs <= doubleClickMs)
    {
        (void) BeginEdit();
        m_pressOffset = UINT64_MAX;
    }
    else
    {
        m_pressOffset = offset;
        m_pressMs     = nowMs;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::TypeCharacter
//
//  A printable character becomes its byte in the view's encoding: Apple text
//  sets the high bit, which is how the screen shows a normal character.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexSourceEdit::TypeCharacter (wchar_t ch)
{
    static constexpr wchar_t  kFirstPrintable = 0x20;
    static constexpr wchar_t  kLastPrintable  = 0x7E;
    static constexpr uint8_t  kAppleHighBit   = 0x80;
    uint8_t                   byte            = 0;



    if (ch < kFirstPrintable || ch > kLastPrintable)
    {
        return false;
    }

    byte = (uint8_t) ch;

    if (m_host.GetEditEncoding() == DxuiHexViewTypes::TextEncoding::AppleHighBit)
    {
        byte |= kAppleHighBit;
    }

    WriteAndAdvance (m_host.GetEditCaret(), std::span<const uint8_t> (&byte, 1));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::WriteValue
//
//  The pending digits as one value, written low byte first at the edit's
//  start.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSourceEdit::WriteValue()
{
    size_t                group = m_host.GetEditGroupBytes();
    uint64_t              value = 0;
    std::vector<uint8_t>  bytes;



    for (wchar_t each : m_pending)
    {
        value = (value << 4) | (uint64_t) GetDigitValue (each);
    }

    for (size_t index = 0; index < group; index++)
    {
        bytes.push_back ((uint8_t) ((value >> (index * 8)) & 0xFF));
    }

    m_pending.clear();
    WriteAndAdvance (m_editStart, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::WriteAndAdvance
//
//  A write the source takes moves the caret past it; one it refuses leaves the
//  caret on the cell and calls the refusal callback.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexSourceEdit::WriteAndAdvance (uint64_t offset, std::span<const uint8_t> bytes)
{
    const IDxuiHexSource  * source = m_host.GetEditSource();



    if (source != nullptr && source->WriteBytes (offset, bytes))
    {
        m_host.MoveEditCaretTo (offset + bytes.size());
        return;
    }

    if (m_onWriteRefused)
    {
        m_onWriteRefused (offset);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit::GetDigitValue
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexSourceEdit::GetDigitValue (wchar_t ch)
{
    int  value = -1;



    if (ch >= L'0' && ch <= L'9')
    {
        value = ch - L'0';
    }
    else if (ch >= L'A' && ch <= L'F')
    {
        value = ch - L'A' + 10;
    }
    else if (ch >= L'a' && ch <= L'f')
    {
        value = ch - L'a' + 10;
    }

    return value;
}
