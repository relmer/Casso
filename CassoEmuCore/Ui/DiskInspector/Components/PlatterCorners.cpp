#include "Pch.h"

#include "Ui/DiskInspector/Components/PlatterCorners.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorMetrics.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/FloatingGroup.h"
#include "Ui/DiskInspector/OverlayChip.h"
#include "Ui/DiskInspector/PlatterLegendView.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/TiltButton.h"





static constexpr int      s_kGroupDip       = 36;
static constexpr int      s_kPadDip         = 4;
static constexpr int      s_kGapDip         = 6;
static constexpr int      s_kIconDip        = 28;
static constexpr int      s_kSliderDip      = 120;
static constexpr int      s_kSliderMarginDip = 16;
static constexpr int      s_kInsetDip       = 4;
static constexpr int64_t  s_kLegendReopenMs = 300;
static constexpr int      s_kLegendWidthDip = 480;
static constexpr double   s_kRadiansPerDeg  = 0.017453292519943295;
static constexpr LPCWSTR  s_kpszGlyphLegend = L"\xE790";
static constexpr LPCWSTR  s_kpszGlyphMinus  = L"\xE738";
static constexpr LPCWSTR  s_kpszGlyphPlus   = L"\xE710";
static constexpr LPCWSTR  s_kpszTiltTip     = L"Tilt: click for flat or tilted, drag the slider or right-drag the disk to set the angle";





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::PlatterCorners
//
////////////////////////////////////////////////////////////////////////////////

PlatterCorners::PlatterCorners (InspectorSession & session, IInspectorComponentHost & host, PlatterView & platter) :
    m_session (session),
    m_host    (host),
    m_platter (platter)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::Create
