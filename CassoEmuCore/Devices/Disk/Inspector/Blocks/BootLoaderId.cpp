#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/BootLoaderId.h"


static constexpr size_t  s_kBlockBytes       = 512;
static constexpr size_t  s_kIndexEntries     = 256;
static constexpr Byte    s_kSeedling         = 1;
static constexpr Byte    s_kSapling          = 2;
static constexpr Byte    s_kTree             = 3;
static constexpr int     s_kByteShift        = 8;
static constexpr Byte    s_kProDosStart[]    = { 0x01, 0x38, 0xB0, 0x03, 0x4C };
static constexpr Byte    s_kHfsSignature[]   = { 0x4C, 0x4B };
static constexpr Byte    s_kAsciiMask        = 0x7F;
static constexpr Byte    s_kFirstPrintable   = 0x20;
static constexpr Byte    s_kLastPrintable    = 0x7E;
static constexpr LPCWSTR s_kpszSosText       = L"SOS BOOT";
static constexpr LPCWSTR s_kpszPascalText    = L"MUST BOOT FROM SLOT";
static constexpr LPCWSTR s_kpszProDos8Text   = L"PRODOS 8 V";
static constexpr LPCWSTR s_kpszProDosText    = L"PRODOS";
static constexpr LPCWSTR s_kpszGsOsPath      = L"SYSTEM/GS.OS";





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::Identify
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BootLoaderId::Identify (const BlockImage & image, const BlockMap & map)
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

    text  = ReadText (image, s_kBlockBytes);
    sosAt = text.find (s_kpszSosText);

    if (isZero)
    {
        loader = L"None";
    }
    else if (isStart)
    {
        loader = IdentifyProDos (image, map);
    }
    else if (sosAt != std::wstring::npos)
    {
        version = GetWord (text, sosAt + wcslen (s_kpszSosText));
        loader  = version.empty() ? std::wstring (L"SOS loader") : L"SOS loader " + version;
    }
    else if (text.find (s_kpszPascalText) != std::wstring::npos)
    {
        loader = L"Apple Pascal loader";
    }
    else if (map.fileSystem == BlockFileSystem::Hfs && image[0] == s_kHfsSignature[0] && image[1] == s_kHfsSignature[1])
    {
        loader = L"Macintosh boot blocks";
    }

    return loader;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::IdentifyProDos
//
//  The PRODOS file in the volume directory: ProDOS 8 by its text, with the
//  version after "ProDOS 8 v" where it has one, and otherwise GS/OS where
//  System/GS.OS is on the volume.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BootLoaderId::IdentifyProDos (const BlockImage & image, const BlockMap & map)
{
    const BlockFileEntry  * prodos  = nullptr;
    std::wstring            loader  = L"ProDOS loader";
    std::wstring            text;
    size_t                  at      = std::wstring::npos;
    bool                    isGsOs  = false;
    size_t                  i       = 0;



    for (i = 0; i < map.entries.size() && prodos == nullptr; i++)
    {
        const BlockFileEntry  & entry = map.entries[i];

        if (entry.parent == -1 && !entry.isDirectory && ToUpper (entry.name) == s_kpszProDosText)
        {
            prodos = &entry;
        }
    }

    for (i = 0; i < map.files.size() && !isGsOs; i++)
    {
        isGsOs = ToUpper (map.files[i]) == s_kpszGsOsPath;
    }

    if (prodos == nullptr)
    {
        return L"No PRODOS file";
    }

    text = ToUpper (ToText (ReadFile (image, *prodos)));
    at   = text.find (s_kpszProDos8Text);

    if (at != std::wstring::npos)
    {
        loader = L"ProDOS 8 v" + GetWord (text, at + wcslen (s_kpszProDos8Text));
    }
    else if (isGsOs)
    {
        loader = L"GS/OS";
    }
    else if (text.find (s_kpszProDosText) != std::wstring::npos)
    {
        loader = L"ProDOS 8";
    }

    return loader;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::ReadFile
//
//  A seedling, sapling or tree file's first kMaxReadBytes, or less where
//  its EOF is; a block outside the volume ends it.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> BootLoaderId::ReadFile (const BlockImage & image, const BlockFileEntry & entry)
{
    size_t            length = std::min ((size_t) entry.eof, kMaxReadBytes);
    size_t            blocks = (length + s_kBlockBytes - 1) / s_kBlockBytes;
    size_t            count  = image.size() / s_kBlockBytes;
    vector<Byte>      bytes;
    vector<uint32_t>  data;
    size_t            i      = 0;
    size_t            b      = 0;
    uint32_t          index  = 0;



    if (entry.storage == s_kSeedling)
    {
        data.push_back (entry.keyBlock);
    }
    else if (entry.storage == s_kSapling && entry.keyBlock < count)
    {
        for (i = 0; i < blocks && i < s_kIndexEntries; i++)
        {
            data.push_back (GetIndex (image, entry.keyBlock, i));
        }
    }
    else if (entry.storage == s_kTree && entry.keyBlock < count)
    {
        index = GetIndex (image, entry.keyBlock, 0);

        for (i = 0; i < blocks && i < s_kIndexEntries && index < count; i++)
        {
            data.push_back (GetIndex (image, index, i));
        }
    }

    for (i = 0; i < data.size() && data[i] < count; i++)
    {
        for (b = 0; b < s_kBlockBytes && bytes.size() < length; b++)
        {
            bytes.push_back (image[(size_t) data[i] * s_kBlockBytes + b]);
        }
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::GetIndex
//
//  An index block's i-th block number: its low byte in the first half and
//  its high byte in the second.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BootLoaderId::GetIndex (const BlockImage & image, uint32_t indexBlock, size_t i)
{
    size_t  at = (size_t) indexBlock * s_kBlockBytes + i;



    return image[at] | ((uint32_t) image[at + s_kIndexEntries] << s_kByteShift);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::ReadText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BootLoaderId::ReadText (const BlockImage & image, size_t length)
{
    vector<Byte>  bytes (length);
    size_t        i     = 0;



    for (i = 0; i < length; i++)
    {
        bytes[i] = image[i];
    }

    return ToText (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::ToText
//
//  Each byte's high bit cleared, as Apple II text may have it set, and every
//  byte that isn't printable a space.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BootLoaderId::ToText (const vector<Byte> & bytes)
{
    std::wstring  text (bytes.size(), L' ');
    Byte          ch   = 0;
    size_t        i    = 0;



    for (i = 0; i < bytes.size(); i++)
    {
        ch = bytes[i] & s_kAsciiMask;

        if (ch >= s_kFirstPrintable && ch <= s_kLastPrintable)
        {
            text[i] = (wchar_t) ch;
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BootLoaderId::ToUpper (std::wstring text)
{
    std::transform (text.begin(), text.end(), text.begin(), ::towupper);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId::GetWord
//
//  The word after `after`, such as the SOS loader's 1.1 after "SOS BOOT".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BootLoaderId::GetWord (const std::wstring & text, size_t after)
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
