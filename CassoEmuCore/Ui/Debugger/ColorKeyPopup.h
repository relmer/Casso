#pragma once

#include "Pch.h"

#include "Ui/Debugger/ColorLegend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyPopup
//
//  One pane's color key: a small balloon under the pane's info button with a
//  line for each color the pane draws, its swatch beside the one-line meaning,
//  in the colors of the theme in force.
//
//  A TOOLTIP CANNOT HOLD IT. DxuiTooltip draws one run of text; a key needs a
//  swatch on each line, drawn as the pane draws the color: a fill, a sample
//  of text in the color or on it, the PC's arrow, a dot, a ring or an
//  outline. So the key is a popup of its own, placed and shown as a tooltip
//  is (no activation, the pointer passing through) and drawing its rows
//  itself.
//
//  It is shown while the pointer rests on the button, and held open by a
//  click or by Space or Enter on the focused button until the same again,
//  Escape, or the focus leaving the button.
//
////////////////////////////////////////////////////////////////////////////////

class ColorKeyPopup
{
public:
    //  A line of the key: how its swatch is drawn, in which color, and the
    //  meaning beside it.
    struct Row
    {
        ColorLegend::Swatch              swatch = ColorLegend::Swatch::Fill;
        uint32_t                         argb   = 0;
        std::wstring                     text;
        std::shared_ptr<DxuiIconImage>   icon;
    };

    static constexpr int    kPadDip     = 8;
    static constexpr int    kSwatchDip  = 14;
    static constexpr int    kGapDip     = 8;
    static constexpr int    kRowDip     = 20;
    static constexpr float  kFontDip    = 12.0f;
    static constexpr int    kMaxTextDip = 420;

    //  A pane's lines, from the legend, in a palette's colors.
    static std::vector<Row>  MakeRows (ColorLegend::Pane pane, const ColorLegend::Palette & palette);

    ~ColorKeyPopup() { Hide(); }

    //  Shows a pane's key under an anchor given in the host's client pixels,
    //  or moves the one up to it. Held, it stays until Hide; otherwise it
    //  goes when the pointer leaves the button.
    void  Show (DxuiHwndSource * host, const RECT & anchor, ColorLegend::Pane pane, const ColorLegend::Palette & palette,
                const IDxuiTheme & theme, bool hold);
    void  Hide ();

    bool                      IsShown  () const { return m_isShown; }
    bool                      IsHeld   () const { return m_isHeld; }
    ColorLegend::Pane         GetPane  () const { return m_pane; }
    const std::vector<Row> &  GetRows  () const { return m_rows; }
    DxuiPopupHost          *  GetPopup () const { return m_popup; }

private:
    HRESULT  Raise       (HWND owner, const RECT & screen);
    SIZE     MeasureDip  (UINT dpi) const;
    void     Render      (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    DxuiHwndSource     * m_host       = nullptr;
    DxuiPopupHost      * m_popup      = nullptr;
    std::vector<Row>     m_rows;
    ColorLegend::Pane    m_pane       = ColorLegend::Pane::Disassembly;
    DxuiDpiScaler        m_scaler;
    uint32_t             m_background = 0;
    uint32_t             m_border     = 0;
    uint32_t             m_foreground = 0;
    bool                 m_isShown    = false;
    bool                 m_isHeld     = false;
};
