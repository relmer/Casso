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

int DiskHeadView::GetPreferredHeightPx (const DxuiDpiScaler & scaler) const
{
    return scaler.ToPx (kRulerDip + kGapDip + kRowDip);
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
//  PH0 to PH3, then the motor, then the drive and track as text.
//
////////////////////////////////////////////////////////////////////////////////

void DiskHeadView::PaintLamps (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr int   kPhases     = 4;
    constexpr int   kHundredths = 100;
    DxuiFontHandle  font        = theme.MonospaceFont();
    float           row         = m_scaler.ToPxf ((float) kRowDip);
    float           lamp        = m_scaler.ToPxf ((float) kLampDip);
    float           step        = m_scaler.ToPxf ((float) kLampStepDip);
    float           top         = (float) m_boundsDip.top + m_scaler.ToPxf ((float) (kRulerDip + kGapDip));
    float           x           = (float) m_boundsDip.left;
    float           size        = m_scaler.ToPxf (font.sizeDip);
    std::wstring    label;
    HRESULT         hr          = S_OK;



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

    label = std::format (L"Drive {}  track {}.{:02}", m_head.drive + 1, m_head.quarterTrack / kQuarters,
                         (m_head.quarterTrack % kQuarters) * (kHundredths / kQuarters));
    hr    = text.DrawString (label.c_str(), x, top, std::max (0.0f, (float) m_boundsDip.right - x), row, theme.Foreground(), size, font.face,
                             DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
