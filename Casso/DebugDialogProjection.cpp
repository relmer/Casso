#include "Pch.h"

#include "DebugDialogProjection.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope helpers
//
////////////////////////////////////////////////////////////////////////////////

static constexpr wchar_t  s_kThousandsSeparator = L',';
static constexpr size_t   s_kCycleBufferChars   = 24;
static constexpr size_t   s_kWallBufferChars    = 16;
static constexpr size_t   s_kUptimeBufferChars  = 12;





////////////////////////////////////////////////////////////////////////////////
//
//  SoundKindLabel
//
//  Diagnostic spelling of a SoundKind for the Detail column. Deliberately
//  the enumerator name rather than prose -- this text is read alongside
//  the audio source, not by end users.
//
////////////////////////////////////////////////////////////////////////////////

static const wchar_t * SoundKindLabel (SoundKind k)
{
    const wchar_t *  label = L"Unknown";



    switch (k)
    {
        case SoundKind::MotorLoop:  label = L"MotorLoop";  break;
        case SoundKind::HeadStep:   label = L"HeadStep";   break;
        case SoundKind::HeadStop:   label = L"HeadStop";   break;
        case SoundKind::DoorOpen:   label = L"DoorOpen";   break;
        case SoundKind::DoorClose:  label = L"DoorClose";  break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SilentReasonLabel
//
//  Why a drive sound that should have played did not. Pairs with
//  SoundKindLabel on the AudioSilent row; same enumerator-name convention.
//
////////////////////////////////////////////////////////////////////////////////

static const wchar_t * SilentReasonLabel (SilentReason r)
{
    const wchar_t *  label = L"Unknown";



    switch (r)
    {
        case SilentReason::DriveAudioDisabled:   label = L"DriveAudioDisabled";   break;
        case SilentReason::BufferMissing:        label = L"BufferMissing";        break;
        case SilentReason::NoSourceRegistered:   label = L"NoSourceRegistered";   break;
        case SilentReason::ColdBootSuppression:  label = L"ColdBootSuppression";  break;
        case SilentReason::NoDiskPresent:        label = L"NoDiskPresent";        break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatCycleWithSeparators
//
//  Decimal-format a uint64_t with comma thousands separators into a
//  fixed-size wchar buffer. Null-terminates. No allocation.
//
////////////////////////////////////////////////////////////////////////////////

static void FormatCycleWithSeparators (uint64_t value, wchar_t * out, size_t cap)
{
    wchar_t   digits[24] = {};
    int       n          = 0;
    int       outIdx     = 0;
    int       i          = 0;

    if (out == nullptr || cap == 0)
    {
        return;
    }

    if (value == 0)
    {
        digits[n++] = L'0';
    }
    else
    {
        while (value > 0 && n < (int) (sizeof (digits) / sizeof (digits[0])))
        {
            digits[n++] = static_cast<wchar_t> (L'0' + (value % 10));
            value      /= 10;
        }
    }

    for (i = n - 1; i >= 0; i--)
    {
        // Insert a separator BEFORE the next digit when the remaining
        // digit count (i + 1) is a positive multiple of 3 and the
        // separator would not be the first emitted character. This
        // groups from the right, producing "1,234,567" not "123,456,7".
        if (outIdx > 0 && (((i + 1) % 3) == 0))
        {
            if (outIdx + 1 >= (int) cap)
            {
                break;
            }

            out[outIdx++] = s_kThousandsSeparator;
        }

        if (outIdx + 1 >= (int) cap)
        {
            break;
        }

        out[outIdx++] = digits[i];
    }

    out[outIdx] = L'\0';
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatWallNow
//
//  Local wall clock as HH:MM:SS.mmm, captured at format time so the
//  Wall column reflects when the projection drained, not when the
//  producer fired. FR-005.
//
////////////////////////////////////////////////////////////////////////////////

static void FormatWallNow (wchar_t * out, size_t cap)
{
    using namespace std::chrono;

    auto       now       = system_clock::now();
    auto       wall      = system_clock::to_time_t (now);
    auto       ms        = duration_cast<milliseconds> (now.time_since_epoch()) % 1000;
    std::tm    local     = {};

    // A buffer too small to hold HH:MM:SS.mmm and a failed localtime_s are
    // the same outcome to the caller -- an empty cell -- so they share an
    // arm. Short-circuit order matters: the conversion is skipped when
    // there is nowhere to put the result.
    if (out == nullptr || cap == 0)
    {
        // No buffer at all, and no out-of-band way to report it.
    }
    else if (cap < s_kWallBufferChars || localtime_s (&local, &wall) != 0)
    {
        out[0] = L'\0';
    }
    else
    {
        swprintf_s (out, cap,
                    L"%02d:%02d:%02d.%03lld",
                    local.tm_hour,
                    local.tm_min,
                    local.tm_sec,
                    (long long) ms.count());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatUptime
//
//  MM:SS.mmm since `anchor`. FR-005.
//
////////////////////////////////////////////////////////////////////////////////

static void FormatUptime (
    std::chrono::steady_clock::time_point  anchor,
    wchar_t *                              out,
    size_t                                 cap)
{
    using namespace std::chrono;

    auto       now      = steady_clock::now();
    long long  totalMs  = 0;
    long long  minutes  = 0;
    long long  seconds  = 0;
    long long  millis   = 0;

    // `now < anchor` means the caller handed us an anchor from the future,
    // which steady_clock makes impossible for a real capture -- treat it as
    // an unset anchor and blank the cell rather than printing a wrapped
    // unsigned duration.
    if (out == nullptr || cap == 0)
    {
        // No buffer at all, and no out-of-band way to report it.
    }
    else if (cap < s_kUptimeBufferChars || now < anchor)
    {
        out[0] = L'\0';
    }
    else
    {
        totalMs = duration_cast<milliseconds> (now - anchor).count();
        minutes = totalMs / 60000;
        seconds = (totalMs / 1000) % 60;
        millis  = totalMs % 1000;

        swprintf_s (out, cap,
                    L"%02lld:%02lld.%03lld",
                    minutes,
                    seconds,
                    millis);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatCoord
//
//  Renders one of the address-mark / data-mark coordinate fields. A
//  cached value of -1 (no preceding address mark for a data read) prints
//  as "?" so the dialog row reads "T? S? V? (256 bytes)" rather than
//  emitting bare -1.
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring FormatCoord (wchar_t prefix, int value)
{
    return (value < 0) ? std::format (L"{}?", prefix)
                       : std::format (L"{}{}", prefix, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatDetail
//
//  Per-event-type Detail column string per FR-005.
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring FormatDetail (const Disk2Event & src)
{
    std::wstring  detail;



    switch (src.type)
    {
        case Disk2EventType::HeadStep:
            detail = std::format (L"quarter-track {} -> {}",
                                  src.payload.step.prevQt,
                                  src.payload.step.newQt);
            break;

        case Disk2EventType::HeadBump:
            detail = std::format (L"at quarter-track {}", src.payload.bump.atQt);
            break;

        case Disk2EventType::AddrMark:
            detail = std::format (L"{} {} {}",
                                  FormatCoord (L'T', src.payload.addrMark.track),
                                  FormatCoord (L'S', src.payload.addrMark.sector),
                                  FormatCoord (L'V', src.payload.addrMark.volume));
            break;

        // Reads and writes both report where the head was and how much
        // moved; the direction is already in the event label.
        case Disk2EventType::DataRead:
        case Disk2EventType::DataWrite:
            detail = std::format (L"{} {} {} ({} bytes)",
                                  FormatCoord (L'T', src.payload.dataMark.track),
                                  FormatCoord (L'S', src.payload.dataMark.sector),
                                  FormatCoord (L'V', src.payload.dataMark.volume),
                                  src.payload.dataMark.byteCount);
            break;

        case Disk2EventType::EventsLost:
            detail = std::format (L"[{} events lost]", src.payload.lost.count);
            break;

        case Disk2EventType::AudioStarted:
        case Disk2EventType::AudioRestarted:
        case Disk2EventType::AudioContinued:
        case Disk2EventType::AudioLoopStarted:
        case Disk2EventType::AudioLoopStopped:
            detail = std::format (L"kind={}", SoundKindLabel (src.payload.audio.kind));
            break;

        case Disk2EventType::AudioSilent:
            detail = std::format (L"kind={} reason={}",
                                  SoundKindLabel    (src.payload.audio.kind),
                                  SilentReasonLabel (src.payload.audio.reason));
            break;

        // Drive and motor transitions carry no payload worth spelling out --
        // the event label plus the Drive column already say it all.
        case Disk2EventType::DriveSelect:
        case Disk2EventType::DiskInserted:
        case Disk2EventType::DiskEjected:
        case Disk2EventType::MotorCommandOn:
        case Disk2EventType::MotorEngaged:
        case Disk2EventType::MotorCommandOff:
        case Disk2EventType::MotorDisengaged:
            break;
    }

    return detail;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PayloadDrive
//
//  Drive index for the FR-014 filter projection and the Drive column.
//  Spec-006 bug fix: every drive-specific event now carries its drive
//  on the top-level Disk2Event.drive field (stamped by the dialog's
//  IDisk2EventSink at fire time from the cached active drive). For
//  event types that already carry an explicit drive in their payload
//  (DriveSelect / DiskInserted / DiskEjected / audio outcomes), the
//  payload value is authoritative and matches the top-level stamp.
//  Returns Disk2EventDisplay::kFieldNotApplicable only for the
//  synthetic EventsLost marker (src.drive == -1).
//
////////////////////////////////////////////////////////////////////////////////

static int PayloadDrive (const Disk2Event & src)
{
    int  drive = Disk2EventDisplay::kFieldNotApplicable;



    switch (src.type)
    {
        case Disk2EventType::DriveSelect:
        case Disk2EventType::DiskInserted:
        case Disk2EventType::DiskEjected:
            drive = src.payload.drive.drive;
            break;

        case Disk2EventType::AudioStarted:
        case Disk2EventType::AudioRestarted:
        case Disk2EventType::AudioContinued:
        case Disk2EventType::AudioSilent:
        case Disk2EventType::AudioLoopStarted:
        case Disk2EventType::AudioLoopStopped:
            drive = src.payload.audio.drive;
            break;

        case Disk2EventType::EventsLost:
            // Synthetic marker -- belongs to no drive. Keeps the initializer.
            break;

        default:
            if (src.drive >= 0)
            {
                drive = src.drive;
            }

            break;
    }

    return drive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugDialogProjection::EventLabel
//
//  Human-readable Event column text. `cat` is unused today -- the type
//  alone determines the label -- but stays in the signature so a future
//  category can disambiguate without churning every call site.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring_view DebugDialogProjection::EventLabel (EventCategory cat, Disk2EventType type)
{
    std::wstring_view  label = L"?";



    (void) cat;

    switch (type)
    {
        case Disk2EventType::MotorCommandOn:    label = L"Motor command on";    break;
        case Disk2EventType::MotorEngaged:      label = L"Motor engaged";       break;
        case Disk2EventType::MotorCommandOff:   label = L"Motor command off";   break;
        case Disk2EventType::MotorDisengaged:   label = L"Motor disengaged";    break;
        case Disk2EventType::HeadStep:          label = L"Head step";           break;
        case Disk2EventType::HeadBump:          label = L"Head bump";           break;
        case Disk2EventType::AddrMark:          label = L"Address mark";        break;
        case Disk2EventType::DataRead:          label = L"Data read";           break;
        case Disk2EventType::DataWrite:         label = L"Data write";          break;
        case Disk2EventType::DriveSelect:       label = L"Drive select";        break;
        case Disk2EventType::DiskInserted:      label = L"Disk inserted";       break;
        case Disk2EventType::DiskEjected:       label = L"Disk ejected";        break;
        case Disk2EventType::EventsLost:        label = L"Events lost";         break;
        case Disk2EventType::AudioStarted:      label = L"Audio started";       break;
        case Disk2EventType::AudioRestarted:    label = L"Audio restarted";     break;
        case Disk2EventType::AudioContinued:    label = L"Audio continued";     break;
        case Disk2EventType::AudioSilent:       label = L"Audio silent";        break;
        case Disk2EventType::AudioLoopStarted:  label = L"Audio loop started";  break;
        case Disk2EventType::AudioLoopStopped:  label = L"Audio loop stopped";  break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugDialogProjection::FormatEvent
//
////////////////////////////////////////////////////////////////////////////////

void DebugDialogProjection::FormatEvent (
    const Disk2Event &                           src,
    std::chrono::steady_clock::time_point        uptimeAnchor,
    Disk2EventDisplay &                         out)
{
    out.category = src.category;
    out.type     = src.type;
    out.drive    = PayloadDrive (src);
    out.track    = Disk2EventDisplay::kFieldNotApplicable;
    out.sector   = Disk2EventDisplay::kFieldNotApplicable;

    if (src.type == Disk2EventType::AddrMark)
    {
        out.track  = src.payload.addrMark.track;
        out.sector = src.payload.addrMark.sector;
    }
    else if (src.type == Disk2EventType::DataRead
             || src.type == Disk2EventType::DataWrite)
    {
        out.track  = src.payload.dataMark.track;
        out.sector = src.payload.dataMark.sector;
    }

    FormatWallNow                 (out.wallStr.data(),   out.wallStr.size());
    FormatUptime                  (uptimeAnchor, out.uptimeStr.data(), out.uptimeStr.size());
    FormatCycleWithSeparators     (src.cycle, out.cycleStr.data(), out.cycleStr.size());

    out.detail = FormatDetail (src);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugDialogProjection::DrainAndProject
//
//  Drains the Disk II event ring and projects each event into a display row.
//
//  Dropped events are surfaced as a SYNTHETIC EventsLost row rather than
//  passing silently, and it is emitted BEFORE the drain so it appears in the
//  log at the point the gap actually occurred. A silent gap reads as a period
//  of no disk activity, which is exactly the wrong conclusion when the ring
//  overflowed because there was too much.
//
//  Draining loops until a short batch, so a burst that exceeds one batch is
//  fully consumed in a single call instead of trickling out over frames.
//
//  A sequence number is stamped per row so a stable sort can fall back to
//  arrival order for events sharing a timestamp -- disk events routinely land
//  in the same cycle.
//
//  A free function over plain containers, so the projection is unit-testable
//  by pushing events into a ring and reading rows out, with no panel involved.
//
////////////////////////////////////////////////////////////////////////////////

void DebugDialogProjection::DrainAndProject (
    Disk2EventRing &                             ring,
    std::deque<Disk2EventDisplay> &              deque,
    uint32_t                                     droppedCount,
    std::chrono::steady_clock::time_point        uptimeAnchor,
    uint64_t *                                   seqCounter)
{
    Disk2Event              batch[kDrainBatchSize] = {};
    uint32_t                drained                = 0;
    uint32_t                i                      = 0;
    Disk2EventDisplay       lostEntry;
    Disk2Event              syntheticLost          = {};

    if (droppedCount > 0)
    {
        syntheticLost.category          = EventCategory::Controller;
        syntheticLost.type              = Disk2EventType::EventsLost;
        syntheticLost.drive             = -1;
        syntheticLost.cycle             = 0;
        syntheticLost.payload.lost.count = droppedCount;

        FormatEvent (syntheticLost, uptimeAnchor, lostEntry);
        if (seqCounter != nullptr)
        {
            lostEntry.seq = (*seqCounter)++;
        }

        deque.push_back (std::move (lostEntry));
    }

    do
    {
        drained = ring.Drain (batch, kDrainBatchSize);

        for (i = 0; i < drained; i++)
        {
            Disk2EventDisplay  entry;

            FormatEvent (batch[i], uptimeAnchor, entry);
            if (seqCounter != nullptr)
            {
                entry.seq = (*seqCounter)++;
            }

            deque.push_back (std::move (entry));
        }
    }
    while (drained == kDrainBatchSize);

    while (deque.size() > kDisplayDequeCap)
    {
        deque.pop_front();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugDialogProjection::PreservedFocusItem
//
//  Spec-006 round-4 bug 5. Resolve where the user's focused row
//  should land after a filter rebuild. Implementation walks the
//  sorted filtered-indices vector with binary search:
//
//    * lower_bound (priorDequeIdx) finds the first surviving
//      filtered entry whose deque index is >= priorDequeIdx.
//    * If that entry's deque index equals priorDequeIdx, the
//      focused row is still in the projection -- focus it.
//    * Otherwise step back one to find the most recent earlier
//      filtered entry; if there is none (priorDequeIdx is smaller
//      than every surviving filtered index), fall back to row 0
//      per the spec.
//
////////////////////////////////////////////////////////////////////////////////

int DebugDialogProjection::PreservedFocusItem (
    uint32_t                       priorDequeIdx,
    const std::vector<uint32_t> &  newFilteredIndices) noexcept
{
    HRESULT hr        = S_OK;
    int     result    = -1;
    bool    hasFilter = false;
    auto    it        = newFilteredIndices.begin();

    hasFilter = !newFilteredIndices.empty();
    CBR (hasFilter);

    it = std::lower_bound (newFilteredIndices.begin(),
                           newFilteredIndices.end(),
                           priorDequeIdx);

    if (it != newFilteredIndices.end() && *it == priorDequeIdx)
    {
        result = static_cast<int> (it - newFilteredIndices.begin());
    }
    else if (it != newFilteredIndices.begin())
    {
        result = static_cast<int> ((it - 1) - newFilteredIndices.begin());
    }
    else
    {
        result = 0;
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugDialogProjection::ResolveSelection
//
//  GH #88. Seq-identity selection resolution: keep the selected event
//  selected (and let the caller scroll it into view) across a sort
//  reorder, and snap gracefully when it is filtered out or evicted. See
//  the header for the full rule set.
//
////////////////////////////////////////////////////////////////////////////////

DebugSelectionResult DebugDialogProjection::ResolveSelection (
    uint64_t                              selectedSeq,
    const std::deque<Disk2EventDisplay> & events,
    const std::vector<size_t> &           filteredIndices) noexcept
{
    DebugSelectionResult  result;                                  // {-1, 0}
    const size_t          kNone         = filteredIndices.size();  // one past any row
    size_t                exactRow      = kNone;
    size_t                bestBeforeRow = kNone;
    uint64_t              bestBeforeSeq = 0;
    size_t                earliestRow   = kNone;
    uint64_t              earliestSeq   = 0;

    // One pass gathers all three candidates. The exact match used to get its
    // own early-exit scan, but a miss then paid for two full sweeps, and a
    // miss is the case this function exists for.
    if (selectedSeq != 0 && !filteredIndices.empty())
    {
        for (size_t row = 0; row < filteredIndices.size(); ++row)
        {
            size_t    idx = filteredIndices[row];
            uint64_t  s   = 0;
            if (idx >= events.size())
            {
                continue;
            }

            s = events[idx].seq;

            // First match wins, matching the old scan's early exit.
            if (s == selectedSeq && exactRow == kNone)
            {
                exactRow = row;
            }

            if (earliestRow == kNone || s < earliestSeq)
            {
                earliestSeq = s;
                earliestRow = row;
            }

            if (s <= selectedSeq && (bestBeforeRow == kNone || s > bestBeforeSeq))
            {
                bestBeforeSeq = s;
                bestBeforeRow = row;
            }
        }

        // Identity first (a sort reorder does not lose the selection), then
        // nearest surviving event at-or-before it, then the earliest left.
        if (exactRow != kNone)
        {
            result.row = (int) exactRow;
            result.seq = selectedSeq;
        }
        else if (bestBeforeRow != kNone)
        {
            result.row = (int) bestBeforeRow;
            result.seq = bestBeforeSeq;
        }
        else if (earliestRow != kNone)
        {
            result.row = (int) earliestRow;
            result.seq = earliestSeq;
        }
    }

    return result;
}
