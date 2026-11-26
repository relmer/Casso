#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexView
//
//  Block mode's hex and text panes: an editable hex view. A Ctrl+click on a
//  byte reports its offset, so a link there can be followed, a double-click
//  on a byte reports its offset, and Page Up, Page Down and Alt with an
//  arrow are reported before the view handles them.
//
////////////////////////////////////////////////////////////////////////////////

class BlockHexView : public DxuiHexView
{
public:
    using ClickFn    = std::function<void (uint64_t offset)>;
    using NavigateFn = std::function<bool (const DxuiKeyEvent & ev)>;

    void  SetOnClick    (ClickFn fn)    { m_onClick    = std::move (fn); }
    void  SetOnOpen     (ClickFn fn)    { m_onOpen     = std::move (fn); }
    void  SetOnNavigate (NavigateFn fn) { m_onNavigate = std::move (fn); }

    BlockHexView () { GetHostEdit().SetEditable (true); }

    bool  OnKey   (const DxuiKeyEvent & ev) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

private:
    ClickFn     m_onClick;
    ClickFn     m_onOpen;
    NavigateFn  m_onNavigate;
    uint64_t    m_pressAt = 0;   // the byte the last press was on
    int64_t     m_pressMs = 0;   // and when, for a double-click
};
