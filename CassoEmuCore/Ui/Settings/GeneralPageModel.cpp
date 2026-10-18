#include "Pch.h"

#include "GeneralPageModel.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageModel::IsOfferChecked
//
//  Only "decline" stops an offer; "ask" and "allow" both leave it on.
//
////////////////////////////////////////////////////////////////////////////////

bool GeneralPageModel::IsOfferChecked (std::string_view consent)
{
    return consent != kpszConsentDecline;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageModel::MakeConsentFromChecked
//
//  Turning an offer back on returns it to "ask", so the next offer is shown
//  rather than taken without asking.
//
////////////////////////////////////////////////////////////////////////////////

std::string GeneralPageModel::MakeConsentFromChecked (bool checked)
{
    return checked ? kpszConsentAsk : kpszConsentDecline;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageModel::GetDayNumber
//
//  Days since the Unix epoch for the date part of a SYSTEMTIME, so two dates
//  can be compared across month and year boundaries.
//
////////////////////////////////////////////////////////////////////////////////

std::int64_t GeneralPageModel::GetDayNumber (const SYSTEMTIME & date)
{
    std::chrono::year_month_day  ymd { std::chrono::year  { (int) date.wYear },
                                       std::chrono::month { (unsigned) date.wMonth },
                                       std::chrono::day   { (unsigned) date.wDay } };



    return std::chrono::sys_days { ymd }.time_since_epoch().count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageModel::MakeClockText
//
//  A 12-hour clock time, such as "8:46 AM".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring GeneralPageModel::MakeClockText (const SYSTEMTIME & time)
{
    constexpr int  kHoursPerHalfDay = 12;



    int            hour             = time.wHour % kHoursPerHalfDay;
    bool           isPm             = time.wHour >= kHoursPerHalfDay;



    if (hour == 0)
    {
        hour = kHoursPerHalfDay;
    }

    return std::format (L"{}:{:02} {}", hour, (int) time.wMinute, isPm ? L"PM" : L"AM");
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageModel::MakeLastCheckedText
//
//  "Last checked today at 8:46 AM.", "... yesterday at ...", or a short date
//  for anything older: the month and day this year, with the year before
//  that. A null time means no check has ever finished.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring GeneralPageModel::MakeLastCheckedText (const SYSTEMTIME * checkedLocal, const SYSTEMTIME & nowLocal)
{
    static constexpr const wchar_t * s_kpszMonths[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
                                                        L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };



    std::wstring    text;
    std::int64_t    daysAgo   = 0;
    const wchar_t * pszMonth  = L"";
    size_t          monthIdx  = 0;



    if (checkedLocal == nullptr)
    {
        return L"Never checked.";
    }

    daysAgo  = GetDayNumber (nowLocal) - GetDayNumber (*checkedLocal);
    monthIdx = (size_t) checkedLocal->wMonth - 1;
    pszMonth = (monthIdx < std::size (s_kpszMonths)) ? s_kpszMonths[monthIdx] : L"";

    if (daysAgo == 0)
    {
        text = L"Last checked today at " + MakeClockText (*checkedLocal) + L".";
    }
    else if (daysAgo == 1)
    {
        text = L"Last checked yesterday at " + MakeClockText (*checkedLocal) + L".";
    }
    else if (checkedLocal->wYear == nowLocal.wYear)
    {
        text = std::format (L"Last checked on {} {}.", pszMonth, (int) checkedLocal->wDay);
    }
    else
    {
        text = std::format (L"Last checked on {} {}, {}.", pszMonth, (int) checkedLocal->wDay, (int) checkedLocal->wYear);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageModel::TryGetLocalTime
//
//  Unix seconds to a local SYSTEMTIME, through the current time zone rules.
//
////////////////////////////////////////////////////////////////////////////////

bool GeneralPageModel::TryGetLocalTime (std::int64_t utcSeconds, SYSTEMTIME & outLocal)
{
    constexpr std::int64_t  kEpochDeltaSeconds = 11644473600LL;   // 1601-01-01 to 1970-01-01
    constexpr std::int64_t  kTicksPerSecond    = 10000000LL;      // FILETIME counts 100 ns



    HRESULT         hr        = S_OK;
    ULARGE_INTEGER  ticks     = {};
    FILETIME        fileTime  = {};
    SYSTEMTIME      utc       = {};
    BOOL            converted = FALSE;
    bool            isValid   = utcSeconds > 0;



    CBR (isValid);

    ticks.QuadPart          = (ULONGLONG) ((utcSeconds + kEpochDeltaSeconds) * kTicksPerSecond);
    fileTime.dwLowDateTime  = ticks.LowPart;
    fileTime.dwHighDateTime = ticks.HighPart;

    converted = FileTimeToSystemTime (&fileTime, &utc);
    CWR (converted);

    converted = SystemTimeToTzSpecificLocalTime (nullptr, &utc, &outLocal);
    CWR (converted);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageModel::MakeLastCheckedTextNow
//
//  The last-checked line for a saved check time against the clock now. A
//  time of 0 is the "never" the prefs default to.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring GeneralPageModel::MakeLastCheckedTextNow (std::int64_t checkedUtc)
{
    SYSTEMTIME  checkedLocal = {};
    SYSTEMTIME  nowLocal     = {};
    bool        hasChecked   = TryGetLocalTime (checkedUtc, checkedLocal);



    GetLocalTime (&nowLocal);

    return MakeLastCheckedText (hasChecked ? &checkedLocal : nullptr, nowLocal);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageModel::MakeSkippedText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring GeneralPageModel::MakeSkippedText (std::string_view skippedVersion)
{
    return L"Skipped version: " + TextEncoding::Utf8ToWide (std::string (skippedVersion));
}
