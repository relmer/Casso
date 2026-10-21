#pragma once

#include "Pch.h"


class DxuiHwndSource;
class DxuiPopupHost;
class IDxuiPainter;
class IDxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabDragVisual
//
//  The card drawn for a dragged tab: a rounded rectangle in the tab's look
//  that moves with the pointer anywhere on the screen, over this window,
//  another, or none, as File Explorer's does. It is a click-through popup, so
//  WindowFromPoint finds the window under it, and it is topmost while the
//  drag lasts.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabDragVisual
{
public:
    using RenderFn = std::function<void (IDxuiPainter & painter, IDxuiTextRenderer & text, float widthPx, float heightPx)>;

    DxuiTabDragVisual() = default;
    ~DxuiTabDragVisual();

    DxuiTabDragVisual (const DxuiTabDragVisual &)             = delete;
    DxuiTabDragVisual & operator= (const DxuiTabDragVisual &) = delete;

    //  Display the card `sizePx` big with its top-left corner at
    //  `topLeftScreen`, filled with `fillArgb` and drawn by `render` in card
    //  pixels.
    HRESULT  Show    (DxuiHwndSource & popups, SIZE sizePx, POINT topLeftScreen, uint32_t fillArgb, RenderFn render);
    void     MoveTo  (POINT topLeftScreen);
    void     Hide    ();
    bool     IsShown () const { return m_popup != nullptr; }

private:
    DxuiHwndSource  * m_popups = nullptr;
    DxuiPopupHost   * m_popup  = nullptr;
    POINT             m_at     = {};
    RenderFn          m_render;
};
