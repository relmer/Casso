#include "Pch.h"

#include "TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TextEncoding::GetConsoleCodePage
//
//  GetConsoleOutputCP answers for a redirected handle too, which is what makes
//  it the right question: a caller writing to a pipe wants the same bytes a
//  caller writing to a window would get, or the two disagree about what a file
//  captured from the tool contains.
//
////////////////////////////////////////////////////////////////////////////////

unsigned TextEncoding::GetConsoleCodePage()
{
    unsigned  codePage = GetConsoleOutputCP();



    // Zero comes back for a process with no console at all. The narrow code
    // page is then the only defensible answer, and it makes Convert a no-op
    // rather than a lossy pass through an encoding nobody chose.
    if (codePage == 0)
    {
        codePage = GetNarrowCodePage();
    }

    return codePage;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextEncoding::GetNarrowCodePage
//
////////////////////////////////////////////////////////////////////////////////

unsigned TextEncoding::GetNarrowCodePage()
{
    return GetACP();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextEncoding::Convert
//
//  Through UTF-16, because that is the only encoding Windows converts every
//  code page to and from. A direct table between two arbitrary code pages does
//  not exist.
//
//  THE SUBSTITUTION CHARACTER IS ONLY ALLOWED TO BE NAMED FOR CODE PAGES THAT
//  ACCEPT ONE. WideCharToMultiByte refuses outright -- ERROR_INVALID_PARAMETER,
//  no output at all -- when a default character is supplied for UTF-8 or UTF-7,
//  so a single call shape that names one unconditionally would turn every
//  conversion into a console's code page into an empty string. That failure
//  looks exactly like a message the program never produced.
//
//  A failed measurement or a failed conversion returns the input unchanged
//  rather than nothing. Bytes in the wrong code page are still readable; an
//  empty diagnostic is not.
//
////////////////////////////////////////////////////////////////////////////////

std::string TextEncoding::Convert (const std::string & text,
                                   unsigned            fromCodePage,
                                   unsigned            toCodePage)
{
    std::wstring  wide;
    std::string   narrow;
    int           wideCount     = 0;
    int           narrowCount   = 0;
    bool          canSubstitute = toCodePage != CP_UTF8 && toCodePage != CP_UTF7;



    if (fromCodePage == toCodePage || text.empty())
    {
        return text;
    }

    wideCount = MultiByteToWideChar (fromCodePage, 0, text.c_str(), (int) text.size(),
                                     nullptr, 0);

    if (wideCount <= 0)
    {
        return text;
    }

    wide.resize ((size_t) wideCount);

    wideCount = MultiByteToWideChar (fromCodePage, 0, text.c_str(), (int) text.size(),
                                     wide.data(), wideCount);

    if (wideCount <= 0)
    {
        return text;
    }

    narrowCount = WideCharToMultiByte (toCodePage, 0, wide.data(), wideCount,
                                       nullptr, 0,
                                       canSubstitute ? "?" : nullptr,
                                       nullptr);

    if (narrowCount <= 0)
    {
        return text;
    }

    narrow.resize ((size_t) narrowCount);

    narrowCount = WideCharToMultiByte (toCodePage, 0, wide.data(), wideCount,
                                       narrow.data(), narrowCount,
                                       canSubstitute ? "?" : nullptr,
                                       nullptr);

    if (narrowCount <= 0)
    {
        return text;
    }

    return narrow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextEncoding::NarrowToConsole
//
////////////////////////////////////////////////////////////////////////////////

std::string TextEncoding::NarrowToConsole (const std::string & text)
{
    return Convert (text, GetNarrowCodePage(), GetConsoleCodePage());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextEncoding::Utf8ToNarrow
//
////////////////////////////////////////////////////////////////////////////////

std::string TextEncoding::Utf8ToNarrow (const std::string & text)
{
    return Convert (text, CP_UTF8, GetNarrowCodePage());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextEncoding::WideToNarrow
//
//  Like Convert, a failure returns something readable rather than nothing:
//  here that is the empty string, since wide bytes are not printable through
//  a narrow stream anyway.
//
////////////////////////////////////////////////////////////////////////////////

std::string TextEncoding::WideToNarrow (const std::wstring & text)
{
    unsigned     codePage      = GetNarrowCodePage();
    bool         canSubstitute = codePage != CP_UTF8 && codePage != CP_UTF7;
    std::string  narrow;
    int          count         = 0;



    if (text.empty())
    {
        return std::string();
    }

    count = WideCharToMultiByte (codePage, 0, text.c_str(), (int) text.size(),
                                 nullptr, 0,
                                 canSubstitute ? "?" : nullptr, nullptr);

    if (count <= 0)
    {
        return std::string();
    }

    narrow.resize ((size_t) count);

    count = WideCharToMultiByte (codePage, 0, text.c_str(), (int) text.size(),
                                 narrow.data(), count,
                                 canSubstitute ? "?" : nullptr, nullptr);

    return (count > 0) ? narrow : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextEncoding::NarrowToWide
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TextEncoding::NarrowToWide (const std::string & text)
{
    unsigned      codePage = GetNarrowCodePage();
    std::wstring  wide;
    int           count    = 0;



    if (text.empty())
    {
        return std::wstring();
    }

    count = MultiByteToWideChar (codePage, 0, text.c_str(), (int) text.size(),
                                 nullptr, 0);

    if (count <= 0)
    {
        return std::wstring();
    }

    wide.resize ((size_t) count);

    count = MultiByteToWideChar (codePage, 0, text.c_str(), (int) text.size(),
                                 wide.data(), count);

    return (count > 0) ? wide : std::wstring();
}
