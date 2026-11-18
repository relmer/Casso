#include "Pch.h"

#include "Devices/Mm58167.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





static constexpr int64_t  s_kMsPerDay           = 86400000;
static constexpr int64_t  s_kMsPerHour          = 3600000;
static constexpr int64_t  s_kMsPerMinute        = 60000;
static constexpr int64_t  s_kMsPerSecond        = 1000;
static constexpr int64_t  s_kMsPerTenth         = 100;
static constexpr int64_t  s_kUnixEpochFileTime  = 116444736000000000;
static constexpr int64_t  s_kFileTimePerMs      = 10000;
static constexpr int      s_kDaysPerWeek        = 7;
static constexpr int      s_kMonthsPerYear      = 12;
static constexpr int64_t  s_kCalendarCycleDays  = 365 * s_kDaysPerWeek;
static constexpr Byte     s_kDontCareNibble     = 0x0C;
static constexpr int      s_kMonthDays[s_kMonthsPerYear] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };





////////////////////////////////////////////////////////////////////////////////
//
//  Mm58167
//
////////////////////////////////////////////////////////////////////////////////

Mm58167::Mm58167()
{
    m_counters[kWeekday] = 1;
    m_counters[kDay]     = 1;
    m_counters[kMonth]   = 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTimeSource
//
////////////////////////////////////////////////////////////////////////////////

void Mm58167::SetTimeSource (TimeSource source)
{
    m_source = std::move (source);
    m_lastMs = GetNow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerOn
//
////////////////////////////////////////////////////////////////////////////////

void Mm58167::PowerOn()
{
    int64_t    now  = GetNow();
    LocalTime  time = SplitLocalTime (now);
    Byte       day  = 0;
    Byte       mon  = 0;



    m_counters[kThousandths] = (Byte) ((time.millisecond % 10) << 4);
    m_counters[kHundredths]  = (Byte) ((((time.millisecond / 100) % 10) << 4) | ((time.millisecond / 10) % 10));
    m_counters[kSeconds]     = (Byte) ToBcd (time.second);
    m_counters[kMinutes]     = (Byte) ToBcd (time.minute);
    m_counters[kHours]       = (Byte) ToBcd (time.hour);
    m_counters[kWeekday]     = (Byte) time.weekday;
    m_counters[kDay]         = (Byte) ToBcd (time.day);
    m_counters[kMonth]       = (Byte) ToBcd (time.month);

    m_latches = {};
    EncodeSosYear ((Byte) ToBcd (time.year % 100), day, mon);
    m_latches[kDayLatch   - kFirstLatch] = day;
    m_latches[kMonthLatch - kFirstLatch] = mon;

    m_interruptStatus  = 0;
    m_interruptControl = 0;
    m_standby          = 0;
    m_status           = false;
    m_lastMs           = now;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  A counter read advances the counters first. Reading the status bit or
//  the interrupt status clears it.
//
////////////////////////////////////////////////////////////////////////////////

Byte Mm58167::Read (Byte reg)
{
    Byte  value = 0;



    reg &= (Byte) (kRegisterCount - 1);

    if (reg < kCounterCount)
    {
        Advance();
    }

    value = Peek (reg);

    if (reg == kStatus)
    {
        m_status = false;
    }
    else if (reg == kInterruptStatus)
    {
        m_interruptStatus = 0;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Peek
//
////////////////////////////////////////////////////////////////////////////////

Byte Mm58167::Peek (Byte reg) const
{
    Byte  value = 0;



    reg &= (Byte) (kRegisterCount - 1);

    if (reg < kCounterCount)
    {
        value = m_counters[reg];
    }
    else if (reg < kInterruptStatus)
    {
        value = m_latches[(size_t) (reg - kFirstLatch)];
    }
    else if (reg == kInterruptStatus)
    {
        value = m_interruptStatus;
    }
    else if (reg == kInterruptControl)
    {
        value = m_interruptControl;
    }
    else if (reg == kStatus)
    {
        value = m_status ? 1 : 0;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  A counter write takes effect from now; the thousandths counter has only
//  its upper nibble. The counter and RAM resets clear the counters or
//  latches whose bits are set, a reset day, weekday or month counter to 1.
//  GO clears the counters from the thousandths to the seconds.
//
////////////////////////////////////////////////////////////////////////////////

void Mm58167::Write (Byte reg, Byte value)
{
    reg &= (Byte) (kRegisterCount - 1);

    if (reg < kCounterCount || reg == kCounterReset || reg == kGo)
    {
        Advance();
    }

    if (reg < kCounterCount)
    {
        m_counters[reg] = (reg == kThousandths) ? (Byte) (value & 0xF0) : value;
    }
    else if (reg < kInterruptStatus)
    {
        m_latches[(size_t) (reg - kFirstLatch)] = value;
    }
    else if (reg == kInterruptControl)
    {
        m_interruptControl = value;
    }
    else if (reg == kCounterReset)
    {
        for (int i = 0; i < kCounterCount; i++)
        {
            if ((value & (1 << i)) != 0)
            {
                m_counters[(size_t) i] = (i >= kWeekday) ? 1 : 0;
            }
        }
    }
    else if (reg == kRamReset)
    {
        for (int i = 0; i < kCounterCount; i++)
        {
            if ((value & (1 << i)) != 0)
            {
                m_latches[(size_t) i] = 0;
            }
        }
    }
    else if (reg == kGo)
    {
        m_counters[kThousandths] = 0;
        m_counters[kHundredths]  = 0;
        m_counters[kSeconds]     = 0;
    }
    else if (reg == kStandby)
    {
        m_standby = (Byte) (value & 0x01);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SplitLocalTime
//
////////////////////////////////////////////////////////////////////////////////

Mm58167::LocalTime Mm58167::SplitLocalTime (int64_t localMs)
{
    using namespace std::chrono;

    LocalTime         time;
    sys_days          day   = floor<days> (sys_time<milliseconds> (milliseconds (localMs)));
    year_month_day    date  { day };
    int64_t           inDay = localMs - duration_cast<milliseconds> (day.time_since_epoch()).count();



    time.year        = (int) date.year();
    time.month       = (int) (unsigned) date.month();
    time.day         = (int) (unsigned) date.day();
    time.weekday     = (int) weekday (day).c_encoding() + 1;
    time.hour        = (int) (inDay / s_kMsPerHour);
    time.minute      = (int) ((inDay / s_kMsPerMinute) % 60);
    time.second      = (int) ((inDay / s_kMsPerSecond) % 60);
    time.millisecond = (int) (inDay % s_kMsPerSecond);

    return time;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EncodeSosYear
//
//  The upper nibble of each latch and the day latch's lower nibble are $C
//  or more, which the comparison ignores.
//
////////////////////////////////////////////////////////////////////////////////

void Mm58167::EncodeSosYear (Byte bcdYear, Byte & outDayLatch, Byte & outMonthLatch)
{
    outMonthLatch = (Byte) ((bcdYear >> 2) | 0xC0);
    outDayLatch   = (Byte) (bcdYear | 0xFC);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Advance
//
//  Adds the milliseconds since the last access, rippling from the
//  thousandths to the months, and sets the interrupt status bits that are
//  enabled for each unit that rolled over.
//
////////////////////////////////////////////////////////////////////////////////

void Mm58167::Advance()
{
    int64_t                           now     = GetNow();
    int64_t                           elapsed = now - m_lastMs;
    std::array<Byte, kCounterCount>   before  = m_counters;
    int64_t                           inDay   = 0;
    int64_t                           total   = 0;
    Byte                              rolled  = 0;



    m_lastMs = now;

    if (elapsed <= 0)
    {
        return;
    }

    inDay = (int64_t) (m_counters[kThousandths] >> 4)
          + (int64_t) (m_counters[kHundredths] & 0x0F) * 10
          + (int64_t) (m_counters[kHundredths] >> 4) * s_kMsPerTenth
          + (int64_t) FromBcd (m_counters[kSeconds]) * s_kMsPerSecond
          + (int64_t) FromBcd (m_counters[kMinutes]) * s_kMsPerMinute
          + (int64_t) FromBcd (m_counters[kHours])   * s_kMsPerHour;
    total = inDay + elapsed;

    rolled |= (total / s_kMsPerTenth  != inDay / s_kMsPerTenth)  ? kIrqTenth  : 0;
    rolled |= (total / s_kMsPerSecond != inDay / s_kMsPerSecond) ? kIrqSecond : 0;
    rolled |= (total / s_kMsPerMinute != inDay / s_kMsPerMinute) ? kIrqMinute : 0;
    rolled |= (total / s_kMsPerHour   != inDay / s_kMsPerHour)   ? kIrqHour   : 0;

    AdvanceDays (total / s_kMsPerDay, rolled);

    total %= s_kMsPerDay;

    m_counters[kThousandths] = (Byte) ((total % 10) << 4);
    m_counters[kHundredths]  = (Byte) ((((total / s_kMsPerTenth) % 10) << 4) | ((total / 10) % 10));
    m_counters[kSeconds]     = (Byte) ToBcd ((int) ((total / s_kMsPerSecond) % 60));
    m_counters[kMinutes]     = (Byte) ToBcd ((int) ((total / s_kMsPerMinute) % 60));
    m_counters[kHours]       = (Byte) ToBcd ((int) (total / s_kMsPerHour));

    if (m_counters != before)
    {
        m_status = true;
        CheckCompare();
    }

    m_interruptStatus |= (Byte) (rolled & m_interruptControl);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceDays
//
//  February is 28 days, as on the chip. The calendar and the week repeat
//  every seven years of 365 days, so a long absence advances by the rest.
//
////////////////////////////////////////////////////////////////////////////////

void Mm58167::AdvanceDays (int64_t days, Byte & outRolled)
{
    int  weekday = m_counters[kWeekday] & 0x0F;
    int  day     = FromBcd (m_counters[kDay]);
    int  month   = FromBcd (m_counters[kMonth]);



    if (days <= 0)
    {
        return;
    }

    outRolled |= kIrqDay;

    if (days > s_kCalendarCycleDays)
    {
        days = s_kCalendarCycleDays + days % s_kCalendarCycleDays;
    }

    for (int64_t i = 0; i < days; i++)
    {
        weekday = (weekday % s_kDaysPerWeek) + 1;
        outRolled |= (weekday == 1) ? kIrqWeek : 0;

        if (++day > GetMonthDays (month))
        {
            day        = 1;
            month      = (month % s_kMonthsPerYear) + 1;
            outRolled |= kIrqMonth;
        }
    }

    m_counters[kWeekday] = (Byte) weekday;
    m_counters[kDay]     = (Byte) ToBcd (day);
    m_counters[kMonth]   = (Byte) ToBcd (month);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CheckCompare
//
//  Each counter nibble against its latch nibble, a latch nibble of $C or
//  more matching anything.
//
////////////////////////////////////////////////////////////////////////////////

void Mm58167::CheckCompare()
{
    bool  isMatch = true;



    for (int i = 0; i < kCounterCount && isMatch; i++)
    {
        for (int shift = 0; shift <= 4 && isMatch; shift += 4)
        {
            Byte  latch   = (Byte) ((m_latches[(size_t) i]  >> shift) & 0x0F);
            Byte  counter = (Byte) ((m_counters[(size_t) i] >> shift) & 0x0F);



            isMatch = latch >= s_kDontCareNibble || latch == counter;
        }
    }

    if (isMatch)
    {
        m_interruptStatus |= (Byte) (kIrqCompare & m_interruptControl);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMonthDays
//
////////////////////////////////////////////////////////////////////////////////

int Mm58167::GetMonthDays (int month)
{
    return (month >= 1 && month <= s_kMonthsPerYear) ? s_kMonthDays[month - 1] : 31;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNow
//
//  The host's local time when there is no time source.
//
////////////////////////////////////////////////////////////////////////////////

int64_t Mm58167::GetNow() const
{
    SYSTEMTIME      local = {};
    FILETIME        file  = {};
    ULARGE_INTEGER  value = {};



    if (m_source)
    {
        return m_source();
    }

    GetLocalTime (&local);
    SystemTimeToFileTime (&local, &file);

    value.LowPart  = file.dwLowDateTime;
    value.HighPart = file.dwHighDateTime;

    return ((int64_t) value.QuadPart - s_kUnixEpochFileTime) / s_kFileTimePerMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
//  The counters as of the save. The time since is host time, so a load
//  advances them from the load on.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Mm58167::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteBytes (m_counters.data(), m_counters.size());
    writer.WriteBytes (m_latches.data(),  m_latches.size());
    writer.WriteByte  (m_interruptStatus);
    writer.WriteByte  (m_interruptControl);
    writer.WriteByte  (m_standby);
    writer.WriteBool  (m_status);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Mm58167::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadBytes (m_counters.data(), m_counters.size());
    reader.ReadBytes (m_latches.data(),  m_latches.size());
    reader.ReadByte  (m_interruptStatus);
    reader.ReadByte  (m_interruptControl);
    reader.ReadByte  (m_standby);
    reader.ReadBool  (m_status);

    hr = reader.EndSection();
    CHR (hr);

    m_lastMs = GetNow();

Error:
    return hr;
}
