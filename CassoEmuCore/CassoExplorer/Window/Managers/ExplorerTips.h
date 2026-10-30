#pragma once

#include "Pch.h"


struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips
//
//  The hover tooltip: whose it is, and the tree's, the list's, the status
//  bar's and a cut-off tree name's tips.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerTips
{
public:
    //  Which part of the window the hover tooltip belongs to.
    enum class TipOwner { None, Toolbar, Tree, List, Status, Menu, Caption };

    explicit ExplorerTips (ExplorerContext & context);

    void  ShowHoverTip        (TipOwner owner, const RECT & anchor, const std::wstring & text,
                               float maxWidthDip = DxuiTooltip::kDefaultMaxTextWidthDip);
    void  HideHoverTip        (TipOwner owner);

    //  The tooltip, which follows the pointer, at the window's DPI and size.
    void  Create        (DxuiHwndSource * popupHost) { m_tooltip.SetPopupHost (popupHost); m_tooltip.SetFollowPointer (true); }
    void  ApplyTheme    (const DxuiTheme & theme) { m_tooltip.SetTheme (theme); }
    void  Layout        (UINT dpi, int widthPx, int heightPx) { m_tooltip.SetDpi (dpi); m_tooltip.SetViewportSize (widthPx, heightPx); }
    void  HideImmediate () { m_tooltip.HideImmediate(); }
    bool  WantsTick     () const { return m_tooltip.WantsTick() || m_tooltip.IsVisible(); }
    void  Tick          (int64_t nowMs);

private:
    ExplorerContext  & m_context;
    DxuiTooltip        m_tooltip;
    POINT              m_listTipPoint  = {};   // where the pointer last was over the list
    bool               m_listTipActive = false;
    TipOwner           m_tipOwner      = TipOwner::None;
};
