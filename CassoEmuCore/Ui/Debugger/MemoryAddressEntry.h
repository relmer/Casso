#pragma once

#include "Pch.h"
#include "Widgets/DxuiTextInput.h"
#include "Widgets/DxuiToolbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryAddressEntry
//
//  The memory bar's Address combo box as a toolbar entry: the window's own
//  text box, which keeps its place in the focus ring and its keys, with a
//  drop-down arrow at its right end for the addresses entered before.
//
//  THE TOOLBAR PLACES IT; THE WINDOW PAINTS AND ROUTES THE BOX. Layout puts
//  the box in the entry's rect less the arrow. A click on the arrow is left
//  to the toolbar, which opens the entry's drop-down; the box takes its own
//  presses through the window.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryAddressEntry : public IDxuiToolbarCustomEntry
{
public:
    explicit MemoryAddressEntry (DxuiTextInput * box) : m_box (box) {}
    ~MemoryAddressEntry() override = default;

    void  SetTooltips (const std::wstring & box, const std::wstring & arrow) { m_boxTip = box; m_arrowTip = arrow; }

    //  The drop-down arrow's rect, in client pixels.
    RECT  GetArrowRect () const { return m_arrow; }

    int              GetWidthPx    (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const override;
    void             Layout        (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler) override;
    void             Paint         (IDxuiPainter      & painter,
                                    IDxuiTextRenderer & text,
                                    const IDxuiTheme  & theme,
                                    bool                hovered,
                                    bool                pressed,
                                    bool                labeled) override;
    const wchar_t *  GetTooltipAt  (int x, int y, RECT & anchor) const override;
    bool             OnClick       (int x, int y) override;
    bool             OnMouseMove   (int x, int y) override;
    void             OnMouseLeave  () override;
    bool             OnLButtonDown (int x, int y) override;

    static constexpr int  kWidthDip = 180;

private:
    static bool  IsInside (const RECT & rc, int x, int y);

    static constexpr int    kArrowDip      = 20;
    static constexpr float  kArrowGlyphDip = 10.0f;

    DxuiTextInput  * m_box   = nullptr;
    DxuiDpiScaler    m_scaler;
    RECT             m_rc    = {};
    RECT             m_arrow = {};
    bool             m_hover = false;
    std::wstring     m_boxTip;
    std::wstring     m_arrowTip;
};
