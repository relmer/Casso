#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/VolumeTypes.h"


enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  ParsedHostName
//
//  What a host file name says about the catalog entry it came from, when it
//  says anything. A bare name says nothing beyond the name, and the caller
//  falls back to the content rule.
//
//  The type is carried as a ProDOS type byte whatever the file name wrote,
//  because ProDOS has a byte for every DOS 3.3 letter and a caller placing the
//  file on a DOS 3.3 volume maps it back through the same table.
//
////////////////////////////////////////////////////////////////////////////////

struct ParsedHostName
{
    enum class ConvertedKind { None, ApplesoftListing, IntegerListing, Text };

    std::string    catalogName;
    bool           hasType   = false;
    Byte           type      = 0;        // ProDOS type byte
    bool           hasAux    = false;
    Word           aux       = 0;
    ConvertedKind  converted = ConvertedKind::None;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNaming
//
//  The suffixes a catalog entry wears on the host, in both directions.
//
//  CATALOG NAMES CARRY NO EXTENSION, so a file copied out has to say what it
//  was or the way back loses its type and address. The descriptive form
//  writes the answer in words a person reads; the CiderPress form writes the
//  six hex digits that tool established. Reading accepts both, and a bare
//  name, whatever the setting.
//
//  Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class HostFileNaming
{
public:
    enum class Style { Descriptive, CiderPress };

    static std::wstring  ForConverted (const std::string             & catalogName,
                                       ParsedHostName::ConvertedKind   kind);

    //  A raw copy. The type is the volume's own byte; the address is given
    //  only when the entry records one, since a DOS 3.3 catalog does not.
    static std::wstring  ForRaw (const std::string  & catalogName,
                                 VolumeKind           kind,
                                 Byte                 type,
                                 bool                 hasAddress,
                                 Word                 address,
                                 Style                style);

    //  False when no suffix was recognized; `catalogName` is still set, to
    //  the name minus any trailing extension.
    static bool  Parse (const std::wstring & hostName, ParsedHostName & out);

    //  The name with every character a host file name cannot hold replaced
    //  by an underscore. `substituted` reports whether anything changed, so
    //  the caller can tell the user the resulting name.
    static std::wstring  MakeHostLegal (const std::string & catalogName, bool & substituted);

    //  The DOS 3.3 type letter for a type byte, and the ProDOS type the
    //  CiderPress form writes for a DOS 3.3 type. Both directions live here
    //  so the two forms cannot disagree about a letter.
    static char  GetDos33TypeLetter    (Byte dosType);
    static bool  TryGetDos33TypeByte   (char letter, Byte & outDosType);
    static Byte  MapDos33ToProDosType  (Byte dosType);
    static bool  TryMapProDosToDos33   (Byte proDosType, Byte & outDosType);

    static constexpr const wchar_t *  kApplesoftSuffix = L".Applesoft BASIC.txt";
    static constexpr const wchar_t *  kIntegerSuffix   = L".Integer BASIC.txt";
    static constexpr const wchar_t *  kTextSuffix      = L".Text.txt";
    static constexpr const wchar_t *  kBinaryWord      = L"Binary";
    static constexpr const wchar_t *  kProDosWord      = L"ProDOS";
    static constexpr const wchar_t *  kDosWord         = L"DOS";
    static constexpr const wchar_t *  kRawExtension    = L".bin";
    static constexpr wchar_t          kCiderPressMark  = L'#';

private:
    //  One DOS 3.3 type, its catalog letter, and the ProDOS type the
    //  CiderPress form writes for it. The two "new" bits DOS sets on a
    //  re-saved program keep the letter of the type they modify, as the
    //  guest's own CATALOG shows them.
    struct Dos33TypeRow
    {
        Byte  dosType;
        char  letter;
        Byte  proDosType;
    };

    static constexpr Dos33TypeRow  kDos33Types[] =
    {
        { 0x00, 'T', 0x04 },
        { 0x01, 'I', 0xFA },
        { 0x02, 'A', 0xFC },
        { 0x04, 'B', 0x06 },
        { 0x08, 'S', 0xF2 },
        { 0x10, 'R', 0xFE },
        { 0x20, 'A', 0xFC },
        { 0x40, 'B', 0x06 },
    };

    static std::wstring  FormatHex (unsigned value, int digits);
    static bool          TryParseHex (const std::wstring & text, size_t at, size_t digits, unsigned & outValue);
    static bool          EndsWithIgnoringCase (const std::wstring & text, const std::wstring & suffix);
    static std::vector<std::wstring>  SplitOnDots (const std::wstring & text);
    static std::string   ToNarrow (const std::wstring & text);
    static std::wstring  ToWide   (const std::string & text);
    static bool          EqualsIgnoringCase (const std::wstring & a, const wchar_t * b);
};
