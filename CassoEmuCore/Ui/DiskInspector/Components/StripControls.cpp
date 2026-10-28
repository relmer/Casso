#include "Pch.h"

#include "Ui/DiskInspector/Components/StripControls.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorMetrics.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/TrackStripView.h"





static constexpr int  s_kLabelDip      = 260;
static constexpr int  s_kWholeTrackDip = 104;





////////////////////////////////////////////////////////////////////////////////
//
//  StripControls::StripControls
//
////////////////////////////////////////////////////////////////////////////////

StripControls::StripControls (InspectorSession & session, IInspectorComponentHost & host, TrackStripView & strip) :
    m_session (session),
    m_host    (host),
    m_strip   (strip)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripControls::Create
//
////////////////////////////////////////////////////////////////////////////////

void StripControls::Create()
{
    DxuiPanel &  parent = m_host.GetControlParent();



    m_label   = parent.CreateChild<DxuiLabel>  (L"", DxuiTextRole::Body, DxuiTextHAlign::Left);
    m_readout = parent.CreateChild<DxuiLabel>  (L"", DxuiTextRole::Muted, DxuiTextHAlign::Left);
    m_out     = parent.CreateChild<DxuiButton> (s_kpszMinus);
    m_in      = parent.CreateChild<DxuiButton> (L"+");
    m_whole   = parent.CreateChild<DxuiButton> (L"Whole track");

    m_out->SetOnClick   ([this] () { m_strip.ZoomAboutCenter (1.0 / InspectorMetrics::kZoomStep); });
    m_in->SetOnClick    ([this] () { m_strip.ZoomAboutCenter (InspectorMetrics::kZoomStep); });
    m_whole->SetOnClick ([this] () { m_strip.ShowWholeTrack(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripControls::Layout
//
////////////////////////////////////////////////////////////////////////////////

void StripControls::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    int  margin = scaler.ToPx (InspectorMetrics::kMarginDip);
    int  button = scaler.ToPx (InspectorMetrics::kButtonDip);
    int  whole  = scaler.ToPx (s_kWholeTrackDip);
    int  label  = scaler.ToPx (s_kLabelDip);
    int  right  = static_cast<int> (area.right) - whole - 2 * button;
    int  left   = static_cast<int> (area.left) + label + margin;



    m_rowPx = area;
    m_label->Layout   ({ area.left, area.top, area.left + label, area.bottom }, scaler);
    m_readout->Layout ({ left, area.top, std::max (left, right - margin), area.bottom }, scaler);
    m_out->Layout     ({ right,              area.top, right + button,         area.bottom }, scaler);
    m_in->Layout      ({ right + button,     area.top, right + 2 * button,     area.bottom }, scaler);
    m_whole->Layout   ({ right + 2 * button, area.top, area.right,             area.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripControls::Update
//
//  Hidden with no disk; "−" and "Whole track" unavailable while the whole
//  track is in view.
//
////////////////////////////////////////////////////////////////////////////////

void StripControls::Update()
{
    bool                   hasDisk = m_session.context.hasDisk;
    const TrackAnalysis *  track   = m_session.model.GetTrack();



    for (IDxuiControl * control : std::initializer_list<IDxuiControl *> { m_label, m_readout, m_out, m_in, m_whole })
    {
        control->SetVisible (hasDisk);
    }

    m_out->SetEnabled   (m_session.model.GetStripSpan() < 1.0);
    m_whole->SetEnabled (m_session.model.GetStripSpan() < 1.0);

    m_label->SetText (std::format (L"Track {} {} one turn from the index", InspectorFormat::FormatQuarterTrack (m_session.model.GetQuarterTrack()), s_kpszMiddleDot));
    m_readout->SetText (m_strip.GetReadout() + ((track != nullptr && m_session.model.GetNibbleCount() > 0)
                        ? L"    " + InspectorText::FormatSelection (*track, m_session.model.GetFirstNibble(), m_session.model.GetNibbleCount()) : L""));
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripControls::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool StripControls::Contains (POINT pointPx) const
{
    return PtInRect (&m_rowPx, pointPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripControls::GetTip
//
//  Each button's tip, and for one that can be unavailable, why it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StripControls::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    struct ControlTip
    {
        const IDxuiControl  * control;
        LPCWSTR               text;
        LPCWSTR               whyUnavailable;
    };

    const ControlTip  tips[] =
    {
        { m_out,   L"Zoom the strip out",                L"The whole track is already in view" },
        { m_in,    L"Zoom the strip in",                 nullptr },
        { m_whole, L"Show the whole track in the strip", L"The whole track is already in view" },
    };


    std::wstring  tip;



    for (const ControlTip & entry : tips)
    {
        RECT  bounds = entry.control->GetBounds();

        if (tip.empty() && entry.control->IsVisible() && PtInRect (&bounds, pointPx))
        {
            tip         = (entry.control->IsEnabled() || entry.whyUnavailable == nullptr) ? entry.text : entry.whyUnavailable;
            outAnchorPx = bounds;
        }
    }

    return tip;
}
