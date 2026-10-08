#pragma once

#include "Pch.h"


class DxuiHwndSource;
class DxuiPopupHost;
class IDxuiPainter;
class IDxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInPlaceTip
//
//  A cut-off name shown whole, laid exactly over the row it belongs to, as
//  Explorer's navigation pane does when its splitter hides a name's end. It
//  shows at once, with no delay, and the pointer passes through it to the
//  row beneath. What it draws is the caller's: the row's own icon and label,
//  at the same places.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiInPlaceTip
{
public:
    using RenderFn = std::function<void (IDxuiPainter & painter, IDxuiTextRenderer & text)>;

    ~DxuiInPlaceTip() { Hide(); }

    void  SetPopupHost (DxuiHwndSource * host) { m_popupHost = host; }

    //  Over `clientRect` of the host window, filled with `backgroundArgb`
    //  (opaque). A second call for the same rect keeps the tip already up.
    void  Show (const RECT & clientRect, uint32_t backgroundArgb, RenderFn render);
    void  Hide ();

    bool          IsVisible () const { return m_activePopup != nullptr; }
    const RECT &  GetRect   () const { return m_rect; }

private:
    DxuiHwndSource  * m_popupHost   = nullptr;
    DxuiPopupHost   * m_activePopup = nullptr;
    RECT              m_rect        = {};
    RenderFn          m_render;
};