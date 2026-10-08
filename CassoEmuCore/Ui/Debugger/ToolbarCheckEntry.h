#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarCheckEntry
//
//  A check box and its label as a toolbar entry, as Visual Studio's
//  disassembly viewing options are. The command the entry goes with gives
//  its label, tip, checked and enabled state each time it paints, and a
//  click runs the command as the toolbar runs any other.
//
////////////////////////////////////////////////////////////////////////////////

class ToolbarCheckEntry : public IDxuiToolbarCustomEntry
{
public:
    explicit ToolbarCheckEntry (std::shared_ptr<const DxuiCommand> command);
    ~ToolbarCheckEntry() override = default;

    const DxuiCheckbox &  GetCheckbox () const { return m_box; }

    //  A tip that changes with what is loaded: asked for each time the tip
    //  shows, in place of the command's own.
    void  SetTipSource (std::function<std::wstring()> getTip) { m_getTip = std::move (getTip); }

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
    bool             OnLButtonDown (int x, int y) override;
    int              GetLeadPx     (const DxuiDpiScaler & scaler) const override { return scaler.ToPx (kLeadDip); }

private:
    static constexpr int    kBoxDip     = 16;
    static constexpr int    kGapDip     = 6;
    static constexpr int    kLeadDip    = 4;    // ahead of the box: a compact button's padding
    static constexpr int    kPadDip     = 8;    // after the label
    static constexpr float  kFontDip    = 13.0f;
    static constexpr int    kLabelGuess = 7;

    std::shared_ptr<const DxuiCommand>  m_command;
    DxuiCheckbox                        m_box;
    RECT                                m_rc = {};
    std::function<std::wstring()>       m_getTip;
    mutable std::wstring                m_tip;
};
