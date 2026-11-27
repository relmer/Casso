#pragma once

#include "Pch.h"

class DxuiHexView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewLink
//
//  Links two hex views, such as two images compared side by side.
//  A setting changed from either view's menu is copied to the other: the
//  columns or bytes per row, the grouping, the value format, text only,
//  the text encoding and the zoom. Scrolling either view, up and down or
//  sideways, scrolls the other to the same place, clamped to its rows.
//
//  The link sets each view's settings and scroll callbacks, so a host's
//  settings callback is set here instead of on a view.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHexViewLink
{
public:
    DxuiHexViewLink () = default;
    DxuiHexViewLink (const DxuiHexViewLink &) = delete;
    DxuiHexViewLink & operator= (const DxuiHexViewLink &) = delete;

    void  Link (DxuiHexView & first, DxuiHexView & second);

    //  Called after either view's menu changes a setting, once both views
    //  have it.
    void  SetOnSettingsChanged (std::function<void ()> fn) { m_onSettings = std::move (fn); }

    //  Copies one view's settings, or its place, to the other.
    static void  CopySettings (const DxuiHexView & from, DxuiHexView & to);
    static void  CopyScroll   (const DxuiHexView & from, DxuiHexView & to);

private:
    void  OnSettings (bool fromFirst);
    void  OnScrolled (bool fromFirst);

    DxuiHexView            * m_first     = nullptr;
    DxuiHexView            * m_second    = nullptr;
    std::function<void ()>   m_onSettings;
    bool                     m_isCopying = false;  // a copy is under way, so the other view's callback doesn't copy back
};