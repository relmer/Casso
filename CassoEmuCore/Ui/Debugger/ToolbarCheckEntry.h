#pragma once

#include "Pch.h"
#include "Core/DxuiCommand.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiToolbar.h"





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

private:
    static constexpr int    kBoxDip     = 16;
    static constexpr int    kGapDip     = 6;
    static constexpr int    kPadDip     = 6;
    static constexpr float  kFontDip    = 13.0f;
    static constexpr int    kLabelGuess = 7;

    std::shared_ptr<const DxuiCommand>  m_command;
    DxuiCheckbox                        m_box;
    RECT                                m_rc = {};
};
