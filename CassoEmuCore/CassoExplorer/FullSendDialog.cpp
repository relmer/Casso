#include "Pch.h"

#include "CassoExplorer/FullSendDialog.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::GetColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FullSendPanel::GetColor (FullSendTrackMark mark, const IDxuiTheme & theme)
{
    switch (mark)
    {
        case FullSendTrackMark::Read:       return 0xFF3E7EBF;
        case FullSendTrackMark::Verified:   return 0xFF2EA043;
        case FullSendTrackMark::Unreliable: return 0xFFD29922;
        case FullSendTrackMark::Unreadable: return 0xFFDA3633;
        default:                            return theme.BackgroundElevated();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::GetColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FullSendPanel::GetColor (FullSendPositionMark mark, const IDxuiTheme & theme)
{
    switch (mark)
    {
        case FullSendPositionMark::Read:     return 0xFF3E7EBF;
        case FullSendPositionMark::Skipped:  return 0xFF6E6E6E;
        case FullSendPositionMark::Weak:     return 0xFFD29922;
        case FullSendPositionMark::Unstable: return 0xFFE07B39;
        default:                             return theme.BackgroundElevated();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::FormatSeconds
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::FormatSeconds (double seconds)
{
    int  whole = (int) std::max (0.0, seconds + 0.5);



    if (whole >= 3600)
    {
        return std::format (L"{}:{:02}:{:02}", whole / 3600, whole / 60 % 60, whole % 60);
    }

    return std::format (L"{}:{:02}", whole / 60, whole % 60);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::DescribeActivity
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::DescribeActivity (const FullSendSnapshot & snapshot)
{
    switch (snapshot.activity)
    {
        case FullSendActivity::Starting:        return L"Waiting for Space on the Apple to begin.";
        case FullSendActivity::Checking:        return std::format (L"Checking the disk in drive {}.", snapshot.drive);
        case FullSendActivity::Sending:         return std::format (L"Sending the disk in drive {}.", snapshot.drive);
        case FullSendActivity::Capturing:       return std::format (L"The disk in drive {} is likely copy-protected. Capturing it raw.", snapshot.drive);
        case FullSendActivity::WaitingForDisk:  return L"Waiting for a new disk. Insert one and press Space on the Apple.";
        case FullSendActivity::WaitingForApple: return std::format (L"Waiting for the Apple to reply, {} so far.", FormatSeconds (snapshot.waited));
        case FullSendActivity::EditingPrefix:   return L"A file name prefix is being typed on the Apple.";
        case FullSendActivity::Ended:           return L"The session ended. " + TextEncoding::NarrowToWide (snapshot.reason);
        default:                                return L"No Full Send is in progress. Press F on the Apple's CassoLink screen to start one.";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::DescribeDrive
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::DescribeDrive (const FullSendDriveStatus & drive)
{
    std::wstring  state;



    switch (drive.state)
    {
        case FullSendDiskState::Checking:  state = L"Checking";                              break;
        case FullSendDiskState::Sending:   state = drive.verified ? L"Sending, verified" : L"Sending"; break;
        case FullSendDiskState::Capturing: state = L"Capturing raw";                         break;
        case FullSendDiskState::Sent:      state = L"Sent";                                  break;
        case FullSendDiskState::Duplicate: state = L"Duplicate";                             break;
        case FullSendDiskState::Empty:     state = L"Empty, or no recognized sectors";       break;
        default:                           state = L"No disk read yet";                      break;
    }

    if (!drive.fileSystem.empty())
    {
        state += L", " + TextEncoding::NarrowToWide (drive.fileSystem);
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::DescribeTimes
//
//  Empty for a drive with no disk begun.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::DescribeTimes (const FullSendDriveStatus & drive)
{
    bool  busy = drive.state == FullSendDiskState::Sending || drive.state == FullSendDiskState::Capturing;
    bool  done = drive.state == FullSendDiskState::Sent;



    if (done)
    {
        return std::format (L"Took {}", FormatSeconds (drive.elapsed));
    }

    if (!busy)
    {
        return std::wstring();
    }

    if (drive.secondsLeft < 0.0)
    {
        return std::format (L"{} elapsed", FormatSeconds (drive.elapsed));
    }

    return std::format (L"{} elapsed, about {} left", FormatSeconds (drive.elapsed), FormatSeconds (drive.secondsLeft));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::DescribeCapture
//
//  Where a raw capture is, and once it is done, its time on the disk and on
//  the line. Empty for a disk not captured raw.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::DescribeCapture (const FullSendDriveStatus & drive)
{
    if (drive.positions.empty())
    {
        return std::wstring();
    }

    if (drive.state != FullSendDiskState::Capturing)
    {
        return std::format (L"Raw capture: {} on the disk, {} on the line", FormatSeconds (drive.diskSeconds), FormatSeconds (drive.lineSeconds));
    }

    if (drive.reads == 0)
    {
        return L"Raw capture: starting";
    }

    return std::format (L"Raw capture: {}, track {:.2f}, read {} of {}", drive.pass == 1 ? L"whole tracks" : L"between tracks",
                        drive.position / 4.0, drive.read, drive.reads);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::DescribeTotals
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::DescribeTotals (const FullSendTotals & totals)
{
    std::wstring  text = std::format (L"{} sent, {} {}, {} raw {}, {} with bad sectors", totals.sent,
                                      totals.duplicates, totals.duplicates == 1 ? L"duplicate" : L"duplicates",
                                      totals.rawCaptures, totals.rawCaptures == 1 ? L"capture" : L"captures", totals.badSectors);



    text += std::format (L"    {} elapsed", FormatSeconds (totals.elapsed));

    if (totals.sent != 0)
    {
        text += std::format (L", {} a disk, {:.1f} MB", FormatSeconds (totals.average), (double) totals.bytes / (1024.0 * 1024.0));
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::DescribeStatus
//
//  A manifest row's status in words.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::DescribeStatus (const FullSendRow & row)
{
    if (row.status == FullSendManifest::kStatusDuplicate)
    {
        return L"Duplicate of " + TextEncoding::NarrowToWide (row.duplicateOf);
    }

    if (row.status == FullSendManifest::kStatusRawCapture)
    {
        return L"Raw capture";
    }

    if (row.status == FullSendManifest::kStatusProtected)
    {
        return L"Likely copy-protected";
    }

    if (row.unreadable != 0)
    {
        return std::format (L"{} unreadable {}", row.unreadable, row.unreadable == 1 ? L"sector" : L"sectors");
    }

    if (row.unreliable != 0)
    {
        return std::format (L"{} unreliable {}", row.unreliable, row.unreliable == 1 ? L"sector" : L"sectors");
    }

    return row.verification == FullSendManifest::kVerifyFull ? L"Sent, verified" : L"Sent";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::SetView
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::SetView (View view)
{
    m_view = std::move (view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsPx);

    m_scaler = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::DrawLine
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::DrawLine (IDxuiTextRenderer & text, const std::wstring & line, float left, float top, float width, uint32_t color, bool heading, DxuiTextHAlign align) const
{
    HRESULT  hr = S_OK;



    hr = text.DrawString (line.c_str(), left, top, width, m_scaler.ToPxf (heading ? kHeadingDip : kLineDip),
                          color, m_scaler.ToPxf (heading ? kHeadingFontDip : kFontDip), s_kpszFont,
                          align, DxuiTextVAlign::Center, heading ? DxuiFontWeight::SemiBold : DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::Paint
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT   bounds  = GetBounds();
    float  left    = (float) bounds.left;
    float  width   = (float) (bounds.right - bounds.left);
    float  gap     = m_scaler.ToPxf (kColumnGapDip);
    float  section = m_scaler.ToPxf (kSectionGapDip);
    float  column  = (width - gap) * 0.5f;
    float  top     = (float) bounds.top;
    float  first   = 0.0f;
    float  second  = 0.0f;



    if (!IsVisible() || width < 1.0f)
    {
        return;
    }

    DrawLine (text, DescribeActivity (m_view.snapshot), left, top, width, theme.Foreground(), true);

    if (!m_view.connection.empty())
    {
        DrawLine (text, m_view.connection, left, top, width, theme.ForegroundMuted(), false, DxuiTextHAlign::Right);
    }

    top += m_scaler.ToPxf (kHeadingDip) + section;

    first  = PaintDrive (painter, text, theme, 1, m_view.snapshot.drives[0], left, top, column);
    second = PaintDrive (painter, text, theme, 2, m_view.snapshot.drives[1], left + column + gap, top, column);
    top    = std::max (first, second) + section * 0.5f;

    top = PaintLegend (painter, text, theme, top) + section;

    DrawLine (text, DescribeTotals (m_view.snapshot.totals), left, top, width, theme.Foreground());
    top += m_scaler.ToPxf (kLineDip) + section;

    top = PaintDisks (text, theme, top) + section;

    PaintActivity (text, theme, top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::PaintDrive
//
//  A drive's heading and state, its disk's title, the map of its tracks and
//  in a raw capture its quarter tracks, its progress bar, and its times.
//  Returns the bottom.
//
////////////////////////////////////////////////////////////////////////////////

float FullSendPanel::PaintDrive (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int number, const FullSendDriveStatus & drive, float left, float top, float width) const
{
    float         lineH   = m_scaler.ToPxf (kLineDip);
    float         cellH   = m_scaler.ToPxf (kMapCellDip);
    float         rawH    = m_scaler.ToPxf (kMapRawDip);
    float         barH    = m_scaler.ToPxf (kBarDip);
    float         space   = std::max (1.0f, std::floor (m_scaler.ToPxf (1.0f)));
    float         pitch   = std::floor (width / FullSendDriveStatus::kTracks);
    float         quarter = pitch / 4.0f;
    bool          busy    = drive.state == FullSendDiskState::Sending || drive.state == FullSendDiskState::Capturing;
    std::wstring  title   = drive.title.empty() ? std::wstring (L"No disk") : TextEncoding::NarrowToWide (drive.title);



    DrawLine (text, std::format (L"Drive {}", number), left, top, width, theme.HeadingForeground(), true);
    DrawLine (text, DescribeDrive (drive), left, top, width, theme.ForegroundMuted(), false, DxuiTextHAlign::Right);
    top += m_scaler.ToPxf (kHeadingDip);

    DrawLine (text, title, left, top, width, theme.Foreground());
    top += lineH + space * 2.0f;

    //  In a raw capture a track's cell is its whole-track read's, as the
    //  sector read before it found the tracks unreadable.
    for (int track = 0; track < FullSendDriveStatus::kTracks; track++)
    {
        uint32_t  color = drive.positions.empty() ? GetColor (drive.tracks[(size_t) track], theme) : GetColor (drive.positions[(size_t) track * 4], theme);

        painter.FillRect (left + track * pitch, top, pitch - space, cellH, color);
    }

    top += cellH + space;

    //  A quarter track's cell is under its track, track 34 having one.
    for (size_t position = 0; position < drive.positions.size(); position++)
    {
        painter.FillRect (left + position * quarter, top, std::max (1.0f, quarter - space), rawH, GetColor (drive.positions[position], theme));
    }

    top += rawH + space * 4.0f;

    painter.FillRoundedRect (left, top, pitch * FullSendDriveStatus::kTracks - space, barH, barH * 0.5f, theme.BackgroundElevated());

    if (busy || drive.state == FullSendDiskState::Sent)
    {
        painter.FillRoundedRect (left, top, (pitch * FullSendDriveStatus::kTracks - space) * (float) std::clamp (drive.percent, 0, 100) / 100.0f, barH, barH * 0.5f, theme.Accent());
    }

    top += barH + space * 2.0f;

    DrawLine (text, DescribeTimes (drive), left, top, width, theme.ForegroundMuted());
    top += lineH;

    DrawLine (text, DescribeCapture (drive), left, top, width, theme.ForegroundMuted());

    return top + lineH;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::PaintLegend
//
//  One row: the tracks' marks, then the quarter tracks', each as wide as
//  its label. Returns the bottom.
//
////////////////////////////////////////////////////////////////////////////////

float FullSendPanel::PaintLegend (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const
{
    struct Entry
    {
        uint32_t         color;
        const wchar_t  * label;
    };

    const Entry  entries[] =
    {
        { GetColor (FullSendTrackMark::Read, theme),          L"Read" },
        { GetColor (FullSendTrackMark::Verified, theme),      L"Verified" },
        { GetColor (FullSendTrackMark::Unreliable, theme),    L"Unreliable or weak" },
        { GetColor (FullSendTrackMark::Unreadable, theme),    L"Unreadable" },
        { GetColor (FullSendPositionMark::Skipped, theme),    L"Skipped" },
        { GetColor (FullSendPositionMark::Unstable, theme),   L"Reads differ in length" },
    };

    HRESULT  hr     = S_OK;
    RECT     bounds = GetBounds();
    float    x      = (float) bounds.left;
    float    lineH  = m_scaler.ToPxf (kLineDip);
    float    swatch = m_scaler.ToPxf (kSwatchDip);
    float    gap    = m_scaler.ToPxf (kColumnGapDip);



    for (const Entry & entry : entries)
    {
        float  width  = 0.0f;
        float  height = 0.0f;

        hr = text.MeasureString (entry.label, m_scaler.ToPxf (kFontDip), s_kpszFont, width, height);
        IGNORE_RETURN_VALUE (hr, S_OK);

        painter.FillRect (x, top + (lineH - swatch) * 0.5f, swatch, swatch, entry.color);
        DrawLine (text, entry.label, x + swatch * 1.6f, top, width + swatch, theme.ForegroundMuted());

        x += swatch * 1.6f + width + gap;
    }

    return top + lineH;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::PaintDisks
//
//  The latest disks done: number, file, file system, status and time.
//  Returns the bottom of kDiskRows rows, whether or not as many are done.
//
////////////////////////////////////////////////////////////////////////////////

float FullSendPanel::PaintDisks (IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const
{
    RECT    bounds = GetBounds();
    float   left   = (float) bounds.left;
    float   width  = (float) (bounds.right - bounds.left);
    float   lineH  = m_scaler.ToPxf (kLineDip);
    size_t  count  = m_view.snapshot.disks.size();
    float   number = width * 0.08f;
    float   file   = width * 0.40f;
    float   system = width * 0.16f;
    float   took   = width * 0.08f;
    float   status = width - number - file - system - took;



    DrawLine (text, count > (size_t) kDiskRows ? std::format (L"Disks done: the latest {} of {}", kDiskRows, count) : std::wstring (L"Disks done"),
              left, top, width, theme.HeadingForeground(), true);
    top += m_scaler.ToPxf (kHeadingDip);

    for (size_t i = 0; i < (size_t) kDiskRows; i++)
    {
        if (i < count)
        {
            const FullSendDoneDisk  & disk = m_view.snapshot.disks[i];
            std::wstring              name = disk.row.file.empty() ? std::wstring (L"(no image)") : TextEncoding::NarrowToWide (disk.row.file);

            DrawLine (text, std::format (L"{:04}", disk.row.sequence), left, top, number, theme.ForegroundMuted());
            DrawLine (text, name, left + number, top, file, theme.Foreground());
            DrawLine (text, TextEncoding::NarrowToWide (disk.fileSystem), left + number + file, top, system, theme.ForegroundMuted());
            DrawLine (text, DescribeStatus (disk.row), left + number + file + system, top, status, theme.ForegroundMuted());
            DrawLine (text, FormatSeconds (disk.seconds), left + width - took, top, took, theme.ForegroundMuted(), false, DxuiTextHAlign::Right);
        }

        top += lineH;
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::PaintActivity
//
//  The latest activity lines, the newest last, each after its time.
//
////////////////////////////////////////////////////////////////////////////////

float FullSendPanel::PaintActivity (IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const
{
    RECT    bounds = GetBounds();
    float   left   = (float) bounds.left;
    float   width  = (float) (bounds.right - bounds.left);
    float   lineH  = m_scaler.ToPxf (kLineDip);
    float   time   = width * 0.10f;
    size_t  count  = m_view.snapshot.lines.size();
    size_t  first  = count > (size_t) kActivityRows ? count - (size_t) kActivityRows : 0;



    DrawLine (text, L"Activity", left, top, width, theme.HeadingForeground(), true);
    top += m_scaler.ToPxf (kHeadingDip);

    for (size_t i = first; i < count; i++)
    {
        DrawLine (text, TextEncoding::NarrowToWide (m_view.snapshot.lines[i].time), left, top, time, theme.ForegroundMuted());
        DrawLine (text, TextEncoding::NarrowToWide (m_view.snapshot.lines[i].text), left + time, top, width - time, theme.Foreground());
        top += lineH;
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDialog::Configure (const IDxuiTheme * theme, SnapshotFn snapshot, ActionFn endSession, ActionFn openFolder)
{
    m_theme      = theme;
    m_snapshot   = std::move (snapshot);
    m_endSession = std::move (endSession);
    m_openFolder = std::move (openFolder);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog::IsRunning
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendDialog::IsRunning (const FullSendSnapshot & snapshot)
{
    return snapshot.activity != FullSendActivity::None && snapshot.activity != FullSendActivity::Ended;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDialog::OnCreate()
{
    DxuiButton  * folder = nullptr;



    m_panel     = CreateDialogContent<FullSendPanel>();
    m_endButton = AddDialogButton (L"End session", kEndId);
    folder      = AddDialogButton (L"Open folder", kFolderId);

    AddDialogButton (L"Close", IDCANCEL);

    m_endButton->SetOnClick ([this]()
    {
        if (m_endSession)
        {
            m_endSession();
        }

        Refresh();
    });

    folder->SetOnClick ([this]()
    {
        if (m_openFolder)
        {
            m_openFolder();
        }
    });

    SetDialogTickIntervalMs (kTickMs);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog::OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDialog::OnDialogTick()
{
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog::Refresh
//
//  The times move while a session runs, so each tick is drawn.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDialog::Refresh()
{
    if (m_snapshot)
    {
        m_snapshot (m_view);
    }

    m_panel->SetView (m_view);
    m_endButton->SetEnabled (IsRunning (m_view.snapshot));

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog::Open
//
//  Without activation for a window that opens when a session starts, so the
//  keyboard focus is where it was.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<FullSendDialog> FullSendDialog::Open (HWND owner, const IDxuiTheme * theme, bool activate, SnapshotFn snapshot, ActionFn endSession, ActionFn openFolder)
{
    HRESULT                          hr     = S_OK;
    std::unique_ptr<FullSendDialog>  dialog = std::make_unique<FullSendDialog>();
    DxuiWindow::CreateParams         params;



    dialog->Configure (theme, std::move (snapshot), std::move (endSession), std::move (openFolder));

    params.title                    = L"Full Send";
    params.hInstance                = (owner != nullptr) ? (HINSTANCE) GetWindowLongPtrW (owner, GWLP_HINSTANCE) : GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kWidthDip, kHeightDip };
    params.minSizeDip               = { kWidthDip, kHeightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;
    params.createNoActivate         = !activate;

    hr = dialog->Create (params);
    CHRA (hr);

    dialog->SetTheme (theme);
    dialog->ShowModelessDialog (IDCANCEL, activate);

Error:
    return SUCCEEDED (hr) ? std::move (dialog) : nullptr;
}
