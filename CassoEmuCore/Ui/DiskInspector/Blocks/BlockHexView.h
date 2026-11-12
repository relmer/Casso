#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexView
//
//  Block mode's hex and text panes: a hex view that edits. A hex digit typed
//  in the hex column sets the caret byte's high nibble, the next its low
//  nibble, and then the caret moves on; a character typed in the text column
//  replaces the caret byte and the caret moves on. A Ctrl+click on a byte
//  reports its offset, so a link there can be followed, and Page Up, Page
//  Down and Alt with an arrow are reported before the view handles them.
//
////////////////////////////////////////////////////////////////////////////////

class BlockHexView : public DxuiHexView
{
public:
    using TypeFn     = std::function<bool (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch)>;
    using ClickFn    = std::function<void (uint64_t offset)>;
    using NavigateFn = std::function<bool (const DxuiKeyEvent & ev)>;

    void  SetOnType     (TypeFn fn)     { m_onType     = std::move (fn); }
    void  SetOnClick    (ClickFn fn)    { m_onClick    = std::move (fn); }
    void  SetOnNavigate (NavigateFn fn) { m_onNavigate = std::move (fn); }

    //  The next hex digit sets a high nibble again, as after a step to
    //  another block.
    void  ResetNibble () { m_isLowNibble = false; }

    bool  OnChar  (wchar_t ch) override;
    bool  OnKey   (const DxuiKeyEvent & ev) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

private:
    TypeFn      m_onType;
    ClickFn     m_onClick;
    NavigateFn  m_onNavigate;
    bool        m_isLowNibble = false;
    uint64_t    m_nibbleAt    = 0;
};
