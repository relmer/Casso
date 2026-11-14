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
//  FullSendPanel::DescribeSummary
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::DescribeSummary (const FullSendSnapshot & snapshot)
{
    int           done  = snapshot.totals.sent + snapshot.totals.duplicates;
    std::wstring  tally = std::format (L"{} {} done", done, done == 1 ? L"disk" : L"disks");
    std::wstring  text;



    switch (snapshot.activity)
    {
        case FullSendActivity::Starting:        text = L"Full Send: waiting for Space on the Apple";                           break;
        case FullSendActivity::WaitingForDisk:  text = std::format (L"Full Send: waiting for a disk, {}", tally);             break;
        case FullSendActivity::WaitingForApple: text = L"Full Send: waiting for the Apple";                                    break;
        case FullSendActivity::EditingPrefix:   text = L"Full Send: a file name prefix is being typed on the Apple";           break;
        case FullSendActivity::Checking:        text = std::format (L"Full Send: checking drive {}, {}", snapshot.drive, tally); break;
        default:                                                                                                                break;
    }

    if ((snapshot.activity == FullSendActivity::Sending || snapshot.activity == FullSendActivity::Capturing) && snapshot.drive >= 1 && snapshot.drive <= (int) snapshot.drives.size())
    {
        const FullSendDriveStatus  & drive = snapshot.drives[(size_t) snapshot.drive - 1];

        text = std::format (L"Full Send: drive {}", snapshot.drive);
        text += drive.title.empty() ? std::wstring() : L", " + TextEncoding::NarrowToWide (drive.title);
        text += snapshot.activity == FullSendActivity::Capturing ? L", raw capture" : L"";
        text += std::format (L", {}%", std::clamp (drive.percent, 0, 100));
        text += drive.secondsLeft >= 0.0 ? std::format (L", about {} left", FormatSeconds (drive.secondsLeft)) : std::wstring();
        text += L", " + tally;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::DescribeCell
//
//  In a raw capture every cell has its position's mark; otherwise a track's
//  cell has the track's.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::DescribeCell (const FullSendDriveStatus & drive, int position)
{
    std::wstring  track = (position % 4 == 0) ? std::format (L"Track {}", position / 4) : std::format (L"Track {:.2f}", position / 4.0);
    std::wstring  state = L"not read yet";



    if (!drive.positions.empty())
    {
        switch (drive.positions[(size_t) position])
        {
            case FullSendPositionMark::Read:     state = L"read";                                                        break;
            case FullSendPositionMark::Skipped:  state = L"not read, the tracks beside it are complete";                 break;
            case FullSendPositionMark::Weak:     state = L"weak bits, which read differently every time, as on the disk"; break;
            case FullSendPositionMark::Unstable: state = L"the reads differ in length";                                  break;
            default:                                                                                                      break;
        }
    }
    else
    {
        switch (drive.tracks[(size_t) position / 4])
        {
            case FullSendTrackMark::Read:       state = L"read";                                  break;
            case FullSendTrackMark::Verified:   state = L"read and verified";                     break;
            case FullSendTrackMark::Unreliable: state = L"a sector's reads disagree";             break;
            case FullSendTrackMark::Unreadable: state = L"a sector could not be read";            break;
            default:                                                                              break;
        }
    }

    return track + L": " + state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::AddCell
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::AddCell (float left, float top, float width, float height, int number, int position) const
{
    Cell  cell;



    cell.bounds   = { (LONG) left, (LONG) top, (LONG) (left + width + 1.0f), (LONG) (top + height + 1.0f) };
    cell.drive    = number - 1;
    cell.position = position;

    m_cells.push_back (cell);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::OnMouse
//
//  The map cell under the pointer has a tip; anywhere else, none.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendPanel::OnMouse (const DxuiMouseEvent & ev)
{
    int64_t  now = (int64_t) GetTickCount64();
    POINT    pt  = { ev.positionDip.x, ev.positionDip.y };



    if (m_tooltip == nullptr)
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Enter)
    {
        for (const Cell & cell : m_cells)
        {
            if (PtInRect (&cell.bounds, pt))
            {
                m_tooltip->RequestShow (cell.bounds, DescribeCell (m_view.snapshot.drives[(size_t) cell.drive], cell.position), now);
                return false;
            }
        }
    }

    m_tooltip->RequestHide (now);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::DescribeActivity
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::DescribeActivity (const FullSendSnapshot & snapshot, const std::wstring & machine)
{
    std::wstring  apple = machine.empty() ? std::wstring (L"Apple") : machine.substr (0, machine.find (L" ("));



    switch (snapshot.activity)
    {
        case FullSendActivity::Starting:        return std::format (L"The {} is waiting for disks to send.", apple);
        case FullSendActivity::Checking:        return std::format (L"Checking the disk in drive {}.", snapshot.drive);
        case FullSendActivity::Sending:         return std::format (L"Sending the disk in drive {}.", snapshot.drive);
        case FullSendActivity::Capturing:       return std::format (L"The disk in drive {} is likely copy-protected; using raw capture mode.", snapshot.drive);
        case FullSendActivity::WaitingForDisk:  return std::format (L"The {} is waiting for more disks to send.", apple);
        case FullSendActivity::WaitingForApple: return std::format (L"Waiting for the Apple to reply, {}.", FormatSeconds (snapshot.waited));
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
        case FullSendDiskState::Sending:   state = drive.verified ? L"Sending, with verification" : L"Sending"; break;
        case FullSendDiskState::Capturing: state = L"Capturing raw";                         break;
        case FullSendDiskState::Sent:      state = L"Sent";                                  break;
        case FullSendDiskState::Duplicate: state = L"Duplicate";                             break;
        case FullSendDiskState::Empty:     state = L"Unformatted, or no disk";               break;
        case FullSendDiskState::Blank:     state = L"Blank";                                 break;
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
    std::wstring  text = std::format (L"{} sent, {} {}, {} blank, {} raw {}, {} with bad sectors", totals.sent,
                                      totals.duplicates, totals.duplicates == 1 ? L"duplicate" : L"duplicates", totals.blanks,
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

    m_cells.clear();

    DrawLine (text, DescribeActivity (m_view.snapshot, m_view.machine), left, top, width, theme.Foreground(), true);

    if (!m_view.connection.empty())
    {
        DrawLine (text, m_view.connection, left, top, width, theme.ForegroundMuted(), false, DxuiTextHAlign::Right);
    }

    top += m_scaler.ToPxf (kHeadingDip) + section;

    //  A drive the Apple lacks is omitted, and the other one is at the left.
    first  = top;
    second = top;

    if (m_view.snapshot.present[0])
    {
        first = PaintDrive (painter, text, theme, 1, m_view.snapshot.drives[0], left, top, column);
    }

    if (m_view.snapshot.present[1])
    {
        second = PaintDrive (painter, text, theme, 2, m_view.snapshot.drives[1], m_view.snapshot.present[0] ? left + column + gap : left, top, column);
    }

    top = std::max (first, second) + section * 0.5f;

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
        AddCell (left + track * pitch, top, pitch - space, cellH, number, track * 4);
    }

    top += cellH + space;

    //  A position between two tracks has a cell under its track only when it
    //  was read. Most disks have nothing there, and the row is empty.
    for (size_t position = 0; position < drive.positions.size(); position++)
    {
        FullSendPositionMark  mark = drive.positions[position];

        if (position % 4 == 0 || mark == FullSendPositionMark::Unread || mark == FullSendPositionMark::Skipped)
        {
            continue;
        }

        painter.FillRect (left + position * quarter, top, std::max (1.0f, quarter - space), rawH, GetColor (mark, theme));
        AddCell (left + position * quarter, top, std::max (1.0f, quarter - space), rawH, number, (int) position);
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
//  The latest activity lines, the newest last, each after its time. A line
//  wider than the list wraps onto a second row, and the list has as many of
//  the latest lines as fit its rows.
//
////////////////////////////////////////////////////////////////////////////////

float FullSendPanel::PaintActivity (IDxuiTextRenderer & text, const IDxuiTheme & theme, float top) const
{
    RECT                       bounds = GetBounds();
    float                      left   = (float) bounds.left;
    float                      width  = (float) (bounds.right - bounds.left);
    float                      lineH  = m_scaler.ToPxf (kLineDip);
    float                      time   = width * 0.10f;
    size_t                     count  = m_view.snapshot.lines.size();
    size_t                     first  = count;
    int                        used   = 0;
    std::vector<std::wstring>  lines  (count);
    std::vector<int>           rows   (count, 1);



    DrawLine (text, L"Activity", left, top, width, theme.HeadingForeground(), true);
    top += m_scaler.ToPxf (kHeadingDip);

    while (first > 0)
    {
        HRESULT  hr       = S_OK;
        float    measured = 0.0f;
        float    height   = 0.0f;
        size_t   i        = first - 1;

        lines[i] = TextEncoding::NarrowToWide (m_view.snapshot.lines[i].text);

        hr = text.MeasureString (lines[i].c_str(), m_scaler.ToPxf (kFontDip), s_kpszFont, measured, height);
        IGNORE_RETURN_VALUE (hr, S_OK);

        rows[i] = (measured > width - time) ? 2 : 1;

        if (used + rows[i] > kActivityRows)
        {
            break;
        }

        used += rows[i];
        first = i;
    }

    for (size_t i = first; i < count; i++)
    {
        HRESULT  hr = S_OK;

        DrawLine (text, TextEncoding::NarrowToWide (m_view.snapshot.lines[i].time), left, top, time, theme.ForegroundMuted());

        hr = text.DrawString (lines[i].c_str(), left + time, top, width - time, lineH * rows[i], theme.Foreground(), m_scaler.ToPxf (kFontDip), s_kpszFont,
                              DxuiTextHAlign::Left, rows[i] == 1 ? DxuiTextVAlign::Center : DxuiTextVAlign::Top, DxuiFontWeight::Normal, rows[i] > 1);
        IGNORE_RETURN_VALUE (hr, S_OK);

        top += lineH * rows[i];
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

    m_tooltip.SetPopupHost (GetPopupHost());
    m_tooltip.SetFollowPointer (true);

    if (m_theme != nullptr)
    {
        m_tooltip.SetTheme (*m_theme);
    }

    m_panel->SetTooltip (&m_tooltip);
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

    SetDialogTickIntervalMs (kTipTickMs);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog::OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDialog::OnDialogTick()
{
    m_tooltip.Tick ((int64_t) GetTickCount64());

    if (m_ticks++ % (kTickMs / kTipTickMs) == 0)
    {
        Refresh();
    }
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
