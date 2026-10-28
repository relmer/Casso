#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorToolbar.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorMetrics.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"





static constexpr int     s_kSegmentDip   = 80;
static constexpr int     s_kRangeDip     = 110;
static constexpr int     s_kControlInset = 5;
static constexpr double  s_kRanges[]     = { 0.01, 0.02, 0.03, 0.05, 0.10, 0.15, 0.20, 0.25 };





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::InspectorToolbar
//
////////////////////////////////////////////////////////////////////////////////

InspectorToolbar::InspectorToolbar (InspectorSession & session, IInspectorComponentHost & host, const InspectorCommands & commands) :
    m_session  (session),
    m_host     (host),
    m_commands (commands)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::Create
//
//  The selectors and the range, then the common commands at the right end;
//  Follow head is a toggle, lit while on.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorToolbar::Create()
{
    DxuiPanel &                 parent = m_host.GetControlParent();
    vector<DxuiToolbar::Entry>  entries;
    vector<std::wstring>        drives;



    m_driveCount = m_session.host->GetDriveCount();

    for (int i = 0; i < m_driveCount; i++)
    {
        drives.push_back (std::format (L"Drive {}", i + 1));
    }

    m_drives     = parent.CreateChild<DxuiSegmentedControl>();
    m_modes      = parent.CreateChild<DxuiSegmentedControl>();
    m_rangeDown  = parent.CreateChild<DxuiButton> (s_kpszMinus);
    m_rangeUp    = parent.CreateChild<DxuiButton> (L"+");
    m_rangeLabel = parent.CreateChild<DxuiLabel> (L"", DxuiTextRole::Body, DxuiTextHAlign::Center);
    m_tools      = parent.CreateChild<DxuiToolbar>();

    m_drives->SetSegments  (std::move (drives));
    m_drives->SetOnChange  ([this] (int index) { (void) m_host.ShowDrive (index); });
    m_modes->SetSegments   ({ L"Structure", L"Timing" });
    m_modes->SetOnChange   ([this] (int index) { m_session.context.isTimingMode = (index == 1); });
    m_rangeDown->SetOnClick ([this] () { StepRange (-1); });
    m_rangeUp->SetOnClick   ([this] () { StepRange (1); });

    if (m_commands.followHead != nullptr && m_driveCount > 0)
    {
        entries.push_back ({ m_commands.followHead, DxuiToolbar::Kind::Toggle, 0 });
    }

    entries.push_back ({ m_commands.goTo, DxuiToolbar::Kind::Command, 1 });
    entries.push_back ({ m_commands.find, DxuiToolbar::Kind::Command, 1 });

    m_tools->SetTextRenderer (m_host.GetTextRenderer());
    m_tools->SetPopupHost    (m_host.GetPopupHost());
    m_tools->SetEntries      (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::GetHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int InspectorToolbar::GetHeightPx (const DxuiDpiScaler & scaler) const
{
    return scaler.ToPx (m_tools->GetBandDp());
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::Layout
//
//  The selectors from the left, each segment wide enough for the widest
//  label; the timing range after them; and the commands from the right end,
//  their labels dropped from the right when the row is short of room.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorToolbar::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    int  margin  = scaler.ToPx (InspectorMetrics::kMarginDip);
    int  inset   = scaler.ToPx (s_kControlInset);
    int  segment = scaler.ToPx (s_kSegmentDip);
    int  button  = scaler.ToPx (InspectorMetrics::kButtonDip);
    int  top     = area.top + inset;
    int  bottom  = area.bottom - inset;
    int  x       = area.left + margin;
    int  tools   = 0;



    m_rowPx = area;

    if (IsDriveShown())
    {
        m_drives->Layout ({ x, top, x + m_driveCount * segment, bottom }, scaler);
        x += m_driveCount * segment + 2 * margin;
    }

    m_modes->Layout ({ x, top, x + 2 * segment, bottom }, scaler);
    x += 2 * segment + margin;

    m_rangeDown->Layout  ({ x,                                         top, x + button,                          bottom }, scaler);
    m_rangeLabel->Layout ({ x + button,                                top, x + scaler.ToPx (s_kRangeDip) - button, bottom }, scaler);
    m_rangeUp->Layout    ({ x + scaler.ToPx (s_kRangeDip) - button,    top, x + scaler.ToPx (s_kRangeDip),      bottom }, scaler);
    x += scaler.ToPx (s_kRangeDip) + margin;

    (void) m_tools->PlanForWidth (std::max (0, static_cast<int> (area.right) - margin - x), scaler);
    tools = m_tools->GetPlannedWidthPx();
    m_tools->Layout ({ area.right - margin - tools, area.top, area.right - margin, area.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::IsDriveShown
//
//  The drive selector is offered only by a host with drives, as Casso is.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorToolbar::IsDriveShown() const
{
    return m_driveCount > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::Update
//
//  The selectors match the window's drive and mode, whatever changed them:
//  a menu command, a key, or a drive switch the user canceled.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorToolbar::Update()
{
    bool  hasDisk  = m_session.context.hasDisk;
    bool  isTiming = m_session.context.isTimingMode;



    m_drives->SetVisible   (IsDriveShown());
    m_drives->SetSelected  (m_session.drive, true);
    m_modes->SetVisible    (hasDisk);
    m_modes->SetSelected   (isTiming ? 1 : 0, true);
    m_rangeDown->SetVisible  (hasDisk && isTiming);
    m_rangeUp->SetVisible    (hasDisk && isTiming);
    m_rangeLabel->SetVisible (hasDisk && isTiming);
    m_rangeDown->SetEnabled  (CanStepRange (-1));
    m_rangeUp->SetEnabled    (CanStepRange (1));
    m_rangeLabel->SetText    (std::wstring (s_kpszPlusMinus) + InspectorFormat::FormatPercent (m_session.context.timingRange).substr (1));
    m_tools->SetVisible      (hasDisk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::Tick
//
////////////////////////////////////////////////////////////////////////////////

void InspectorToolbar::Tick (int64_t nowMs)
{
    m_drives->Tick (nowMs);
    m_modes->Tick  (nowMs);

    if (m_tools->WantsTick())
    {
        m_tools->TickMenus (nowMs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::PaintUnder
//
//  The row's surface, which the commands' strip matches.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorToolbar::PaintUnder (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (text);

    painter.FillRect (static_cast<float> (m_rowPx.left), static_cast<float> (m_rowPx.top), static_cast<float> (m_rowPx.right - m_rowPx.left),
                      static_cast<float> (m_rowPx.bottom - m_rowPx.top), theme.BackgroundElevated());
    m_tools->SetStripColors (theme.BackgroundElevated(), theme.Foreground());
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::OnMouse
//
//  The commands' strip takes its input from its host.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorToolbar::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isLeft    = ev.button == DxuiMouseButton::Left;
    bool  isHandled = false;



    if (m_tools->IsVisible())
    {
        switch (ev.kind)
        {
            case DxuiMouseEventKind::Move:  isHandled = m_tools->OnToolbarMouseMove (ev.positionDip.x, ev.positionDip.y);            break;
            case DxuiMouseEventKind::Leave: m_tools->OnToolbarMouseLeave();                                                          break;
            case DxuiMouseEventKind::Down:  isHandled = isLeft && m_tools->OnToolbarLButtonDown (ev.positionDip.x, ev.positionDip.y); break;
            case DxuiMouseEventKind::Up:    isHandled = isLeft && m_tools->OnToolbarLButtonUp (ev.positionDip.x, ev.positionDip.y);   break;
            default:                                                                                                                  break;
        }
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::GetTip
//
//  Only slot 6's drives are inspected (FR-060). The commands' tips come from
//  the commands.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorToolbar::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    RECT             drives = m_drives->GetBounds();
    RECT             modes  = m_modes->GetBounds();
    RECT             down   = m_rangeDown->GetBounds();
    RECT             up     = m_rangeUp->GetBounds();
    const wchar_t  * tool   = nullptr;
    std::wstring     tip;



    if (m_drives->IsVisible() && PtInRect (&drives, pointPx) && m_session.host->HasOtherDiskController())
    {
        tip         = L"Slot 6 drives only";
        outAnchorPx = drives;
    }
    else if (m_modes->IsVisible() && PtInRect (&modes, pointPx))
    {
        tip         = (m_modes->HitTest (pointPx.x, pointPx.y) == 0) ? L"Structure: color the platter by what each cell contains"
                                                                     : L"Timing: color flux tracks by how fast or slow each cell runs";
        outAnchorPx = modes;
    }
    else if (m_rangeDown->IsVisible() && PtInRect (&down, pointPx))
    {
        tip         = L"Narrow the timing range";
        outAnchorPx = down;
    }
    else if (m_rangeUp->IsVisible() && PtInRect (&up, pointPx))
    {
        tip         = L"Widen the timing range";
        outAnchorPx = up;
    }
    else if (m_tools->IsVisible() && (tool = m_tools->GetTooltipAt (pointPx.x, pointPx.y, outAnchorPx)) != nullptr)
    {
        tip = tool;
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::StepRange
//
//  The timing range steps through ±1% to ±25% (FR-024).
//
////////////////////////////////////////////////////////////////////////////////

void InspectorToolbar::StepRange (int delta)
{
    m_rangeStep                   = std::clamp (m_rangeStep + delta, 0, static_cast<int> (std::size (s_kRanges)) - 1);
    m_session.context.timingRange = s_kRanges[m_rangeStep];
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::CanStepRange
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorToolbar::CanStepRange (int delta) const
{
    int  step = m_rangeStep + delta;



    return step >= 0 && step < static_cast<int> (std::size (s_kRanges));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::SetTimingMode
//
////////////////////////////////////////////////////////////////////////////////

void InspectorToolbar::SetTimingMode (bool isTiming)
{
    m_session.context.isTimingMode = isTiming;
    m_modes->SetSelected (isTiming ? 1 : 0, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::GetPrefs
//
////////////////////////////////////////////////////////////////////////////////

void InspectorToolbar::GetPrefs (DiskInspectorPrefs & inOut) const
{
    inOut.isTimingMode = m_session.context.isTimingMode;
    inOut.timingRange  = m_session.context.timingRange;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar::ApplyPrefs
//
//  The timing range step nearest the saved range.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorToolbar::ApplyPrefs (const DiskInspectorPrefs & prefs)
{
    int  step = 0;



    for (step = 0; step < static_cast<int> (std::size (s_kRanges)); step++)
    {
        m_rangeStep = (std::abs (s_kRanges[step] - prefs.timingRange) < std::abs (s_kRanges[m_rangeStep] - prefs.timingRange)) ? step : m_rangeStep;
    }

    m_session.context.isTimingMode = prefs.isTimingMode;
    m_session.context.timingRange  = s_kRanges[m_rangeStep];
    m_modes->SetSelected (prefs.isTimingMode ? 1 : 0);
}
