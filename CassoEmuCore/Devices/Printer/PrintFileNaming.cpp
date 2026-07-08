#include "Pch.h"

#include "Devices/Printer/PrintFileNaming.h"




static constexpr int   s_kMaxOrdinal = 100000;   // guard against a pathological loop




////////////////////////////////////////////////////////////////////////////////
//
//  CandidateName
//
//  The first candidate is the bare base name; later ones gain a " (n)" suffix,
//  matching the familiar shell de-duplication style.
//
////////////////////////////////////////////////////////////////////////////////

wstring PrintFileNaming::CandidateName (const wstring & base, int ordinal)
{
    if (ordinal <= 1)
    {
        return base + L".png";
    }

    return base + L" (" + to_wstring (ordinal) + L").png";
}




////////////////////////////////////////////////////////////////////////////////
//
//  ComposePngPath
//
////////////////////////////////////////////////////////////////////////////////

fs::path PrintFileNaming::ComposePngPath (
    const fs::path &                          folder,
    const SYSTEMTIME &                        when,
    const function<bool (const fs::path &)> & taken)
{
    wstring    base    = std::format (L"Casso Print {:04}-{:02}-{:02} {:02}{:02}{:02}",
                                      (int) when.wYear, (int) when.wMonth, (int) when.wDay,
                                      (int) when.wHour, (int) when.wMinute, (int) when.wSecond);
    fs::path   result  = folder / CandidateName (base, 1);
    int        ordinal = 1;

    for (ordinal = 1; ordinal <= s_kMaxOrdinal; ordinal++)
    {
        result = folder / CandidateName (base, ordinal);

        if (!taken (result))
        {
            break;
        }
    }

    return result;
}
