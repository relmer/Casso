#include "Pch.h"

#include "Devices/Printer/PrintFileNaming.h"




static constexpr int   s_kMaxOrdinal = 100000;   // guard against a pathological loop





////////////////////////////////////////////////////////////////////////////////
//
//  ComposeCandidateName
//
//  The first candidate is the bare base name; later ones gain a " (n)" suffix,
//  matching the familiar shell de-duplication style.
//
////////////////////////////////////////////////////////////////////////////////

wstring PrintFileNaming::ComposeCandidateName (const wstring & base,
                                               const wstring & extension,
                                               int             ordinal)
{
    return (ordinal <= 1) ? (base + extension)
                          : (base + L" (" + to_wstring (ordinal) + L")" + extension);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComposeTimestampedPath
//
//  The timestamp is LOCAL wall-clock, to the second, and deliberately not
//  finer. A second is short enough that two files rarely share one, and the
//  " (n)" suffix already covers the case when they do -- where sub-second
//  digits would put noise in every name to spare the rare collision.
//
////////////////////////////////////////////////////////////////////////////////

fs::path PrintFileNaming::ComposeTimestampedPath (
    const fs::path &                          folder,
    const wstring &                           baseName,
    const wstring &                           extension,
    const SYSTEMTIME &                        when,
    const function<bool (const fs::path &)> & taken)
{
    fs::path  result;
    wstring   base;
    int       ordinal = 0;



    base = std::format (L"{} {:04}-{:02}-{:02} {:02}{:02}{:02}",
                        baseName,
                        (int) when.wYear, (int) when.wMonth, (int) when.wDay,
                        (int) when.wHour, (int) when.wMinute, (int) when.wSecond);

    for (ordinal = 1; ordinal <= s_kMaxOrdinal; ordinal++)
    {
        result = folder / ComposeCandidateName (base, extension, ordinal);

        if (!taken (result))
        {
            break;
        }
    }

    return result;
}
