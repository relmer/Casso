#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarLabelEntry
//
//  A static label on a toolbar, ahead of the control it describes, as Visual
//  Studio's "Show output from:" is. It takes no press and runs nothing.
//
////////////////////////////////////////////////////////////////////////////////

class ToolbarLabelEntry : public IDxuiToolbarCustomEntry
{
public:
    explicit ToolbarLabelEntry (std::wstring label);
    ~ToolbarLabelEntry() override = default;

    //  The command a toolbar entry carries: the label, and nothing to run.
    std::shared_ptr<const DxuiCommand>  GetCommand () const { return m_command; }
    const std::wstring               &  GetLabel   () const { return m_label; }

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
    int              GetLeadPx     (const DxuiDpiScaler & scaler) const override { return scaler.ToPx (kPadDip); }

private:
    static constexpr int    kPadDip     = 6;
    static constexpr float  kFontDip    = 13.0f;
    static constexpr int    kLabelGuess = 7;

    std::wstring                  m_label;
    std::shared_ptr<DxuiCommand>  m_command;
    RECT                          m_rc     = {};
    DxuiDpiScaler                 m_scaler;
};
