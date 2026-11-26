#pragma once

#include "Pch.h"
#include "Widgets/HexView/DxuiHexViewTypes.h"


class IDxuiHexEditHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexSourceEdit
//
//  Typing a hex view's values into its byte source. In the hex column,
//  digits collect for the value under the caret (the hex format only) and
//  the value is written, low byte first, as soon as it has all its digits;
//  in the text column each character is written as it is typed, in the
//  view's text encoding. The caret moves on after a write the source takes.
//  Escape or any caret move drops digits not yet written.
//
//  An edit of a value is under way from its first digit, from Enter and
//  from a double-click on it, until its last digit is typed, Enter writes
//  the digits typed or Escape drops them.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHexSourceEdit
{
public:
    explicit DxuiHexSourceEdit (IDxuiHexEditHost & host) : m_host (host) {}

    void                  SetEditable       (bool isEditable)                    { m_isEditable = isEditable; Drop(); }
    void                  SetOnWriteRefused (DxuiHexViewTypes::WriteRefusedFn fn) { m_onWriteRefused = std::move (fn); }

    bool                  IsEditable        () const { return m_isEditable; }
    bool                  IsEditing         () const { return m_isEditing; }
    bool                  HasPendingDigits  () const { return !m_pending.empty(); }
    uint64_t              GetEditStart      () const { return m_editStart; }
    const std::wstring &  GetPendingDigits  () const { return m_pending; }

    bool                  TypeChar          (wchar_t ch);
    bool                  BeginEdit         ();
    bool                  CommitEdit        ();
    bool                  CancelEdit        ();
    void                  Drop              ();
    void                  NotePress         (uint64_t offset, uint64_t nowMs, uint64_t doubleClickMs);

    static int            GetDigitValue     (wchar_t ch);

private:
    bool  TypeHexDigit    (wchar_t ch);
    bool  TypeCharacter   (wchar_t ch);
    void  WriteValue      ();
    void  WriteAndAdvance (uint64_t offset, std::span<const uint8_t> bytes);

    IDxuiHexEditHost                 & m_host;
    bool                               m_isEditable  = false;
    bool                               m_isEditing   = false;
    std::wstring                       m_pending;
    uint64_t                           m_editStart   = 0;
    uint64_t                           m_pressMs     = 0;
    uint64_t                           m_pressOffset = UINT64_MAX;
    DxuiHexViewTypes::WriteRefusedFn   m_onWriteRefused;
};
