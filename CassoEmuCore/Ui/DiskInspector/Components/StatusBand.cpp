#include "Pch.h"

#include "Ui/DiskInspector/Components/StatusBand.h"
#include "Ui/DiskInspector/ComparisonText.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorMetrics.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorText.h"





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
//  StatusBand::Tick
//
//  The zoom's slider goes by where the cursor is, not the last mouse event, so
//  it stays through a tooltip that ends the window's hover.
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::Tick (int64_t nowMs)
{
    POINT  cursor = {};



    (void) nowMs;

    if (m_scaler.GetDpi() != 0 && m_host.GetOwnerHwnd() != nullptr && GetCursorPos (&cursor) && ScreenToClient (m_host.GetOwnerHwnd(), &cursor))
    {
        m_zoomControl->UpdateHover (cursor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::GetHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int StatusBand::GetHeightPx (const DxuiDpiScaler & scaler) const
{
    return scaler.ToPx (DxuiStatusBar::GetBandDp());
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::Layout
//
//  The zoom over the bar's last field, the "?" button in the field before
//  it, and the chips, the selection and the hint in the rest.
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    int   pad   = scaler.ToPx (InspectorMetrics::kMarginDip);
    RECT  help  = {};
    RECT  zoom  = {};



    m_scaler = scaler;
    m_bandPx = area;

    m_bar.SetDividers (false);
    m_bar.Layout      (area, scaler);
    LayOutBar         (0);

    help = m_bar.GetFieldRect (kFieldHelp);
    zoom = m_bar.GetFieldRect (kFieldZoom);

    m_helpPx  = { help.left + pad, area.top + 3, help.right - pad, area.bottom - 3 };
    m_chipsPx = { area.left + pad, area.top, help.left, area.bottom };
    m_zoomControl->Layout ({ zoom.left + pad, area.top + 1, zoom.right - pad, area.bottom - 1 }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::LayOutBar
//
//  The bar's fields: the room the chips and the selection take, the hint in
//  the stretch field, the "?" button's field, and the zoom's field.
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::LayOutBar (int leadPx)
{
    int  pad   = m_scaler.ToPx (InspectorMetrics::kMarginDip);
    int  helpW = (m_bandPx.bottom - m_bandPx.top) - 6;



    m_bar.SetFields ({ { L"", 0, false, leadPx },
                       { m_hint, 0, true },
                       { L"", 0, false, helpW + 2 * pad },
                       { L"", DxuiZoomControl::kWidthDip + 2 * InspectorMetrics::kMarginDip, false } });
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
//  The bar with the hint in the room after the chips and the selection, a
//  divider along the band's top, then the chips, the selection and the "?"
//  button over the bar.
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::PaintUnder (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    vector<LeadItem>  items = BuildLead();
    float             x     = PlaceLead (text, items);
    int               lead  = (x > m_chipsPx.left) ? static_cast<int> (std::ceil (x)) - m_bandPx.left : 0;



    LayOutBar   (std::min (lead, static_cast<int> (m_chipsPx.right - m_bandPx.left)));
    m_bar.Paint (painter, text, theme);

    painter.FillRect (static_cast<float> (m_bandPx.left), static_cast<float> (m_bandPx.top), static_cast<float> (m_bandPx.right - m_bandPx.left), 1.0f,
                      theme.Divider());

    PaintLead (painter, text, theme, items);
    PaintHelp (text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::BuildLead
//
//  The band's left end in order: the comparison's result while comparing
//  (FR-121), the summary chips (FR-018), a chip for custom decode settings,
//  "Analyzing" while analysis is pending, and the selection.
//
////////////////////////////////////////////////////////////////////////////////

vector<StatusBand::LeadItem> StatusBand::BuildLead() const
{
    vector<LeadItem>  items;
    std::wstring      selection = GetSelectionLine();



    if (m_session.context.hasDisk)
    {
        if (m_session.comparison.IsComparing())
        {
            items.push_back ({ (m_session.comparison.IsBusy() || !m_session.comparison.HasResult()) ? std::wstring (L"Comparing")
                                                                                                   : ComparisonText::FormatResult (m_session.comparison.GetResult()),
                               LeadKind::Result });
        }

        for (const SummaryChip & chip : InspectorText::BuildChips (m_session.analysis.summary))
        {
            items.push_back ({ chip.text, chip.isBad ? LeadKind::BadChip : LeadKind::Chip });
        }

        if (!m_session.analysis.settings.IsStandard())
        {
            items.push_back ({ L"Custom decode settings", LeadKind::Chip });
        }
    }

    if (m_session.pendingRequest != 0 || m_session.scheduler->HasPending())
    {
        items.push_back ({ s_kpszAnalyzing, LeadKind::Analyzing });
    }

    if (!selection.empty())
    {
        items.push_back ({ selection, LeadKind::Selection });
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::PlaceLead
//
//  Where each item goes, and where the hint's room starts. Room for the
//  selection is set aside first; summary chips that would crowd it are left
//  out, and so is a selection wider than the room.
//
////////////////////////////////////////////////////////////////////////////////

float StatusBand::PlaceLead (IDxuiTextRenderer & text, vector<LeadItem> & inOutItems) const
{
    float  pad        = m_scaler.ToPxf (s_kChipPadDip);
    float  gap        = m_scaler.ToPxf (s_kChipGapDip);
    float  x          = static_cast<float> (m_chipsPx.left);
    float  right      = static_cast<float> (m_chipsPx.right);
    float  selectionW = 0;
    float  h          = 0;
    bool   isFull     = false;



    for (LeadItem & item : inOutItems)
    {
        bool  isChip = item.kind == LeadKind::Chip || item.kind == LeadKind::BadChip;

        text.MeasureString (item.text.c_str(), m_scaler.ToPxf (item.kind == LeadKind::Selection ? s_kHintTextDip : s_kChipTextDip),
                            item.kind == LeadKind::Selection ? DxuiTheme::GetUiFace() : DxuiTheme::kBodyFace, item.w, h);

        item.w     += (isChip || item.kind == LeadKind::Result) ? 2 * pad : 0.0f;
        selectionW  = (item.kind == LeadKind::Selection) ? item.w : selectionW;
    }

    for (LeadItem & item : inOutItems)
    {
        bool  isChip = item.kind == LeadKind::Chip || item.kind == LeadKind::BadChip;

        if (item.kind == LeadKind::Selection)
        {
            x += (x > m_chipsPx.left) ? gap : 0.0f;
        }

        isFull        = isFull || (isChip && x + item.w > right - selectionW);
        item.isPlaced = isChip ? !isFull : (item.kind != LeadKind::Selection || x + item.w <= right);
        item.x        = x;

        if (item.isPlaced)
        {
            x += item.w + gap * (isChip || item.kind == LeadKind::Result ? 1.0f : 2.0f);
        }
    }

    return x;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::PaintLead
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::PaintLead (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const vector<LeadItem> & items) const
{
    float     chipH  = m_scaler.ToPxf (s_kChipHeightDip);
    float     textPx = m_scaler.ToPxf (s_kChipTextDip);
    float     y      = (m_chipsPx.top + m_chipsPx.bottom - chipH) / 2.0f;
    uint32_t  bad    = m_session.context.palette.colors.sectorBad;



    for (const LeadItem & item : items)
    {
        if (!item.isPlaced)
        {
            continue;
        }

        switch (item.kind)
        {
            case LeadKind::Result:
                painter.FillRoundedRect (item.x, y, item.w, chipH, chipH / 2, theme.SelectionBackground());
                text.DrawString (item.text.c_str(), item.x, y, item.w, chipH, theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                                 DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
                break;

            case LeadKind::Chip:
            case LeadKind::BadChip:
                painter.FillRoundedRect    (item.x, y, item.w, chipH, chipH / 2, theme.ButtonIdle());
                painter.OutlineRoundedRect (item.x, y, item.w, chipH, chipH / 2, m_scaler.ToPxf (1.0f),
                                            item.kind == LeadKind::BadChip ? bad : theme.ButtonBorder());
                text.DrawString (item.text.c_str(), item.x, y, item.w, chipH, item.kind == LeadKind::BadChip ? bad : theme.Foreground(), textPx,
                                 DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                break;

            case LeadKind::Analyzing:
                text.DrawString (item.text.c_str(), item.x, y, item.w + 1.0f, chipH, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
                                 DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                break;

            case LeadKind::Selection:
                text.DrawString (item.text.c_str(), item.x, static_cast<float> (m_chipsPx.top), item.w + 1.0f, static_cast<float> (m_chipsPx.bottom - m_chipsPx.top),
                                 theme.Foreground(), m_scaler.ToPxf (s_kHintTextDip), DxuiTheme::GetUiFace(), DxuiTextHAlign::Left,
                                 DxuiTextVAlign::CenterOnCapHeight, DxuiFontWeight::Normal, false);
                break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::GetSelectionLine
//
//  What is selected, or nothing with no disk.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StatusBand::GetSelectionLine() const
{
    const InspectorViewModel &  model = m_session.model;



    return m_session.context.hasDisk ? InspectorText::FormatSelection (model.GetTrack(), model.GetQuarterTrack(), model.GetSectorIndex(), model.GetFirstNibble(),
                                                                       model.GetNibbleCount(), m_session.GetFileMap())
                                     : std::wstring();
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
