#include "Pch.h"

#include "Ui/Settings/JoyportSwitchView.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





static constexpr const wchar_t *  s_kpszReadoutFont  = L"Segoe UI";
static constexpr float            s_kReadoutFontDip  = 12.0f;
static constexpr float            s_kLabelBandDip    = 18.0f;
static constexpr float            s_kOutlineDip      = 1.5f;
static constexpr float            s_kGapDip          = 16.0f;

// The cross is three cells on a side; the fire button's diameter, in cells.
static constexpr float            s_kCrossCells      = 3.0f;
static constexpr float            s_kFireCells       = 1.2f;





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void JoyportSwitchView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_scaler.SetDpi (scaler.GetDpi());
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  The cross is as large as the bounds allow with the fire button and its gap
//  beside it: up, down, left and right cells around an unlit hub, then fire,
//  centered on the cross's middle row and captioned underneath. A cell fills
//  with the accent while its switch reads closed. With no reading everything
//  stays unlit, the way an unplugged stick reads.
//
////////////////////////////////////////////////////////////////////////////////

void JoyportSwitchView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     bounds  = GetBounds();
    float    width   = (float) (bounds.right - bounds.left);
    float    height  = (float) (bounds.bottom - bounds.top);
    float    gap     = m_scaler.ToPxf (s_kGapDip);
    float    band    = m_scaler.ToPxf (s_kLabelBandDip);
    float    fontPx  = m_scaler.ToPxf (s_kReadoutFontDip);
    float    outline = m_scaler.ToPxf (s_kOutlineDip);
    float    cross   = std::min (height, (width - gap) * s_kCrossCells / (s_kCrossCells + s_kFireCells));
    float    cell    = cross / s_kCrossCells;
    float    left    = (float) bounds.left;
    float    top     = (float) bounds.top;
    float    radius  = cell * s_kFireCells * 0.5f;
    float    fireX   = left + cross + gap + radius;
    float    fireY   = top + cross * 0.5f;
    bool     isFire  = m_isActive && IsClosed (JoystickSwitch::Fire);
    HRESULT  hr      = S_OK;



    UNREFERENCED_PARAMETER (painter);

    if (!IsVisible() || cell <= outline)
    {
        return;
    }

    PaintCell (text, theme, left + cell,     top,            cell, m_isActive && IsClosed (JoystickSwitch::Up));
    PaintCell (text, theme, left,            top + cell,     cell, m_isActive && IsClosed (JoystickSwitch::Left));
    PaintCell (text, theme, left + cell,     top + cell,     cell, false);
    PaintCell (text, theme, left + cell * 2, top + cell,     cell, m_isActive && IsClosed (JoystickSwitch::Right));
    PaintCell (text, theme, left + cell,     top + cell * 2, cell, m_isActive && IsClosed (JoystickSwitch::Down));

    hr = text.FillEllipse (fireX, fireY, radius, radius, isFire ? theme.Accent() : theme.BackgroundElevated());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawEllipse (fireX, fireY, radius - outline * 0.5f, radius - outline * 0.5f, outline,
                           isFire ? theme.Accent() : theme.Border());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (L"Fire", fireX - radius * 2.0f, fireY + radius, radius * 4.0f, band,
                          theme.ForegroundMuted(), fontPx, s_kpszReadoutFont,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintCell
//
//  One square of the cross, outlined, and filled while lit.
//
////////////////////////////////////////////////////////////////////////////////

void JoyportSwitchView::PaintCell (
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme,
    float                x,
    float                y,
    float                size,
    bool                 isLit) const
{
    float     outline = m_scaler.ToPxf (s_kOutlineDip);
    uint32_t  edge    = isLit ? theme.Accent() : theme.Border();
    HRESULT   hr      = S_OK;



    hr = text.FillRect (x, y, size, size, isLit ? theme.Accent() : theme.BackgroundElevated());
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (x,        y,        x + size, y,        outline, edge);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (x + size, y,        x + size, y + size, outline, edge);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (x + size, y + size, x,        y + size, outline, edge);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (x,        y + size, x,        y,        outline, edge);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
