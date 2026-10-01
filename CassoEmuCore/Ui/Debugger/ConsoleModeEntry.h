#pragma once

#include "Pch.h"
#include "Widgets/DxuiToolbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeEntry
//
//  The console bar's mode picker as a toolbar entry: a static "Mode:" label
//  and a drop-down box showing the mode in force. A click on the box is left
//  to the toolbar, which opens the entry's drop-down of modes; a click on the
//  label does nothing.
//
////////////////////////////////////////////////////////////////////////////////

class ConsoleModeEntry : public IDxuiToolbarCustomEntry
{
public:
    ConsoleModeEntry()           = default;
    ~ConsoleModeEntry() override = default;

    void                   SetModeText (const std::wstring & text) { m_modeText = text; }
    const std::wstring &   GetModeText () const                    { return m_modeText; }
    void                   SetTooltip  (const std::wstring & text) { m_tip = text; }

    //  The box's rect, in client pixels.
    RECT  GetBoxRect () const { return m_box; }

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

    static constexpr int    kLabelDip      = 44;
    static constexpr int    kBoxDip        = 110;
    static constexpr int    kInsetYDip     = 4;
    static constexpr int    kTextPadDip    = 6;
    static constexpr int    kArrowDip      = 18;
    static constexpr float  kFontDip       = 12.0f;
    static constexpr float  kArrowGlyphDip = 9.0f;

private:
    static bool  IsInside (const RECT & rc, int x, int y);

    DxuiDpiScaler  m_scaler;
    RECT           m_rc       = {};
    RECT           m_box      = {};
    bool           m_hover    = false;
    std::wstring   m_modeText;
    std::wstring   m_tip;
};
