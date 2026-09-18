#include "Pch.h"

#include "Cassque/Model/HostFileNaming.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"



//  The characters a Windows file name cannot hold.
static constexpr const char *  s_kpszHostIllegal = "<>:\"/\\|?*";





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::GetDos33TypeLetter
//
////////////////////////////////////////////////////////////////////////////////

char HostFileNaming::GetDos33TypeLetter (Byte dosType)
{
    for (const Dos33TypeRow & row : kDos33Types)
    {
        if (row.dosType == dosType)
        {
            return row.letter;
        }
    }

    return '?';
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::TryGetDos33TypeByte
//
////////////////////////////////////////////////////////////////////////////////

bool HostFileNaming::TryGetDos33TypeByte (char letter, Byte & outDosType)
{
    char  upper = (letter >= 'a' && letter <= 'z') ? (char) (letter - 'a' + 'A') : letter;



    outDosType = 0;

    for (const Dos33TypeRow & row : kDos33Types)
    {
        if (row.letter == upper)
        {
            outDosType = row.dosType;

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::MapDos33ToProDosType
//
////////////////////////////////////////////////////////////////////////////////

Byte HostFileNaming::MapDos33ToProDosType (Byte dosType)
{
    for (const Dos33TypeRow & row : kDos33Types)
    {
        if (row.dosType == dosType)
        {
            return row.proDosType;
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::TryMapProDosToDos33
//
////////////////////////////////////////////////////////////////////////////////

bool HostFileNaming::TryMapProDosToDos33 (Byte proDosType, Byte & outDosType)
{
    outDosType = 0;

    for (const Dos33TypeRow & row : kDos33Types)
    {
        if (row.proDosType == proDosType)
        {
            outDosType = row.dosType;

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::FormatHex
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostFileNaming::FormatHex (unsigned value, int digits)
{
    return std::format (L"{:0{}X}", value, digits);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::TryParseHex
//
////////////////////////////////////////////////////////////////////////////////

bool HostFileNaming::TryParseHex (const std::wstring & text, size_t at, size_t digits, unsigned & outValue)
{
    size_t  i = 0;



    outValue = 0;

    if (at + digits > text.size())
    {
        return false;
    }

    for (i = 0; i < digits; i++)
    {
        wchar_t   c     = text[at + i];
        unsigned  digit = 0;

        if (c >= L'0' && c <= L'9')      { digit = (unsigned) (c - L'0'); }
        else if (c >= L'A' && c <= L'F') { digit = (unsigned) (c - L'A' + 10); }
        else if (c >= L'a' && c <= L'f') { digit = (unsigned) (c - L'a' + 10); }
        else                             { return false; }

        outValue = (outValue << 4) | digit;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::EndsWithIgnoringCase
//
////////////////////////////////////////////////////////////////////////////////

bool HostFileNaming::EndsWithIgnoringCase (const std::wstring & text, const std::wstring & suffix)
{
    size_t  textSize   = text.size();
    size_t  suffixSize = suffix.size();



    if (suffixSize > textSize)
    {
        return false;
    }

    return _wcsnicmp (text.c_str() + (textSize - suffixSize), suffix.c_str(), suffixSize) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::EqualsIgnoringCase
//
////////////////////////////////////////////////////////////////////////////////

bool HostFileNaming::EqualsIgnoringCase (const std::wstring & a, const wchar_t * b)
{
    return _wcsicmp (a.c_str(), b) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::SplitOnDots
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> HostFileNaming::SplitOnDots (const std::wstring & text)
{
    std::vector<std::wstring>  parts;
    size_t                     start = 0;
    size_t                     dot   = 0;



    while ((dot = text.find (L'.', start)) != std::wstring::npos)
    {
        parts.push_back (text.substr (start, dot - start));
        start = dot + 1;
    }

    parts.push_back (text.substr (start));

    return parts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::ToNarrow
//
//  Catalog names are seven-bit, so the conversion is a truncation of code
//  units a caller has already kept in range.
//
////////////////////////////////////////////////////////////////////////////////

std::string HostFileNaming::ToNarrow (const std::wstring & text)
{
    std::string  narrow;



    for (wchar_t c : text)
    {
        narrow += (char) (c & 0x7F);
    }

    return narrow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::ToWide
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostFileNaming::ToWide (const std::string & text)
{
    std::wstring  wide;



    for (char c : text)
    {
        wide += (wchar_t) (Byte) c;
    }

    return wide;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::MakeHostLegal
//
//  A catalog name can hold anything printable, and a few of those characters
//  end a Windows path or open a redirect. Each becomes an underscore, and the
//  caller is told so the user sees the name the file actually got.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostFileNaming::MakeHostLegal (const std::string & catalogName, bool & substituted)
{
    std::wstring  legal;



    substituted = false;

    for (char c : catalogName)
    {
        bool  illegal = (Byte) c < 0x20 || strchr (s_kpszHostIllegal, c) != nullptr;

        if (illegal)
        {
            legal       += L'_';
            substituted  = true;
        }
        else
        {
            legal += (wchar_t) (Byte) c;
        }
    }

    return legal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::ForConverted
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostFileNaming::ForConverted (
    const std::string             & catalogName,
    ParsedHostName::ConvertedKind   kind)
{
    bool          substituted = false;
    std::wstring  name        = MakeHostLegal (catalogName, substituted);



    switch (kind)
    {
        case ParsedHostName::ConvertedKind::ApplesoftListing: return name + kApplesoftSuffix;
        case ParsedHostName::ConvertedKind::IntegerListing:   return name + kIntegerSuffix;
        case ParsedHostName::ConvertedKind::Text:             return name + kTextSuffix;
        default:                                              return name;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::ForRaw
//
//  The descriptive form says what the file is in words; the CiderPress form
//  is six hex digits after a hash, the type first. A DOS 3.3 type writes its
//  letter in the descriptive form and its ProDOS equivalent in the CiderPress
//  form, since that form has no room for a letter.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostFileNaming::ForRaw (
    const std::string  & catalogName,
    VolumeKind           kind,
    Byte                 type,
    bool                 hasAddress,
    Word                 address,
    Style                style)
{
    bool          substituted = false;
    bool          isDos       = kind == VolumeKind::Dos33;
    bool          isBinary    = isDos ? (type == Dos33Volume::kTypeBinary)
                                      : (type == ProDosVolume::kTypeBinary);
    Byte          proDosType  = isDos ? MapDos33ToProDosType (type) : type;
    Word          aux         = hasAddress ? address : 0;
    std::wstring  name        = MakeHostLegal (catalogName, substituted);



    if (style == Style::CiderPress)
    {
        return name + kCiderPressMark + FormatHex (proDosType, 2) + FormatHex (aux, 4);
    }

    if (isBinary)
    {
        return name + L"." + kBinaryWord + L".$" + FormatHex (aux, 4) + kRawExtension;
    }

    if (isDos)
    {
        return name + L"." + kDosWord + L"." + (wchar_t) GetDos33TypeLetter (type) + kRawExtension;
    }

    name += std::wstring (L".") + kProDosWord + L".$" + FormatHex (type, 2);

    if (aux != 0)
    {
        name += L".$" + FormatHex (aux, 4);
    }

    return name + kRawExtension;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming::Parse
//
//  Every form this class writes, plus the bare name. Matching is on the
//  suffix words, case-insensitively, and the catalog name is whatever came
//  before them.
//
////////////////////////////////////////////////////////////////////////////////

bool HostFileNaming::Parse (const std::wstring & hostName, ParsedHostName & out)
{
    std::vector<std::wstring>  parts;
    size_t                     count = 0;
    size_t                     hash  = 0;
    unsigned                   type  = 0;
    unsigned                   aux   = 0;
    std::wstring               stem;



    out = ParsedHostName();

    //  Converted listings: NAME.<words>.txt
    if (EndsWithIgnoringCase (hostName, kApplesoftSuffix))
    {
        out.catalogName = ToNarrow (hostName.substr (0, hostName.size() - wcslen (kApplesoftSuffix)));
        out.converted   = ParsedHostName::ConvertedKind::ApplesoftListing;

        return true;
    }

    if (EndsWithIgnoringCase (hostName, kIntegerSuffix))
    {
        out.catalogName = ToNarrow (hostName.substr (0, hostName.size() - wcslen (kIntegerSuffix)));
        out.converted   = ParsedHostName::ConvertedKind::IntegerListing;

        return true;
    }

    if (EndsWithIgnoringCase (hostName, kTextSuffix))
    {
        out.catalogName = ToNarrow (hostName.substr (0, hostName.size() - wcslen (kTextSuffix)));
        out.converted   = ParsedHostName::ConvertedKind::Text;

        return true;
    }

    //  CiderPress: NAME#TTAAAA, exactly six hex digits after the hash.
    hash = hostName.rfind (kCiderPressMark);

    if (hash != std::wstring::npos && hash > 0 && hostName.size() == hash + 7
     && TryParseHex (hostName, hash + 1, 2, type)
     && TryParseHex (hostName, hash + 3, 4, aux))
    {
        out.catalogName = ToNarrow (hostName.substr (0, hash));
        out.hasType     = true;
        out.type        = (Byte) type;
        out.hasAux      = true;
        out.aux         = (Word) aux;

        return true;
    }

    //  Descriptive raw forms, all ending in .bin.
    if (EndsWithIgnoringCase (hostName, kRawExtension))
    {
        stem  = hostName.substr (0, hostName.size() - wcslen (kRawExtension));
        parts = SplitOnDots (stem);
        count = parts.size();

        //  NAME.Binary.$AAAA
        if (count >= 3 && EqualsIgnoringCase (parts[count - 2], kBinaryWord)
         && parts[count - 1].size() == 5 && parts[count - 1][0] == L'$'
         && TryParseHex (parts[count - 1], 1, 4, aux))
        {
            out.catalogName = ToNarrow (stem.substr (0, stem.size() - parts[count - 1].size() - parts[count - 2].size() - 2));
            out.hasType     = true;
            out.type        = ProDosVolume::kTypeBinary;
            out.hasAux      = true;
            out.aux         = (Word) aux;

            return true;
        }

        //  NAME.ProDOS.$TT.$AAAA
        if (count >= 4 && EqualsIgnoringCase (parts[count - 3], kProDosWord)
         && parts[count - 2].size() == 3 && parts[count - 2][0] == L'$'
         && parts[count - 1].size() == 5 && parts[count - 1][0] == L'$'
         && TryParseHex (parts[count - 2], 1, 2, type)
         && TryParseHex (parts[count - 1], 1, 4, aux))
        {
            out.catalogName = ToNarrow (stem.substr (0, stem.size() - parts[count - 1].size()
                                                          - parts[count - 2].size() - parts[count - 3].size() - 3));
            out.hasType     = true;
            out.type        = (Byte) type;
            out.hasAux      = true;
            out.aux         = (Word) aux;

            return true;
        }

        //  NAME.ProDOS.$TT
        if (count >= 3 && EqualsIgnoringCase (parts[count - 2], kProDosWord)
         && parts[count - 1].size() == 3 && parts[count - 1][0] == L'$'
         && TryParseHex (parts[count - 1], 1, 2, type))
        {
            out.catalogName = ToNarrow (stem.substr (0, stem.size() - parts[count - 1].size() - parts[count - 2].size() - 2));
            out.hasType     = true;
            out.type        = (Byte) type;
            out.hasAux      = true;
            out.aux         = 0;

            return true;
        }

        //  NAME.DOS.X
        if (count >= 3 && EqualsIgnoringCase (parts[count - 2], kDosWord)
         && parts[count - 1].size() == 1)
        {
            Byte  dosType = 0;
            bool  known   = TryGetDos33TypeByte ((char) parts[count - 1][0], dosType);

            if (known)
            {
                out.catalogName = ToNarrow (stem.substr (0, stem.size() - 1 - parts[count - 2].size() - 2));
                out.hasType     = true;
                out.type        = MapDos33ToProDosType (dosType);
                out.hasAux      = false;

                return true;
            }
        }
    }

    //  A bare name: the catalog name is everything before the last dot, when
    //  there is one that is not the first character.
    {
        size_t  dot = hostName.rfind (L'.');

        out.catalogName = ToNarrow ((dot != std::wstring::npos && dot > 0) ? hostName.substr (0, dot) : hostName);
    }

    return false;
}
