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
////////////////////////////////////////////////////////////////////////////////

static const wchar_t * SoundKindLabel (SoundKind k)
{
    switch (k)
    {
        case SoundKind::MotorLoop:  return L"MotorLoop";
        case SoundKind::HeadStep:   return L"HeadStep";
        case SoundKind::HeadStop:   return L"HeadStop";
        case SoundKind::DoorOpen:   return L"DoorOpen";
        case SoundKind::DoorClose:  return L"DoorClose";
    }

    return L"Unknown";
}





////////////////////////////////////////////////////////////////////////////////
//
//  SilentReasonLabel
//
////////////////////////////////////////////////////////////////////////////////

static const wchar_t * SilentReasonLabel (SilentReason r)
{
    switch (r)
    {
        case SilentReason::DriveAudioDisabled:   return L"DriveAudioDisabled";
        case SilentReason::BufferMissing:        return L"BufferMissing";
        case SilentReason::NoSourceRegistered:   return L"NoSourceRegistered";
        case SilentReason::ColdBootSuppression:  return L"ColdBootSuppression";
        case SilentReason::NoDiskPresent:        return L"NoDiskPresent";
    }

    return L"Unknown";
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
    errno_t    err       = 0;

    if (out == nullptr || cap < s_kWallBufferChars)
    {
        if (out != nullptr && cap > 0)
        {
            out[0] = L'\0';
        }

        return;
    }

    err = localtime_s (&local, &wall);

    if (err != 0)
    {
        out[0] = L'\0';
        return;
    }

    swprintf_s (out, cap,
                L"%02d:%02d:%02d.%03lld",
                local.tm_hour,
                local.tm_min,
                local.tm_sec,
                (long long) ms.count());
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

    if (out == nullptr || cap < s_kUptimeBufferChars)
    {
        if (out != nullptr && cap > 0)
        {
            out[0] = L'\0';
        }

        return;
    }

    if (now < anchor)
    {
        out[0] = L'\0';
        return;
    }

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





////////////////////////////////////////////////////////////////////////////////
//
//  FormatTrackSectorVolume
//
//  Helper that renders one of the address-mark / data-mark coordinate
//  fields. A cached value of -1 (no preceding address mark for a data
//  read) prints as "?" so the dialog row reads "T? S? V? (256 bytes)"
//  rather than emitting bare -1.
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring FormatCoord (wchar_t prefix, int value)
{
    if (value < 0)
    {
        return std::format (L"{}?", prefix);
    }

    return std::format (L"{}{}", prefix, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatDetail
//
//  Per-event-type Detail column string per FR-005.
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring FormatDetail (const DiskIIEvent & src)
{
    switch (src.type)
    {
        case DiskIIEventType::HeadStep:
            return std::format (L"quarter-track {} -> {}",
                                src.payload.step.prevQt,
                                src.payload.step.newQt);

        case DiskIIEventType::HeadBump:
            return std::format (L"at quarter-track {}", src.payload.bump.atQt);

        case DiskIIEventType::AddrMark:
            return std::format (L"{} {} {}",
                                FormatCoord (L'T', src.payload.addrMark.track),
                                FormatCoord (L'S', src.payload.addrMark.sector),
                                FormatCoord (L'V', src.payload.addrMark.volume));

        case DiskIIEventType::DataRead:
            return std::format (L"{} {} {} ({} bytes)",
                                FormatCoord (L'T', src.payload.dataMark.track),
                                FormatCoord (L'S', src.payload.dataMark.sector),
                                FormatCoord (L'V', src.payload.dataMark.volume),
                                src.payload.dataMark.byteCount);

        case DiskIIEventType::DataWrite:
            return std::format (L"{} {} {} ({} bytes)",
                                FormatCoord (L'T', src.payload.dataMark.track),
                                FormatCoord (L'S', src.payload.dataMark.sector),
                                FormatCoord (L'V', src.payload.dataMark.volume),
                                src.payload.dataMark.byteCount);

        case DiskIIEventType::DriveSelect:
        case DiskIIEventType::DiskInserted:
        case DiskIIEventType::DiskEjected:
            return std::wstring();

        case DiskIIEventType::EventsLost:
            return std::format (L"[{} events lost]", src.payload.lost.count);

        case DiskIIEventType::AudioStarted:
        case DiskIIEventType::AudioRestarted:
        case DiskIIEventType::AudioContinued:
        case DiskIIEventType::AudioLoopStarted:
        case DiskIIEventType::AudioLoopStopped:
            return std::format (L"kind={}", SoundKindLabel (src.payload.audio.kind));

        case DiskIIEventType::AudioSilent:
            return std::format (L"kind={} reason={}",
                                SoundKindLabel    (src.payload.audio.kind),
                                SilentReasonLabel (src.payload.audio.reason));

        case DiskIIEventType::MotorCommandOn:
        case DiskIIEventType::MotorEngaged:
        case DiskIIEventType::MotorCommandOff:
        case DiskIIEventType::MotorDisengaged:
            return std::wstring();
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PayloadDrive
//
//  Drive index for the FR-014 filter projection and the Drive column.
//  Spec-006 bug fix: every drive-specific event now carries its drive
//  on the top-level DiskIIEvent.drive field (stamped by the dialog's
//  IDiskIIEventSink at fire time from the cached active drive). For
//  event types that already carry an explicit drive in their payload
//  (DriveSelect / DiskInserted / DiskEjected / audio outcomes), the
//  payload value is authoritative and matches the top-level stamp.
//  Returns DiskIIEventDisplay::kFieldNotApplicable only for the
//  synthetic EventsLost marker (src.drive == -1).
//
////////////////////////////////////////////////////////////////////////////////

static int PayloadDrive (const DiskIIEvent & src)
{
    switch (src.type)
    {
        case DiskIIEventType::DriveSelect:
        case DiskIIEventType::DiskInserted:
        case DiskIIEventType::DiskEjected:
            return src.payload.drive.drive;

        case DiskIIEventType::AudioStarted:
        case DiskIIEventType::AudioRestarted:
        case DiskIIEventType::AudioContinued:
        case DiskIIEventType::AudioSilent:
        case DiskIIEventType::AudioLoopStarted:
        case DiskIIEventType::AudioLoopStopped:
            return src.payload.audio.drive;

        case DiskIIEventType::EventsLost:
            return DiskIIEventDisplay::kFieldNotApplicable;

        default:
            if (src.drive < 0)
            {
                return DiskIIEventDisplay::kFieldNotApplicable;
            }
            return src.drive;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugDialogProjection::EventLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring_view DebugDialogProjection::EventLabel (EventCategory cat, DiskIIEventType type)
{
    (void) cat;

    switch (type)
    {
        case DiskIIEventType::MotorCommandOn:    return L"Motor command on";
        case DiskIIEventType::MotorEngaged:      return L"Motor engaged";
        case DiskIIEventType::MotorCommandOff:   return L"Motor command off";
        case DiskIIEventType::MotorDisengaged:   return L"Motor disengaged";
        case DiskIIEventType::HeadStep:          return L"Head step";
        case DiskIIEventType::HeadBump:          return L"Head bump";
        case DiskIIEventType::AddrMark:          return L"Address mark";
        case DiskIIEventType::DataRead:          return L"Data read";
        case DiskIIEventType::DataWrite:         return L"Data write";
        case DiskIIEventType::DriveSelect:       return L"Drive select";
        case DiskIIEventType::DiskInserted:      return L"Disk inserted";
        case DiskIIEventType::DiskEjected:       return L"Disk ejected";
        case DiskIIEventType::EventsLost:        return L"Events lost";
        case DiskIIEventType::AudioStarted:      return L"Audio started";
        case DiskIIEventType::AudioRestarted:    return L"Audio restarted";
        case DiskIIEventType::AudioContinued:    return L"Audio continued";
        case DiskIIEventType::AudioSilent:       return L"Audio silent";
        case DiskIIEventType::AudioLoopStarted:  return L"Audio loop started";
        case DiskIIEventType::AudioLoopStopped:  return L"Audio loop stopped";
    }

    return L"?";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugDialogProjection::FormatEvent
//
////////////////////////////////////////////////////////////////////////////////

void DebugDialogProjection::FormatEvent (
    const DiskIIEvent &                          src,
    std::chrono::steady_clock::time_point        uptimeAnchor,
    DiskIIEventDisplay &                         out)
{
    out.category = src.category;
    out.type     = src.type;
    out.drive    = PayloadDrive (src);
    out.track    = DiskIIEventDisplay::kFieldNotApplicable;
    out.sector   = DiskIIEventDisplay::kFieldNotApplicable;

    if (src.type == DiskIIEventType::AddrMark)
    {
        out.track  = src.payload.addrMark.track;
        out.sector = src.payload.addrMark.sector;
    }
    else if (src.type == DiskIIEventType::DataRead
             || src.type == DiskIIEventType::DataWrite)
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
////////////////////////////////////////////////////////////////////////////////

void DebugDialogProjection::DrainAndProject (
    DiskIIEventRing &                            ring,
    std::deque<DiskIIEventDisplay> &             deque,
    uint32_t                                     droppedCount,
    std::chrono::steady_clock::time_point        uptimeAnchor)
{
    DiskIIEvent             batch[kDrainBatchSize] = {};
    uint32_t                drained                = 0;
    uint32_t                i                      = 0;
    DiskIIEventDisplay      lostEntry;
    DiskIIEvent             syntheticLost          = {};

    if (droppedCount > 0)
    {
        syntheticLost.category          = EventCategory::Controller;
        syntheticLost.type              = DiskIIEventType::EventsLost;
        syntheticLost.drive             = -1;
        syntheticLost.cycle             = 0;
        syntheticLost.payload.lost.count = droppedCount;

        FormatEvent (syntheticLost, uptimeAnchor, lostEntry);
        deque.push_back (std::move (lostEntry));
    }

    do
    {
        drained = ring.Drain (batch, kDrainBatchSize);

        for (i = 0; i < drained; i++)
        {
            DiskIIEventDisplay  entry;

            FormatEvent (batch[i], uptimeAnchor, entry);
            deque.push_back (std::move (entry));
        }
    }
    while (drained == kDrainBatchSize);

    while (deque.size() > kDisplayDequeCap)
    {
        deque.pop_front();
    }
}
