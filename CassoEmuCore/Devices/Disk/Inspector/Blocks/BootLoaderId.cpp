#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/BootLoaderId.h"


static constexpr size_t  s_kBlockBytes       = 512;
static constexpr Byte    s_kProDosStart[]    = { 0x01, 0x38, 0xB0, 0x03, 0x4C };
static constexpr Byte    s_kHfsSignature[]   = { 0x4C, 0x4B };
static constexpr Byte    s_kAsciiMask        = 0x7F;
static constexpr Byte    s_kFirstPrintable   = 0x20;
static constexpr Byte    s_kLastPrintable    = 0x7E;
static constexpr LPCWSTR s_kpszSosText       = L"SOS BOOT";
static constexpr LPCWSTR s_kpszPascalText    = L"MUST BOOT FROM SLOT";





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::Identify
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BootLoaderId::Identify (const BlockImage & image, BlockFileSystem fileSystem)
{
    std::wstring  loader  = L"Unknown loader";
    std::wstring  text;
    std::wstring  version;
    size_t        sosAt   = std::wstring::npos;
    bool          isZero  = true;
    bool          isStart = true;
    size_t        i       = 0;



    if (image.size() < s_kBlockBytes)
    {
        return std::wstring();
    }

    for (i = 0; i < s_kBlockBytes && isZero; i++)
    {
        isZero = image[i] == 0;
    }

    for (i = 0; i < std::size (s_kProDosStart) && isStart; i++)
    {
        isStart = image[i] == s_kProDosStart[i];
    }

    text  = ReadText (image);
    sosAt = text.find (s_kpszSosText);

    if (isZero)
    {
        loader = L"None";
    }
    else if (isStart)
    {
        loader = L"ProDOS loader";
    }
    else if (sosAt != std::wstring::npos)
    {
        version = GetVersion (text, sosAt + wcslen (s_kpszSosText));
        loader  = version.empty() ? std::wstring (L"SOS loader") : L"SOS loader " + version;
    }
    else if (text.find (s_kpszPascalText) != std::wstring::npos)
    {
        loader = L"Apple Pascal loader";
    }
    else if (fileSystem == BlockFileSystem::Hfs && image[0] == s_kHfsSignature[0] && image[1] == s_kHfsSignature[1])
    {
        loader = L"Macintosh boot blocks";
    }

    return loader;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::ReadText
//
//  The first block as text, each byte's high bit cleared, as Apple II text
//  may have it set, and every other byte a space.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BootLoaderId::ReadText (const BlockImage & image)
{
    std::wstring  text (s_kBlockBytes, L' ');
    Byte          ch   = 0;
    size_t        i    = 0;



    for (i = 0; i < s_kBlockBytes; i++)
    {
        ch = image[i] & s_kAsciiMask;

        if (ch >= s_kFirstPrintable && ch <= s_kLastPrintable)
        {
            text[i] = (wchar_t) ch;
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::GetVersion
//
//  The word after the SOS loader's "SOS BOOT", such as 1.1.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BootLoaderId::GetVersion (const std::wstring & text, size_t after)
{
    size_t  start = text.find_first_not_of (L' ', after);
    size_t  end   = std::wstring::npos;



    if (start == std::wstring::npos)
    {
        return std::wstring();
    }

    end = text.find (L' ', start);

    return text.substr (start, end == std::wstring::npos ? std::wstring::npos : end - start);
}
