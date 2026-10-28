#include "Pch.h"

#include "Ui/DiskInspector/Components/StatusBand.h"
#include "Ui/DiskInspector/ComparisonText.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorMetrics.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorText.h"





static constexpr int      s_kBandDip       = 28;
static constexpr float    s_kChipPadDip    = 8.0f;
static constexpr float    s_kChipGapDip    = 6.0f;
static constexpr float    s_kChipHeightDip = 20.0f;
static constexpr float    s_kChipTextDip   = 12.0f;
static constexpr float    s_kHintTextDip   = 12.0f;
static constexpr LPCWSTR  s_kpszAnalyzing  = L"Analyzing";





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::StatusBand
//
////////////////////////////////////////////////////////////////////////////////

StatusBand::StatusBand (InspectorSession & session, IInspectorComponentHost & host) :
    m_session (session),
    m_host    (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::Create
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::Create()
{
    m_zoomControl = m_host.GetControlParent().CreateChild<DxuiZoomControl>();
    m_zoomControl->SetOnZoom ([this] (double zoom) { m_host.SetWindowZoom (zoom); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::GetHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int StatusBand::GetHeightPx (const DxuiDpiScaler & scaler) const
{
    return scaler.ToPx (s_kBandDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::Layout
//
//  The zoom at the right end, the "?" button before it, and the chips and
//  the hint in the rest.
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    int  pad   = scaler.ToPx (InspectorMetrics::kMarginDip);
    int  zoomW = scaler.ToPx (DxuiZoomControl::kWidthDip);
    int  helpW = (area.bottom - area.top) - 6;



    m_scaler  = scaler;
    m_bandPx  = area;
    m_helpPx  = { area.right - zoomW - 2 * pad - helpW, area.top + 3, area.right - zoomW - 2 * pad, area.bottom - 3 };
    m_chipsPx = { area.left + pad, area.top, m_helpPx.left - pad, area.bottom };
    m_zoomControl->Layout ({ area.right - zoomW - pad, area.top + 1, area.right - pad, area.bottom - 1 }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::SetZoom
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::SetZoom (double zoom)
{
    m_zoomControl->SetZoom (zoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::PaintUnder
//
//  The band's surface with a divider along its top, the chips, the hint in
//  the room after them, and the "?" button.
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::PaintUnder (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float  x = static_cast<float> (m_chipsPx.left);



    painter.FillRect (static_cast<float> (m_bandPx.left), static_cast<float> (m_bandPx.top), static_cast<float> (m_bandPx.right - m_bandPx.left),
                      static_cast<float> (m_bandPx.bottom - m_bandPx.top), theme.BackgroundElevated());
    painter.FillRect (static_cast<float> (m_bandPx.left), static_cast<float> (m_bandPx.top), static_cast<float> (m_bandPx.right - m_bandPx.left), 1.0f,
                      theme.Divider());

    PaintChips (painter, text, theme, x);

    if (x < m_chipsPx.right)
    {
        text.DrawString (m_hint.c_str(), x, static_cast<float> (m_chipsPx.top), static_cast<float> (m_chipsPx.right) - x,
                         static_cast<float> (m_chipsPx.bottom - m_chipsPx.top), theme.ForegroundMuted(), m_scaler.ToPxf (s_kHintTextDip),
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    PaintHelp (text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::PaintChips
//
//  The summary chips that fit (FR-018), the comparison's result first while
//  comparing (FR-121), a chip for custom decode settings, and "Analyzing"
//  while records are not yet analyzed. Moves inOutX past what it drew.
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::PaintChips (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float & inOutX)
{
    float                pad    = m_scaler.ToPxf (s_kChipPadDip);
    float                gap    = m_scaler.ToPxf (s_kChipGapDip);
    float                chipH  = m_scaler.ToPxf (s_kChipHeightDip);
    float                textPx = m_scaler.ToPxf (s_kChipTextDip);
    float                y      = (m_chipsPx.top + m_chipsPx.bottom - chipH) / 2.0f;
    float                right  = static_cast<float> (m_chipsPx.right);
    float                w      = 0;
    float                h      = 0;
    bool                 isFull = false;
    vector<SummaryChip>  chips;



    if (m_session.context.hasDisk)
    {
        if (m_session.comparison.IsComparing())
        {
            std::wstring  result = (m_session.comparison.IsBusy() || !m_session.comparison.HasResult()) ? std::wstring (L"Comparing")
                                                                                                       : ComparisonText::FormatResult (m_session.comparison.GetResult());

            text.MeasureString (result.c_str(), textPx, DxuiTheme::kBodyFace, w, h);
            painter.FillRoundedRect (inOutX, y, w + 2 * pad, chipH, chipH / 2, theme.SelectionBackground());
            text.DrawString (result.c_str(), inOutX, y, w + 2 * pad, chipH, theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
            inOutX += w + 2 * pad + gap;
        }

        chips = InspectorText::BuildChips (m_session.analysis.summary);

        if (!m_session.analysis.settings.IsStandard())
        {
            SummaryChip  custom;

            custom.text = L"Custom decode settings";
            chips.push_back (custom);
        }

        for (const SummaryChip & chip : chips)
        {
            text.MeasureString (chip.text.c_str(), textPx, DxuiTheme::kBodyFace, w, h);
            isFull = isFull || inOutX + w + 2 * pad > right;

            if (!isFull)
            {
                painter.FillRoundedRect    (inOutX, y, w + 2 * pad, chipH, chipH / 2, theme.ButtonIdle());
                painter.OutlineRoundedRect (inOutX, y, w + 2 * pad, chipH, chipH / 2, m_scaler.ToPxf (1.0f),
                                            chip.isBad ? m_session.context.palette.colors.sectorBad : theme.ButtonBorder());
                text.DrawString (chip.text.c_str(), inOutX, y, w + 2 * pad, chipH, chip.isBad ? m_session.context.palette.colors.sectorBad : theme.Foreground(),
                                 textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                inOutX += w + 2 * pad + gap;
            }
        }
    }

    if (m_session.pendingRequest != 0 || m_session.scheduler->HasPending())
    {
        text.MeasureString (s_kpszAnalyzing, textPx, DxuiTheme::kBodyFace, w, h);
        text.DrawString (s_kpszAnalyzing, inOutX, y, w + gap, chipH, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        inOutX += w + 2 * gap;
    }

    inOutX += (inOutX > m_chipsPx.left) ? gap : 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::PaintHelp
//
//  The "?" button in a circle, which opens the walkthrough (FR-153).
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::PaintHelp (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    float  cx     = (m_helpPx.left + m_helpPx.right) / 2.0f;
    float  cy     = (m_helpPx.top + m_helpPx.bottom) / 2.0f;
    float  radius = std::min (m_helpPx.right - m_helpPx.left, m_helpPx.bottom - m_helpPx.top) / 2.0f;



    text.DrawEllipse (cx, cy, radius - 0.5f, radius - 0.5f, 1.0f, theme.ForegroundMuted());
    text.DrawString  (L"?", static_cast<float> (m_helpPx.left), static_cast<float> (m_helpPx.top), static_cast<float> (m_helpPx.right - m_helpPx.left),
                      static_cast<float> (m_helpPx.bottom - m_helpPx.top), theme.Foreground(), radius * 1.3f, DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                      DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::OnMouse
//
//  A press on "?" opens the walkthrough.
//
////////////////////////////////////////////////////////////////////////////////

bool StatusBand::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isPress = ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && PtInRect (&m_helpPx, ev.positionDip);



    if (isPress)
    {
        m_host.OpenWalkthrough();
    }

    return isPress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::GetTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StatusBand::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    std::wstring  tip;



    if (PtInRect (&m_helpPx, pointPx))
    {
        tip         = L"Open the walkthrough";
        outAnchorPx = m_helpPx;
    }

    return tip;
}
