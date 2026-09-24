#include "Pch.h"

#include "Ui/Debugger/Panes/DiskHeadView.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetPreferredHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DiskHeadView::GetPreferredHeightPx (int widthPx, const DxuiDpiScaler & scaler) const
{
    int  rows = IsLabelBelow ((float) widthPx, scaler) ? 2 : 1;



    return scaler.ToPx (kRulerDip + kGapDip + kRowDip * rows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskHeadView::GetLabel() const
{
    constexpr int  kHundredths = 100;



    return std::format (L"Drive {}  track {}.{:02}", m_head.drive + 1, m_head.quarterTrack / kQuarters,
                        (m_head.quarterTrack % kQuarters) * (kHundredths / kQuarters));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::IsLabelBelow
//
//  The lamps and the label side by side where they fit, measured by the
//  monospace advance; otherwise the label takes a row of its own rather than
//  being cut off at the pane's edge. The widest track is assumed, so the
//  label does not jump between rows as the head moves.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskHeadView::IsLabelBelow (float widthPx, const DxuiDpiScaler & scaler) const
{
    static constexpr float  kAdvancePerDip = 0.6f;
    static constexpr int    kLamps         = 5;
    static constexpr size_t kWidestLabel   = std::size (L"Drive 2  track 39.75") - 1;
    float                   lamps          = scaler.ToPxf ((float) kLampStepDip) * kLamps;
    float                   label          = scaler.ToPxf (m_fontDip) * kAdvancePerDip * (float) kWidestLabel;



    return lamps + label > widthPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetHeadX
//
//  Each quarter track has an equal share of the width; the marker covers the
//  head's share.
//
////////////////////////////////////////////////////////////////////////////////

float DiskHeadView::GetHeadX() const
{
    int  quarter = std::clamp (m_head.quarterTrack, 0, GetQuarterCount() - 1);



    return (float) m_boundsDip.left + GetHeadWidth() * (float) quarter;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::GetHeadWidth
//
////////////////////////////////////////////////////////////////////////////////

float DiskHeadView::GetHeadWidth() const
{
    return (float) (m_boundsDip.right - m_boundsDip.left) / (float) GetQuarterCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (!m_visible || m_boundsDip.right <= m_boundsDip.left)
    {
        return;
    }

    //  Whether the label fits beside the lamps is judged at this size before
    //  the next layout.
    m_fontDip = theme.MonospaceFont().sizeDip;

    PaintRuler (painter, theme);
    PaintLamps (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::PaintRuler
//
//  A tick at the start of every track, a longer one every fifth, and the
//  head in the accent color while the motor turns and muted while it rests.
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::PaintRuler (IDxuiPainter & painter, const IDxuiTheme & theme) const
{
    float  ruler = m_scaler.ToPxf ((float) kRulerDip);
    float  line  = std::max (1.0f, std::round (m_scaler.ToPxf (1.0f)));
    float  top   = (float) m_boundsDip.top;
    float  width = GetHeadWidth();



    painter.FillRect ((float) m_boundsDip.left, top, (float) (m_boundsDip.right - m_boundsDip.left), ruler, theme.ControlBackground());

    for (int quarter = 0; quarter < GetQuarterCount(); quarter += kQuarters)
    {
        bool   major = (quarter / kQuarters) % kMajorTrack == 0;
        float  tall  = major ? ruler : ruler / 2;

        painter.FillRect ((float) m_boundsDip.left + width * (float) quarter, top + ruler - tall, line, tall, theme.Divider());
    }

    painter.FillRect (GetHeadX(), top, std::max (line, width), ruler, m_head.motorOn ? theme.Accent() : theme.ForegroundMuted());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadView::PaintLamps
//
//  PH0 to PH3, then the motor, then the drive and track as text: beside the
//  lamps, or on the row below them where it would not fit.
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::PaintLamps (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr int   kPhases = 4;
    DxuiFontHandle  font    = theme.MonospaceFont();
    float           row     = m_scaler.ToPxf ((float) kRowDip);
    float           lamp    = m_scaler.ToPxf ((float) kLampDip);
    float           step    = m_scaler.ToPxf ((float) kLampStepDip);
    float           top     = (float) m_boundsDip.top + m_scaler.ToPxf ((float) (kRulerDip + kGapDip));
    float           x       = (float) m_boundsDip.left;
    float           size    = m_scaler.ToPxf (font.sizeDip);
    std::wstring    label;
    HRESULT         hr      = S_OK;



    for (int phase = 0; phase <= kPhases; phase++)
    {
        bool  isMotor = (phase == kPhases);
        bool  on      = isMotor ? m_head.motorOn : ((m_head.phases >> phase) & 1) != 0;

        painter.FillRect (x, top + (row - lamp) / 2, lamp, lamp, on ? theme.Accent() : theme.ControlBackground());

        label = isMotor ? std::wstring (L"M") : std::format (L"{}", phase);
        hr    = text.DrawString (label.c_str(), x + lamp, top, step - lamp, row, theme.ForegroundMuted(), size, font.face,
                                 DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        x += step;
    }

    if (IsLabelBelow ((float) (m_boundsDip.right - m_boundsDip.left), m_scaler))
    {
        x    = (float) m_boundsDip.left;
        top += row;
    }

    label = GetLabel();
    hr    = text.DrawString (label.c_str(), x, top, std::max (0.0f, (float) m_boundsDip.right - x), row, theme.Foreground(), size, font.face,
                             DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
