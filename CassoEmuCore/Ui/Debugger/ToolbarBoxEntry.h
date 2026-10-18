#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarBoxEntry
//
//  One of the window's text boxes as a toolbar entry, with a label ahead of
//  it: the box is in the focus ring and gets its own keys, as the memory
//  bar's Address box does.
//
//  THE TOOLBAR PLACES IT; THE WINDOW PAINTS AND ROUTES THE BOX. Layout puts
//  the box in the entry's rect less a gap and the label, which Paint draws.
//  The box takes its own presses through the window; the entry takes none.
//
////////////////////////////////////////////////////////////////////////////////

class ToolbarBoxEntry : public IDxuiToolbarCustomEntry
{
public:
    ToolbarBoxEntry (DxuiTextInput * box, std::wstring label, std::wstring tip, int labelWidthDip, int boxWidthDip) :
        m_box           (box),
        m_label         (std::move (label)),
        m_tip           (std::move (tip)),
        m_labelWidthDip (labelWidthDip),
        m_boxWidthDip   (boxWidthDip)
    {
    }

    ~ToolbarBoxEntry() override = default;

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
    int              GetMinWidthPx (const DxuiDpiScaler & scaler) const override { return scaler.ToPx (kGapDip + m_labelWidthDip + m_boxWidthDip); }

    static constexpr float  kLabelFontDip = 12.0f;

    //  The space ahead of the label, so a label is clear of the entry before
    //  it.
    static constexpr int    kGapDip       = 12;

private:
    static bool  IsInside (const RECT & rc, int x, int y);

    DxuiTextInput  * m_box           = nullptr;
    std::wstring     m_label;
    std::wstring     m_tip;
    int              m_labelWidthDip = 0;
    int              m_boxWidthDip   = 0;
    DxuiDpiScaler    m_scaler;
    RECT             m_rc            = {};
    RECT             m_labelRc       = {};
};
