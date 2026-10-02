#include "Pch.h"

#include "Ui/Debugger/ToolbarCheckEntry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarCheckEntry::ToolbarCheckEntry
//
////////////////////////////////////////////////////////////////////////////////

ToolbarCheckEntry::ToolbarCheckEntry (std::shared_ptr<const DxuiCommand> command) :
    m_command (std::move (command)),
    m_box     (m_command->label)
{
    m_box.SetSingleLineLabel (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarCheckEntry::GetWidthPx
//
//  The box, the gap and the label, with room to either side; the same in
//  both forms, since a check box has no icon to collapse to.
//
////////////////////////////////////////////////////////////////////////////////

int ToolbarCheckEntry::GetWidthPx (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const
{
    HRESULT  hr     = S_OK;
    float    labelW = (float) (m_command->label.size() * kLabelGuess);
    float    labelH = 0.0f;



    (void) labeled;

    if (text != nullptr)
    {
        hr = text->MeasureString (m_command->label.c_str(), scaler.ToPxf (kFontDip), DxuiTheme::kBodyFace, labelW, labelH);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
    else
    {
        labelW = scaler.ToPxf (labelW);
    }

    return scaler.ToPx (kBoxDip + kGapDip + 2 * kPadDip) + (int) std::ceil (labelW);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarCheckEntry::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ToolbarCheckEntry::Layout (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler)
{
    int  pad = scaler.ToPx (kPadDip);



    (void) labeled;

    m_rc = rc;
    m_box.Layout (RECT { rc.left + pad, rc.top, (std::max) (rc.left + pad, rc.right - pad), rc.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarCheckEntry::Paint
//
////////////////////////////////////////////////////////////////////////////////

void ToolbarCheckEntry::Paint (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme,
    bool                 hovered,
    bool                 pressed,
    bool                 labeled)
{
    (void) hovered;
    (void) pressed;
    (void) labeled;

    m_box.SetChecked (m_command->IsChecked());
    m_box.SetEnabled (m_command->IsEnabled());
    m_box.Paint      (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarCheckEntry::GetTooltipAt
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ToolbarCheckEntry::GetTooltipAt (int x, int y, RECT & anchor) const
{
    if (x < m_rc.left || x >= m_rc.right || y < m_rc.top || y >= m_rc.bottom)
    {
        return nullptr;
    }

    anchor = m_rc;
    m_tip  = m_getTip ? m_getTip() : m_command->tip;

    return m_tip.empty() ? nullptr : m_tip.c_str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarCheckEntry::OnClick
//
////////////////////////////////////////////////////////////////////////////////

bool ToolbarCheckEntry::OnClick (int x, int y)
{
    (void) x;
    (void) y;

    //  Not consumed: the toolbar runs the command.
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarCheckEntry::OnLButtonDown
//
//  The whole entry is the button, the label as much as the box.
//
////////////////////////////////////////////////////////////////////////////////

bool ToolbarCheckEntry::OnLButtonDown (int x, int y)
{
    return x >= m_rc.left && x < m_rc.right && y >= m_rc.top && y < m_rc.bottom;
}
