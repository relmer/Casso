#include "Pch.h"

#include "CassoExplorer/FullSendDialog.h"
#include "CassoLink/FullSend/FullSendManifest.h"
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
//  FullSendPanel::TryGetQuarterColor
//
//  A quarter track's stripe in its track's cell, by its quarter, 1 to 3: a
//  quarter track read is a lighter blue toward the next track, and a weak or
//  unstable one has its mark's color. One that wasn't read has no stripe.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendPanel::TryGetQuarterColor (FullSendPositionMark mark, int quarter, uint32_t & outColor)
{
    constexpr std::array<uint32_t, 3>  kShades = { 0xFF5B95D1, 0xFF7FAEE0, 0xFFA6C8EE };



    switch (mark)
    {
        case FullSendPositionMark::Read:     outColor = kShades[(size_t) std::clamp (quarter, 1, 3) - 1]; return true;
        case FullSendPositionMark::Weak:     outColor = 0xFFD29922;                                  return true;
        case FullSendPositionMark::Unstable: outColor = 0xFFE07B39;                                  return true;
        default:                                                                                     return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::GetCellColor
//
//  In a raw capture a track's cell has its whole-track read's color, as the
//  sector read before it found the tracks unreadable; otherwise the track's.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FullSendPanel::GetCellColor (const FullSendDriveStatus & drive, int track, const IDxuiTheme & theme)
{
    return drive.positions.empty() ? GetColor (drive.tracks[(size_t) track], theme) : GetColor (drive.positions[(size_t) track * 4], theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::GetTrackSwatches
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTooltipSwatch> FullSendPanel::GetTrackSwatches (const FullSendDriveStatus & drive, int track, const IDxuiTheme & theme)
{
    std::vector<DxuiTooltipSwatch>  swatches = { { 0, GetCellColor (drive, track, theme) } };
    int                             last     = drive.positions.empty() ? track * 4 : std::min (track * 4 + 3, (int) drive.positions.size() - 1);
    uint32_t                        color    = 0;



    for (int position = track * 4 + 1; position <= last; position++)
    {
        if (TryGetQuarterColor (drive.positions[(size_t) position], position - track * 4, color))
        {
            swatches.push_back ({ (int) swatches.size(), color });
        }
    }

    return swatches;
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

std::wstring FullSendPanel::DescribeSummary (const FullSendSnapshot & snapshot, const std::wstring & connection)
{
    int           done  = snapshot.totals.sent + snapshot.totals.duplicates;
    std::wstring  tally = std::format (L"{} {} done", done, done == 1 ? L"disk" : L"disks");
    std::wstring  prefix = connection.empty() ? std::wstring (L"Full Send: ") : std::format (L"Full Send on {}: ", connection);
    std::wstring  text;



    switch (snapshot.activity)
    {
        case FullSendActivity::Starting:        text = prefix + L"waiting for Space on the Apple";                           break;
        case FullSendActivity::WaitingForDisk:  text = prefix + std::format (L"waiting for a disk, {}", tally);             break;
        case FullSendActivity::WaitingForApple: text = prefix + L"waiting for the Apple";                                    break;
        case FullSendActivity::EditingPrefix:   text = prefix + L"a file name prefix is being typed on the Apple";           break;
        case FullSendActivity::Checking:        text = prefix + std::format (L"checking drive {}, {}", snapshot.drive, tally); break;
        case FullSendActivity::Validating:      text = prefix + std::format (L"validating the image, {}", tally);             break;
        default:                                                                                                                break;
    }

    if ((snapshot.activity == FullSendActivity::Sending || snapshot.activity == FullSendActivity::Capturing) && snapshot.drive >= 1 && snapshot.drive <= (int) snapshot.drives.size())
    {
        const FullSendDriveStatus  & drive = snapshot.drives[(size_t) snapshot.drive - 1];

        text = prefix + std::format (L"drive {}", snapshot.drive);
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
//  FullSendPanel::DescribeTrack
//
//  The track's cell, and in a raw capture each of its quarter tracks that
//  was read. Most disks have nothing between their tracks.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::DescribeTrack (const FullSendDriveStatus & drive, int track)
{
    int           last = drive.positions.empty() ? track * 4 : std::min (track * 4 + 3, (int) drive.positions.size() - 1);
    std::wstring  text = DescribeCell (drive, track * 4);



    for (int position = track * 4 + 1; position <= last; position++)
    {
        FullSendPositionMark  mark = drive.positions[(size_t) position];

        if (mark == FullSendPositionMark::Read || mark == FullSendPositionMark::Weak || mark == FullSendPositionMark::Unstable)
        {
            text += L"\n" + DescribeCell (drive, position);
        }
    }

    return text;
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
//  FullSendPanel::TryGetTrackAt
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendPanel::TryGetTrackAt (POINT pointPx, int & outDrive, int & outTrack, RECT * outBounds) const
{
    auto  found = std::find_if (m_cells.begin(), m_cells.end(), [&] (const Cell & cell) { return PtInRect (&cell.bounds, pointPx) != FALSE; });



    outDrive = found == m_cells.end() ? 0 : found->drive + 1;
    outTrack = found == m_cells.end() ? 0 : found->position / 4;

    if (outBounds != nullptr && found != m_cells.end())
    {
        *outBounds = found->bounds;
    }

    return found != m_cells.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::OnMouse
//
//  A map is magnified while the pointer is over it, or over where its
//  magnified cells are, and the track under the pointer has a tip at once.
//  The track is found on the map at rest, so it is the same one while the
//  map grows under the pointer. A move that changes the magnification is
//  taken, so the window is drawn again.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendPanel::OnMouse (const DxuiMouseEvent & ev)
{
    int64_t   now       = (int64_t) GetTickCount64();
    POINT     pt        = { ev.positionDip.x, ev.positionDip.y };
    bool      isMove    = ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Enter;
    bool      wasActive = m_magnifier.IsActive (now);
    int       drive     = isMove ? FindMapAt (pt) : -1;
    int       track     = -1;
    MapItems  items     = {};



    if (HandleClick (ev))
    {
        return true;
    }

    if (drive >= 0)
    {
        MakeMapItems (m_maps[(size_t) drive], items);

        track      = DxuiMagnifiedRow::FindItemAt (items, (float) pt.x);
        m_hotDrive = drive;
        m_pointerX = (float) pt.x;
    }

    m_magnifier.SetNear (drive >= 0, now);
    UpdateTip (drive, track, pt, now);

    return wasActive || drive >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::UpdateTip
//
//  The track's tip is under the pointer, clear of the magnified cells above
//  it, and follows the pointer. Each line has its color mark.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::UpdateTip (int drive, int track, POINT pointPx, int64_t nowMs)
{
    if (m_tooltip != nullptr && drive >= 0 && track >= 0 && m_theme != nullptr)
    {
        const FullSendDriveStatus  & status = m_view.snapshot.drives[(size_t) drive];

        m_tooltip->SetSwatches (GetTrackSwatches (status, track, *m_theme));
        m_tooltip->RequestShowNowBelowPointer (pointPx, DescribeTrack (status, track), nowMs);
    }
    else if (m_tooltip != nullptr)
    {
        m_tooltip->RequestHide (nowMs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::FindMapAt
//
//  The drive, from 0, whose map is under a point: over its cells at rest,
//  or, once it is magnified, above them where its magnified cells are, so
//  the line above a map can be pointed at without magnifying it. -1 for
//  none.
//
////////////////////////////////////////////////////////////////////////////////

int FullSendPanel::FindMapAt (POINT pointPx) const
{
    int  found = -1;



    for (size_t i = 0; i < m_maps.size(); i++)
    {
        const Map  & map   = m_maps[i];
        float        x     = (float) pointPx.x;
        float        y     = (float) pointPx.y;
        float        right = map.left + map.pitch * (FullSendDriveStatus::kTracks - 1) + map.cellW;
        float        top   = (m_magnifier.IsNear() && (int) i == m_hotDrive) ? map.top - (kMagnifyMax - 1.0f) * map.cellH : map.top;

        if (map.isPainted && x >= map.left && x < right && y >= top && y < map.top + map.cellH)
        {
            found = (int) i;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::MakeMapItems
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::MakeMapItems (const Map & map, MapItems & outItems)
{
    for (size_t track = 0; track < outItems.size(); track++)
    {
        outItems[track] = { map.left + (float) track * map.pitch, map.cellW, kMagnifyMax };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::LayMap
//
//  A drive's map magnified about a pointer at a strength, spreading past
//  the ends of the map at rest, with the point under the pointer under it.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::LayMap (int drive, float pointer, float strength, MapSpans & outSpans) const
{
    MapItems                   items = {};
    DxuiMagnifiedRow::Options  options;



    MakeMapItems (m_maps[(size_t) drive], items);

    options.reach  = m_maps[(size_t) drive].pitch * kMagnifyReachTracks;
    options.anchor = DxuiMagnifiedRow::Anchor::Pointer;

    DxuiMagnifiedRow::Lay (items, pointer, strength, options, outSpans);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::HandleClick
//
//  A second press on a disk's row within the double-click time opens it; a
//  right press on a row or on the activity list opens its menu.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendPanel::HandleClick (const DxuiMouseEvent & ev)
{
    int64_t      now     = (int64_t) GetTickCount64();
    POINT        pt      = { ev.positionDip.x, ev.positionDip.y };
    std::string  file;
    bool         onRow   = TryGetDiskAt (pt, file);
    bool         handled = false;



    if (ev.kind != DxuiMouseEventKind::Down)
    {
        return false;
    }

    if (ev.button == DxuiMouseButton::Left && onRow)
    {
        handled = file == m_lastClicked && now - m_lastClickMs <= (int64_t) GetDoubleClickTime();

        m_lastClicked = handled ? std::string() : file;
        m_lastClickMs = now;

        if (handled && m_onOpen)
        {
            m_onOpen (file);
        }
    }
    else if (ev.button == DxuiMouseButton::Right && (onRow || IsOverActivity (pt)) && m_onMenu)
    {
        m_onMenu (onRow ? file : std::string(), pt);
        handled = true;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::TryGetDiskAt
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendPanel::TryGetDiskAt (POINT pointPx, std::string & outFile) const
{
    auto  found = std::find_if (m_rows.begin(), m_rows.end(), [pointPx] (const Row & row) { return PtInRect (&row.bounds, pointPx) != FALSE; });



    outFile = (found != m_rows.end()) ? found->file : std::string();

    return found != m_rows.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::IsOverActivity
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendPanel::IsOverActivity (POINT pointPx) const
{
    return PtInRect (&m_activity, pointPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::FormatActivity
//
//  Every line of the session, not only those the list has room for.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendPanel::FormatActivity (const FullSendSnapshot & snapshot)
{
    std::wstring  text;



    for (const auto & line : snapshot.lines)
    {
        text += TextEncoding::NarrowToWide (line.time) + L"  " + TextEncoding::NarrowToWide (line.text) + L"\r\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::MakeManifestView
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendPanel::MakeManifestView (std::span<const Byte> manifest, FullSendSnapshot & outSnapshot)
{
    HRESULT           hr     = S_OK;
    FullSendManifest  parsed;



    outSnapshot = FullSendSnapshot();

    hr = parsed.Parse (manifest);
    CHR (hr);

    for (auto row = parsed.GetRows().rbegin(); row != parsed.GetRows().rend(); row++)
    {
        FullSendDoneDisk  disk;

        disk.row = *row;
        outSnapshot.disks.push_back (std::move (disk));
    }

Error:
    return SUCCEEDED (hr);
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
        case FullSendActivity::Validating:      return std::format (L"Validating the image of the disk in drive {}: booting it to check that it runs.", snapshot.drive);
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



    text += std::format (L"    {} active, {} idle", FormatSeconds (totals.elapsed), FormatSeconds (totals.idle));

    if (totals.sent != 0)
    {
        text += std::format (L", average {} per disk, {:.1f} MB", FormatSeconds (totals.average), (double) totals.bytes / (1024.0 * 1024.0));
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
    const wchar_t  * boot = row.boot == FullSendManifest::kBootBooted      ? L", boots"
                          : row.boot == FullSendManifest::kBootDidNotBoot  ? L", did not boot"
                          : row.boot == FullSendManifest::kBootNotBootable ? L", not bootable"
                          : row.boot == FullSendManifest::kBootUnclear     ? L", boot unclear"
                                                                           : L"";



    if (row.status == FullSendManifest::kStatusDuplicate)
    {
        return L"Duplicate of " + TextEncoding::NarrowToWide (row.duplicateOf);
    }

    if (row.status == FullSendManifest::kStatusRawCapture)
    {
        return std::wstring (L"Raw capture") + boot;
    }

    if (row.status == FullSendManifest::kStatusProtected)
    {
        return std::wstring (L"Likely copy-protected") + boot;
    }

    if (row.unreadable != 0)
    {
        return std::format (L"{} unreadable {}", row.unreadable, row.unreadable == 1 ? L"sector" : L"sectors") + boot;
    }

    if (row.unreliable != 0)
    {
        return std::format (L"{} unreliable {}", row.unreliable, row.unreliable == 1 ? L"sector" : L"sectors") + boot;
    }

    return std::wstring (row.verification == FullSendManifest::kVerifyFull ? L"Sent, verified" : L"Sent") + boot;
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
    RECT      bounds  = GetBounds();
    float     left    = (float) bounds.left;
    float     width   = (float) (bounds.right - bounds.left);
    float     gap     = m_scaler.ToPxf (kColumnGapDip);
    float     section = m_scaler.ToPxf (kSectionGapDip);
    float     column  = (width - gap) * 0.5f;
    float     top     = (float) bounds.top;
    float     first   = 0.0f;
    float     second  = 0.0f;
    float     gain    = m_magnifier.GetStrength ((int64_t) GetTickCount64());
    MapSpans  spans   = {};



    if (!IsVisible() || width < 1.0f)
    {
        return;
    }

    m_cells.clear();
    m_rows.clear();
    m_activity = {};
    m_theme    = &theme;

    UpdateCover (gain);

    m_maps = {};

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

    //  A magnified map is drawn over everything near it, on an opaque cover
    //  from its drive's heading down: the text drawn so far is flushed
    //  first, so the cover hides the headings and titles under it.
    if (m_cover.right > m_cover.left && m_hotDrive >= 0 && m_maps[(size_t) m_hotDrive].isPainted)
    {
        LayMap (m_hotDrive, m_pointerX, gain, spans);

        painter.FlushLayer();
        painter.FillRect ((float) m_cover.left, (float) m_cover.top, (float) (m_cover.right - m_cover.left), (float) (m_cover.bottom - m_cover.top), theme.Background());
        PaintCells (painter, theme, m_view.snapshot.drives[(size_t) m_hotDrive], m_maps[(size_t) m_hotDrive], spans);
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
//  A drive's heading and state, its disk's title, the map of its tracks with
//  the quarter tracks of a raw capture, its progress bar, and its times.
//  Returns the bottom.
//
////////////////////////////////////////////////////////////////////////////////

float FullSendPanel::PaintDrive (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int number, const FullSendDriveStatus & drive, float left, float top, float width) const
{
    float         lineH   = m_scaler.ToPxf (kLineDip);
    float         cellH   = m_scaler.ToPxf (kMapCellDip);
    float         barH    = m_scaler.ToPxf (kBarDip);
    float         space   = std::max (1.0f, std::floor (m_scaler.ToPxf (1.0f)));
    float         pitch   = std::floor (width / FullSendDriveStatus::kTracks);
    float         heading = m_scaler.ToPxf (kHeadingDip);
    MapSpans      spans   = {};
    bool          busy    = drive.state == FullSendDiskState::Sending || drive.state == FullSendDiskState::Capturing;
    std::wstring  title   = drive.title.empty() ? std::wstring (L"No disk") : TextEncoding::NarrowToWide (drive.title);
    float         header  = top;



    DrawLine (text, std::format (L"Drive {}", number), left, top, width, theme.HeadingForeground(), true);
    DrawLine (text, DescribeDrive (drive), left, top, width, theme.ForegroundMuted(), false, DxuiTextHAlign::Right);
    top += heading;

    DrawLine (text, title, left, top, width, theme.Foreground());
    top += lineH + space * 2.0f;

    //  The map at rest; Paint draws a magnified map over it.
    m_maps[(size_t) number - 1] = { true, left, top, pitch, pitch - space, cellH, header, width };
    LayMap (number - 1, 0.0f, 0.0f, spans);
    PaintCells (painter, theme, drive, m_maps[(size_t) number - 1], spans);

    for (int track = 0; track < FullSendDriveStatus::kTracks; track++)
    {
        AddCell (left + track * pitch, top, pitch - space, cellH, number, track * 4);
    }

    top += cellH + space * 4.0f;

    painter.FillRect (left, top, pitch * FullSendDriveStatus::kTracks - space, barH, theme.BackgroundElevated());

    if (busy || drive.state == FullSendDiskState::Sent)
    {
        PaintProgress (painter, left, top, (pitch * FullSendDriveStatus::kTracks - space) * (float) std::clamp (drive.percent, 0, 100) / 100.0f, barH, busy);
    }

    top += barH + space * 2.0f;

    DrawLine (text, DescribeTimes (drive), left, top, width, theme.ForegroundMuted());
    top += lineH;

    DrawLine (text, DescribeCapture (drive), left, top, width, theme.ForegroundMuted());

    return top + lineH;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::PaintCells
//
//  A map's cells as laid out. In a raw capture a track's cell has a stripe
//  for each of its quarter tracks that was read; most disks have nothing
//  between their tracks, and the cell is one color. A magnified cell grows
//  up from the map's bottom edge.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::PaintCells (IDxuiPainter & painter, const IDxuiTheme & theme, const FullSendDriveStatus & drive, const Map & map, const MapSpans & spans) const
{
    uint32_t  shade = 0;



    for (int track = 0; track < FullSendDriveStatus::kTracks; track++)
    {
        const DxuiMagnifiedSpan  & span    = spans[(size_t) track];
        float                      height  = map.cellH * span.scale;
        float                      cellTop = map.top + map.cellH - height;
        float                      stripe  = span.width / 4.0f;

        painter.FillRect (span.left, cellTop, span.width, height, GetCellColor (drive, track, theme));

        for (int quarter = 1; quarter < 4 && (size_t) (track * 4 + quarter) < drive.positions.size(); quarter++)
        {
            if (TryGetQuarterColor (drive.positions[(size_t) (track * 4 + quarter)], quarter, shade))
            {
                painter.FillRect (span.left + quarter * stripe, cellTop, stripe, height, shade);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::UpdateCover
//
//  What a magnified map covers, from the maps as last painted: from its
//  drive's heading down to the map's bottom edge, across its drive's column
//  and as far as its cells spread past it. Empty with no map magnified.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::UpdateCover (float strength) const
{
    MapSpans  spans = {};



    m_cover = {};

    if (strength > 0.0f && m_hotDrive >= 0 && m_maps[(size_t) m_hotDrive].isPainted)
    {
        const Map  & map = m_maps[(size_t) m_hotDrive];

        LayMap (m_hotDrive, m_pointerX, strength, spans);

        m_cover = { (LONG) std::floor (std::min (map.left, spans.front().left)),
                    (LONG) std::floor (map.header),
                    (LONG) std::ceil  (std::max (map.left + map.width, spans.back().left + spans.back().width)),
                    (LONG) std::ceil  (map.top + map.cellH) };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendPanel::PaintProgress
//
//  A thin green line. While it grows, its last kBarGlowDip brighten toward
//  the end, drawn a pixel at a time, as the painter's gradients are
//  vertical.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendPanel::PaintProgress (IDxuiPainter & painter, float left, float top, float width, float height, bool isGrowing) const
{
    float  glow = isGrowing ? std::min (width, m_scaler.ToPxf (kBarGlowDip)) : 0.0f;
    float  step = std::max (1.0f, std::floor (m_scaler.ToPxf (1.0f)));



    painter.FillRect (left, top, width - glow, height, kBarArgb);

    for (float x = 0.0f; x < glow; x += step)
    {
        painter.FillRect (left + width - glow + x, top, std::min (step, glow - x), height, DxuiColor::Mix (kBarArgb, kBarTipArgb, (x + step) / glow));
    }
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
            DrawLine (text, (disk.seconds > 0.0) ? FormatSeconds (disk.seconds) : std::wstring(), left + width - took, top, took, theme.ForegroundMuted(), false, DxuiTextHAlign::Right);

            if (!disk.row.file.empty())
            {
                m_rows.push_back ({ RECT { (LONG) left, (LONG) top, (LONG) (left + width), (LONG) (top + lineH) }, disk.row.file });
            }
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
    float                      textH  = lineH;



    DrawLine (text, L"Activity", left, top, width, theme.HeadingForeground(), true);
    top += m_scaler.ToPxf (kHeadingDip);

    m_activity = RECT { (LONG) left, (LONG) top, (LONG) (left + width), (LONG) (top + lineH * kActivityRows) };

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
        textH   = (height > 0.0f) ? std::min (height, lineH) : textH;

        if (used + rows[i] > kActivityRows)
        {
            break;
        }

        used += rows[i];
        first = i;
    }

    //  A wrapped line's text begins where a single line's does, centered in
    //  the first row, and its block is one row and one more line of text
    //  tall, so the space below it matches the space below a single line.
    for (size_t i = first; i < count; i++)
    {
        HRESULT  hr     = S_OK;
        float    inset  = (lineH - textH) * 0.5f;
        float    blockH = rows[i] == 1 ? lineH : lineH + textH;

        DrawLine (text, TextEncoding::NarrowToWide (m_view.snapshot.lines[i].time), left, top, time, theme.ForegroundMuted());

        hr = rows[i] == 1 ? text.DrawString (lines[i].c_str(), left + time, top, width - time, lineH, theme.Foreground(), m_scaler.ToPxf (kFontDip), s_kpszFont,
                                             DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false)
                          : text.DrawString (lines[i].c_str(), left + time, top + inset, width - time, blockH - inset, theme.Foreground(), m_scaler.ToPxf (kFontDip), s_kpszFont,
                                             DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);
        IGNORE_RETURN_VALUE (hr, S_OK);

        top += blockH;
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
//  FullSendDialog::IsReadingDisk
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendDialog::IsReadingDisk (const FullSendSnapshot & snapshot)
{
    return std::any_of (snapshot.drives.begin(), snapshot.drives.end(), [] (const FullSendDriveStatus & drive)
    {
        return drive.state == FullSendDiskState::Checking || drive.state == FullSendDiskState::Sending || drive.state == FullSendDiskState::Capturing;
    });
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
    m_panel->SetOnOpenDisk ([this] (const std::string & file)
    {
        if (m_diskAction)
        {
            m_diskAction (file, DiskAction::Select);
        }
    });
    m_panel->SetOnMenu ([this] (const std::string & file, POINT pointPx)
    {
        DxuiContextMenu::Show (*GetPopupHost(), pointPx.x, pointPx.y, MakeMenu (file));
    });
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
    int64_t  now    = (int64_t) GetTickCount64();
    UINT     tickMs = m_panel->IsMagnifying (now) ? kFrameMs : kTipTickMs;



    m_tooltip.Tick (now);
    NoteMoved();

    if (tickMs != m_tickMs)
    {
        m_tickMs = tickMs;
        SetDialogTickIntervalMs (tickMs);
    }

    if (now - m_refreshedMs >= (int64_t) kTickMs)
    {
        m_refreshedMs = now;
        Refresh();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog::AddCommand
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDialog::AddCommand (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, std::function<void()> dispatch)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



    command->label    = label;
    command->dispatch = std::move (dispatch);

    items.push_back (DxuiPopupMenuItem::ForCommand (command));
    m_menuCommands.push_back (std::move (command));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog::MakeMenu
//
//  A disk's row has Open, Inspect disk image and, for a WOZ image, Disk
//  properties, as the image's menu in the folder has them. The activity
//  list has Copy, which copies every line of the session.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> FullSendDialog::MakeMenu (const std::string & file)
{
    std::vector<DxuiPopupMenuItem>  items;
    std::filesystem::path           name = file;



    m_menuCommands.clear();

    if (file.empty())
    {
        AddCommand (items, L"&Copy", [this]() { DxuiClipboard::SetText (GetHwnd(), FullSendPanel::FormatActivity (m_view.snapshot)); });
        return items;
    }

    AddCommand (items, L"&Open",               [this, file]() { if (m_diskAction) { m_diskAction (file, DiskAction::Open); } });
    AddCommand (items, L"&Inspect disk image", [this, file]() { if (m_diskAction) { m_diskAction (file, DiskAction::Inspect); } });

    if (_wcsicmp (name.extension().c_str(), L".woz") == 0)
    {
        AddCommand (items, L"&Disk properties", [this, file]() { if (m_diskAction) { m_diskAction (file, DiskAction::Properties); } });
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialog::NoteMoved
//
//  The window's top left, passed on each time it changes, so it opens there
//  the next time.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDialog::NoteMoved()
{
    RECT  frame   = {};
    BOOL  hasRect = (GetHwnd() != nullptr) ? GetWindowRect (GetHwnd(), &frame) : FALSE;
    POINT topLeft = { frame.left, frame.top };



    if (!hasRect || !m_onMoved || (topLeft.x == m_lastTopLeft.x && topLeft.y == m_lastTopLeft.y))
    {
        return;
    }

    if (m_lastTopLeft.x != LONG_MIN)
    {
        m_onMoved (topLeft);
    }

    m_lastTopLeft = topLeft;
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

std::unique_ptr<FullSendDialog> FullSendDialog::Open (HWND owner, const IDxuiTheme * theme, bool activate, SnapshotFn snapshot, ActionFn endSession, ActionFn openFolder,
                                                      const POINT * position, const std::wstring & connection)
{
    HRESULT                          hr       = S_OK;
    std::unique_ptr<FullSendDialog>  dialog   = std::make_unique<FullSendDialog>();
    DxuiWindow::CreateParams         params;
    bool                             isPlaced = position != nullptr && MonitorFromPoint (*position, MONITOR_DEFAULTTONULL) != nullptr;



    dialog->Configure (theme, std::move (snapshot), std::move (endSession), std::move (openFolder));

    params.title                    = connection.empty() ? std::wstring (L"Full Send") : L"Full Send - " + connection;
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

    if (isPlaced)
    {
        SetWindowPos (dialog->GetHwnd(), nullptr, position->x, position->y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    dialog->ShowModelessDialog (IDCANCEL, activate);

Error:
    return SUCCEEDED (hr) ? std::move (dialog) : nullptr;
}