//
//  The groups are created after the platter, so they are in front of it and
//  take the pointer first.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::Create()
{
    DxuiPanel &  parent = m_host.GetControlParent();



    m_legendGroup  = parent.CreateChild<FloatingGroup>();
    m_tiltGroup    = parent.CreateChild<FloatingGroup>();
    m_chipsGroup   = parent.CreateChild<FloatingGroup>();
    m_zoomGroup    = parent.CreateChild<FloatingGroup>();

    m_legendButton = m_legendGroup->CreateChild<DxuiIconButton>();
    m_legend       = parent.CreateChild<PlatterLegendView> (m_session.context);
    m_tilt         = m_tiltGroup->CreateChild<TiltButton>();
    m_tiltSlider   = m_tiltGroup->CreateChild<DxuiSlider>();
    m_filesChip    = m_chipsGroup->CreateChild<OverlayChip> (L"Files");
    m_readsChip    = m_chipsGroup->CreateChild<OverlayChip> (L"Reads");
    m_writesChip   = m_chipsGroup->CreateChild<OverlayChip> (L"Writes");
    m_scansChip    = m_chipsGroup->CreateChild<OverlayChip> (L"Scans");
    m_alignChip    = m_chipsGroup->CreateChild<OverlayChip> (L"Track alignment");
    m_diffsChip    = m_chipsGroup->CreateChild<OverlayChip> (L"Differences");
    m_zoomOut      = m_zoomGroup->CreateChild<DxuiIconButton>();
    m_zoomIn       = m_zoomGroup->CreateChild<DxuiIconButton>();

    m_legend->SetVisible (false);

    m_legendButton->SetGlyph          (s_kpszGlyphLegend);
    m_legendButton->SetRound          (true);
    m_legendButton->SetAccessibleName (L"Legend");
    m_legendButton->SetOnClick ([this] ()
                                {
                                    if (m_isLegendPinned)
                                    {
                                        HideLegend();
                                    }
                                    else if (static_cast<int64_t> (GetTickCount64()) - m_legendClosedMs > s_kLegendReopenMs)
                                    {
                                        ShowLegend (true);
                                    }
                                });

    m_tilt->SetOnClick  ([this] () { ToggleTilt(); });
    m_tilt->SetIsTilted ([this] () { return m_session.model.IsTilted(); });

    m_tiltSlider->SetVertical   (true);
    m_tiltSlider->SetRange      (0.0f, static_cast<float> (InspectorViewModel::kMaxTilt / s_kRadiansPerDeg));
    m_tiltSlider->SetStep       (1.0f);
    m_tiltSlider->SetShowValue  (false);
    m_tiltSlider->SetShowTicks  (false);
    m_tiltSlider->SetVisible    (false);
    m_tiltSlider->SetOnChange   ([this] (float degrees) { m_session.model.SetTilt (degrees * s_kRadiansPerDeg); });

    m_filesChip->SetOnChange ([this] (bool isOn) { SetOverlay (PlatterOverlay::Files,       isOn); });
    m_readsChip->SetOnChange ([this] (bool isOn) { SetOverlay (PlatterOverlay::Reads,       isOn); });
    m_writesChip->SetOnChange ([this] (bool isOn) { SetOverlay (PlatterOverlay::Writes,     isOn); });
    m_scansChip->SetOnChange ([this] (bool isOn) { SetOverlay (PlatterOverlay::Scans,       isOn); });
    m_alignChip->SetOnChange ([this] (bool isOn) { SetOverlay (PlatterOverlay::Alignment,   isOn); });
    m_diffsChip->SetOnChange ([this] (bool isOn) { SetOverlay (PlatterOverlay::Differences, isOn); });

    m_zoomOut->SetGlyph          (s_kpszGlyphMinus);
    m_zoomOut->SetRound          (true);
    m_zoomOut->SetAccessibleName (L"Zoom out");
    m_zoomOut->SetOnClick        ([this] () { m_platter.ZoomAboutCenter (1.0 / InspectorMetrics::kZoomStep); });
    m_zoomIn->SetGlyph           (s_kpszGlyphPlus);
    m_zoomIn->SetRound           (true);
    m_zoomIn->SetAccessibleName  (L"Zoom in");
    m_zoomIn->SetOnClick         ([this] () { m_platter.ZoomAboutCenter (InspectorMetrics::kZoomStep); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::GetChipsWidthPx
//
//  The overlay group's width with every chip offered now.
//
////////////////////////////////////////////////////////////////////////////////

int PlatterCorners::GetChipsWidthPx (const DxuiDpiScaler & scaler) const
{
    IDxuiTextRenderer  * text  = m_host.GetTextRenderer();
    int                  width = 2 * scaler.ToPx (s_kPadDip);
    int                  count = 0;



    for (const OverlayChip * chip : { m_filesChip, m_readsChip, m_writesChip, m_scansChip, m_alignChip, m_diffsChip })
    {
        if (chip->IsVisible() && text != nullptr)
        {
            width += chip->GetWidthPx (*text, scaler);
            count++;
        }
    }

    return width + std::max (0, count - 1) * scaler.ToPx (s_kGapDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::GetZoomWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int PlatterCorners::GetZoomWidthPx (const DxuiDpiScaler & scaler) const
{
    return 3 * scaler.ToPx (s_kPadDip) + 2 * scaler.ToPx (s_kIconDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::GetMinWidthPx
//
//  The platter column's least width: the bottom groups side by side, so the
//  overlay chips never run under the zoom group.
//
////////////////////////////////////////////////////////////////////////////////

int PlatterCorners::GetMinWidthPx (const DxuiDpiScaler & scaler) const
{
    return GetChipsWidthPx (scaler) + GetZoomWidthPx (scaler) + 3 * scaler.ToPx (s_kInsetDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::Layout
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    m_areaPx = area;
    m_scaler = scaler;
    Place();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::Place
//
//  Each group inset in its corner of the platter column. The tilt group
//  grows downward to take the slider while the slider is out.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::Place()
{
    int                  inset  = m_scaler.ToPx (s_kInsetDip);
    int                  group  = m_scaler.ToPx (s_kGroupDip);
    int                  pad    = m_scaler.ToPx (s_kPadDip);
    int                  gap    = m_scaler.ToPx (s_kGapDip);
    int                  icon   = m_scaler.ToPx (s_kIconDip);
    int                  slider = m_isSliderOut ? m_scaler.ToPx (s_kSliderDip) : 0;
    int                  left   = m_areaPx.left + inset;
    int                  top    = m_areaPx.top + inset;
    int                  right  = m_areaPx.right - inset;
    int                  bottom = m_areaPx.bottom - inset;
    int                  chipsW = GetChipsWidthPx (m_scaler);
    int                  zoomW  = GetZoomWidthPx (m_scaler);
    int                  x      = 0;
    int                  w      = 0;
    IDxuiTextRenderer  * text   = m_host.GetTextRenderer();



    m_legendGroup->Layout  ({ left, top, left + group, top + group }, m_scaler);
    m_legendButton->Layout ({ left + pad, top + pad, left + group - pad, top + group - pad }, m_scaler);

    m_tiltGroup->Layout  ({ right - group, top, right, top + group + slider }, m_scaler);
    m_tilt->Layout       ({ right - group + pad, top + pad, right - pad, top + group - pad }, m_scaler);
    m_tiltSlider->Layout ({ right - group + pad, top + group, right - pad, top + group + slider - pad }, m_scaler);

    m_chipsGroup->Layout ({ left, bottom - group, left + chipsW, bottom }, m_scaler);
    x = left + pad;

    for (OverlayChip * chip : { m_filesChip, m_readsChip, m_writesChip, m_scansChip, m_alignChip, m_diffsChip })
    {
        w = (chip->IsVisible() && text != nullptr) ? chip->GetWidthPx (*text, m_scaler) : 0;
        chip->Layout ({ x, bottom - group + pad, x + w, bottom - pad }, m_scaler);
        x += (w > 0) ? w + gap : 0;
    }

    x = right - zoomW;
    m_zoomGroup->Layout   ({ x, bottom - group, right, bottom }, m_scaler);
    m_zoomOut->Layout     ({ x + pad, bottom - group + pad, x + pad + icon, bottom - pad }, m_scaler);
    m_zoomIn->Layout      ({ right - pad - icon, bottom - group + pad, right - pad, bottom - pad }, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::Update
//
//  The groups are hidden with no disk; the chips match their overlays, so a
//  View menu change appears on them; and the slider is out while the tilt
//  button or the slider is under the pointer or has the focus.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::Update()
{
    bool  isShown      = m_session.context.hasDisk && !m_session.is35;
    bool  isReads      = isShown && m_session.host->GetDriveCount() > 0 && m_session.IsWindowDiskA();
    bool  isDiffs      = isShown && m_session.comparison.IsComparing();
    bool  isRelayout   = isReads != m_readsChip->IsVisible() || isDiffs != m_diffsChip->IsVisible();
    bool  isSliderOut  = isShown && IsSliderWanted();



    for (FloatingGroup * group : { m_legendGroup, m_tiltGroup, m_chipsGroup, m_zoomGroup })
    {
        group->SetVisible (isShown);
    }

    m_readsChip->SetVisible (isReads);
    m_writesChip->SetVisible (isReads);
    m_scansChip->SetVisible  (isReads);
    m_diffsChip->SetVisible (isDiffs);
    m_filesChip->SetOn (m_session.context.isFilesOverlay);
    m_readsChip->SetOn (m_session.context.isReadsOverlay);
    m_writesChip->SetOn (m_session.context.isWritesOverlay);
    m_scansChip->SetOn  (m_session.context.isScansOverlay);
    m_alignChip->SetOn (m_isAlignment);
    m_diffsChip->SetOn (m_session.context.isDiffsOverlay);

    //  A disk with a volume the map reads opens on the Files overlay, the
    //  most telling view of it; after that, Files is on or off as the user sets it.
    if (!m_isFilesDefaulted && m_session.GetFileMap() != nullptr && m_session.GetFileMap()->notMapped == NotMappedReason::None)
    {
        m_isFilesDefaulted = true;
        SetOverlay (PlatterOverlay::Files, true);
    }

    m_zoomOut->SetEnabled  (!m_session.model.IsAtFit());

    if (!m_tiltSlider->IsDragging())
    {
        m_tiltSlider->SetValue (static_cast<float> (m_session.model.GetTilt() / s_kRadiansPerDeg));
    }

    //  The last tilt the user set, for the tilt button and View > "Tilted".
    m_lastTilt = m_session.model.IsTilted() ? m_session.model.GetTilt() : m_lastTilt;

    if (isSliderOut != m_isSliderOut)
    {
        m_isSliderOut = isSliderOut;
        m_tiltSlider->SetVisible (isSliderOut);
        isRelayout = true;
    }

    if (isRelayout && m_scaler.GetDpi() != 0)
    {
        Place();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::IsSliderWanted
//
//  The slider comes out with the tilt button under the pointer or focused,
//  and stays out while the pointer is over the tilt group or within
//  s_kSliderMarginDip of it, from where the cursor is, not from the last
//  mouse event: a tooltip popping up under the pointer ends the window's
//  hover, which would otherwise hide the slider mid-use.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterCorners::IsSliderWanted() const
{
    RECT   area   = m_tiltGroup->GetBounds();
    POINT  cursor = {};
    bool   isNear = false;
    int    margin = m_scaler.ToPx (s_kSliderMarginDip);



    if (m_isSliderOut && m_host.GetOwnerHwnd() != nullptr && GetCursorPos (&cursor) && ScreenToClient (m_host.GetOwnerHwnd(), &cursor))
    {
        InflateRect (&area, margin, margin);
        isNear = PtInRect (&area, cursor) != FALSE;
    }

    return m_tilt->IsHovered() || m_tilt->IsFocused() || m_tiltSlider->IsFocused() || m_tiltSlider->IsDragging() || isNear;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterCorners::Contains (POINT pointPx) const
{
    bool  isIn = false;



    for (const FloatingGroup * group : { m_legendGroup, m_tiltGroup, m_chipsGroup, m_zoomGroup })
    {
        isIn = isIn || (group != nullptr && group->Contains (pointPx));
    }

    return isIn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::OnMouse
//
//  The groups take the pointer as the window's children; here the legend
//  previews under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterCorners::OnMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        TrackLegendHover (ev.kind == DxuiMouseEventKind::Leave ? POINT { SHRT_MIN, SHRT_MIN } : ev.positionDip);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::GetTip
//
//  Each control's tip, and for one that can be unavailable, why it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PlatterCorners::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    struct ControlTip
    {
        const IDxuiControl  * control;
        LPCWSTR               text;
        LPCWSTR               whyUnavailable;
    };

    const ControlTip  tips[] =
    {
        { m_legendButton, L"Legend: what the platter's colors and patterns mean",                     nullptr },
        { m_tilt,         s_kpszTiltTip,                                                               nullptr },
        { m_tiltSlider,   L"Tilt angle",                                                               nullptr },
        { m_filesChip,    L"Color each sector's data field by its role in the file map",               nullptr },
        { m_readsChip,    L"Shade each sector by how many times the guest read its data since the disk was inserted",           nullptr },
        { m_writesChip,   L"Shade each sector by how many times the guest wrote it since the disk was inserted",                nullptr },
        { m_scansChip,    L"Shade each sector by how many times the guest read its address field while searching for a sector", nullptr },
        { m_alignChip,    L"Mark where sector 0 and the longest sync start on each track",             nullptr },
        { m_diffsChip,    L"Mark the quarter tracks that differ",                                      nullptr },
        { m_zoomOut,      L"Zoom the platter out (minus)",                                             L"The whole disk is shown, so the platter cannot zoom out further" },
        { m_zoomIn,       L"Zoom the platter in (plus)",                                               nullptr },
    };


    std::wstring  tip;



    for (const ControlTip & entry : tips)
    {
        RECT  bounds = entry.control->GetBounds();

        if (tip.empty() && entry.control->IsVisible() && entry.control->GetParent() != nullptr && entry.control->GetParent()->IsVisible() &&
            PtInRect (&bounds, pointPx))
        {
            tip         = (entry.control->IsEnabled() || entry.whyUnavailable == nullptr) ? entry.text : entry.whyUnavailable;
            outAnchorPx = bounds;
        }
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::IsOverlayOn
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterCorners::IsOverlayOn (PlatterOverlay overlay) const
{
    bool  isOn = false;



    switch (overlay)
    {
        case PlatterOverlay::Files:       isOn = m_session.context.isFilesOverlay; break;
        case PlatterOverlay::Reads:       isOn = m_session.context.isReadsOverlay; break;
        case PlatterOverlay::Writes:      isOn = m_session.context.isWritesOverlay; break;
        case PlatterOverlay::Scans:       isOn = m_session.context.isScansOverlay;  break;
        case PlatterOverlay::Alignment:   isOn = m_isAlignment;                    break;
        case PlatterOverlay::Differences: isOn = m_session.context.isDiffsOverlay; break;
    }

    return isOn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::SetOverlay
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::SetOverlay (PlatterOverlay overlay, bool isOn)
{
    switch (overlay)
    {
        case PlatterOverlay::Files:       m_session.context.isFilesOverlay = isOn; m_filesChip->SetOn (isOn); break;
        case PlatterOverlay::Reads:       m_session.context.isReadsOverlay = isOn; m_readsChip->SetOn (isOn); break;
        case PlatterOverlay::Writes:      m_session.context.isWritesOverlay = isOn; m_writesChip->SetOn (isOn); break;
        case PlatterOverlay::Scans:       m_session.context.isScansOverlay  = isOn; m_scansChip->SetOn  (isOn); break;
        case PlatterOverlay::Alignment:   m_isAlignment = isOn; m_platter.SetAlignmentShown (isOn); m_alignChip->SetOn (isOn); break;
        case PlatterOverlay::Differences: m_session.context.isDiffsOverlay = isOn; m_diffsChip->SetOn (isOn); break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::ToggleTilt
//
//  Flat to the last tilt, and any tilt to flat (FR-151).
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::ToggleTilt()
{
    m_session.model.SetTilt (m_session.model.IsTilted() ? 0.0 : m_lastTilt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::ShowLegend
//
//  The legend in a popup below the "Legend" button (FR-030). A preview on
//  hover lets the pointer through and closes when the pointer moves off the
//  button; a pinned one, after a click, stays until the next click outside
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::ShowLegend (bool isPinned)
{
    HRESULT                    hr       = S_OK;
    DxuiHwndSource           * host     = m_host.GetPopupHost();
    DxuiPopupHost::ShowParams  params;
    RECT                       button   = m_legendButton->GetBounds();
    POINT                      topLeft  = { button.left,  button.top };
    POINT                      botRight = { button.right, button.bottom };
    int                        width    = (m_host.GetTextRenderer() != nullptr) ? PlatterLegendView::GetWidthDip (*m_host.GetTextRenderer(), m_session.context) : s_kLegendWidthDip;



    HideLegend();
    BAIL_OUT_IF (host == nullptr, S_OK);

    m_legendPopup = host->AcquirePopup();
    BAIL_OUT_IF (m_legendPopup == nullptr, S_OK);

    ClientToScreen (m_host.GetOwnerHwnd(), &topLeft);
    ClientToScreen (m_host.GetOwnerHwnd(), &botRight);

    params.ownerHwnd        = m_host.GetOwnerHwnd();
    params.anchorRectScreen = { topLeft.x, topLeft.y, botRight.x, botRight.y };
    params.placement        = DxuiPopupPlacement::Below;
    params.flipIfOffscreen  = true;
    params.dismiss          = isPinned ? DxuiPopupDismiss::OnClickOutside : DxuiPopupDismiss::Manual;
    params.input            = isPinned ? DxuiPopupInput::Interactive : DxuiPopupInput::PassThrough;
    params.sizeDip          = { width, m_legend->GetHeightDip() };
    params.backgroundArgb   = m_session.theme->BackgroundElevated();
    params.renderContent    = [this, width] (IDxuiPainter & painter, IDxuiTextRenderer & text)
                              {
                                  m_legend->Layout ({ 0, 0, m_session.scaler.ToPx (width), m_session.scaler.ToPx (m_legend->GetHeightDip()) }, m_session.scaler);
                                  m_legend->Paint  (painter, text, *m_session.theme);
                              };
    params.onClosed         = [this] ()
                              {
                                  m_legendPopup    = nullptr;
                                  m_isLegendPinned = false;
                                  m_legendClosedMs = static_cast<int64_t> (GetTickCount64());
                              };

    hr = m_legendPopup->Show (std::move (params));
    CHRF (hr, host->ReleasePopup (m_legendPopup); m_legendPopup = nullptr);

    m_isLegendPinned = isPinned;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::HideLegend
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::HideLegend()
{
    DxuiPopupHost  * popup = m_legendPopup;



    m_legendPopup    = nullptr;
    m_isLegendPinned = false;

    if (popup != nullptr && m_host.GetPopupHost() != nullptr)
    {
        m_host.GetPopupHost()->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::TrackLegendHover
//
//  The pointer over the "Legend" button previews the legend. Moving off the
//  button closes the preview; a pinned legend stays open.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::TrackLegendHover (POINT pointPx)
{
    RECT  button = m_legendButton->GetBounds();
    bool  isOver = m_legendGroup->IsVisible() && PtInRect (&button, pointPx);



    if (isOver && m_legendPopup == nullptr)
    {
        ShowLegend (false);
    }
    else if (!isOver && m_legendPopup != nullptr && !m_isLegendPinned)
    {
        HideLegend();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::OnDiskStarted
//
//  A saved Files choice applies to every disk (FR-080).
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::OnDiskStarted (const DiskChange & change)
{
    if (change.hasDisk)
    {
        m_isFilesDefaulted = m_isFilesSaved;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::GetPrefs
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::GetPrefs (DiskInspectorPrefs & inOut) const
{
    inOut.isAlignment = m_isAlignment;
    inOut.isFiles     = m_session.context.isFilesOverlay;
    inOut.isReads     = m_session.context.isReadsOverlay;
    inOut.isWrites    = m_session.context.isWritesOverlay;
    inOut.isScans     = m_session.context.isScansOverlay;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners::ApplyPrefs
//
//  Saved prefs mean the user has chosen Files or not, so a new disk does not
//  turn it on.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterCorners::ApplyPrefs (const DiskInspectorPrefs & prefs)
{
    m_isFilesDefaulted = prefs.w > 0;
    m_isFilesSaved     = prefs.w > 0;

    SetOverlay (PlatterOverlay::Files,     prefs.isFiles);
    SetOverlay (PlatterOverlay::Reads,     prefs.isReads);
    SetOverlay (PlatterOverlay::Writes,    prefs.isWrites);
    SetOverlay (PlatterOverlay::Scans,     prefs.isScans);
    SetOverlay (PlatterOverlay::Alignment, prefs.isAlignment);
}
