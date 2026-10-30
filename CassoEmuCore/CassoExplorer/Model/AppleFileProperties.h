#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/VolumeTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties
//
//  The Apple file properties dialog's rules, apart from the dialog so they
//  are testable without a window: the type choices for each file system,
//  which number goes in the second field for a type, and hex, date and name
//  checks.
//
//  A file on the PC uses ProDOS's vocabulary: a CiderPress or AppleSingle
//  name stores a ProDOS type byte, and a DOS 3.3 type maps to one.
//
////////////////////////////////////////////////////////////////////////////////

class AppleFileProperties
{
public:
    struct TypeChoice
    {
        Byte          type = 0;
        std::wstring  label;   // letter or code, hex byte and name, tab-separated for a list's columns
    };

    //  What goes in the field after the type, for a type.
    enum class AuxKind { None, LoadAddress, RecordLength, AuxType };

    //  The DOS 3.3 list has T, I, A, B, S and R, plus the 20 or 40 type when
    //  `current` is one of them; the ProDOS list has the types in kProDosTypes.
    static std::vector<TypeChoice>  GetTypeChoices (VolumeKind kind, Byte current);

    //  The index of the choice for `type`, or -1 for a ProDOS type not in the list.
    static int                      FindChoice     (const std::vector<TypeChoice> & choices, Byte type);

    static AuxKind                  GetAuxKind     (VolumeKind kind, Byte type);
    static std::wstring             GetAuxLabel    (AuxKind kind);

    //  Hex digits only, no $, up to `digits` of them.
    static bool                     TryParseHex    (const std::wstring & text, size_t digits, unsigned & outValue);
    static std::wstring             FormatHex      (unsigned value, size_t digits);

    //  A ProDOS date and time in the Windows short date and time format;
    //  empty for a date of zero, which in ProDOS is an unset date.
    static std::wstring             FormatDate     (Word date, Word time);

    //  The reverse, in any form the Windows date parser reads. Empty text is
    //  no date. False for a date ProDOS can't store: before 1940 or after
    //  2039, since the year is two digits.
    static bool                     TryParseDate   (const std::wstring & text, Word & outDate, Word & outTime);

    //  A name the file system can store; for a file on the PC, DOS 3.3's
    //  rule, the looser of the two.
    static bool                     IsValidName    (VolumeKind kind, const std::wstring & name);

    //  What the dialog's fields changed, each only where its edited flag is
    //  set. Access is ProDOS's bits, the changed ones in accessMask; on DOS 3.3
    //  the Locked box is the lock.
    struct Edits
    {
        std::wstring  name;
        Byte          type           = 0;
        Word          aux            = 0;
        Byte          access         = 0;
        Byte          accessMask     = 0;
        bool          locked         = false;
        Word          createdDate    = 0;
        Word          createdTime    = 0;
        Word          modifiedDate   = 0;
        Word          modifiedTime   = 0;
        bool          nameEdited     = false;
        bool          typeEdited     = false;
        bool          auxEdited      = false;
        bool          lockedEdited   = false;
        bool          createdEdited  = false;
        bool          modifiedEdited = false;

        bool  ChangesAttributes () const { return typeEdited || auxEdited || accessMask != 0 || lockedEdited || createdEdited || modifiedEdited; }
    };

    //  The catalog changes for one file: its access with only the changed bits
    //  replaced, and the second field only where the file's type, after the
    //  edit, has one.
    static FileAttributes  MakeAttributes (const FileEntry & entry, VolumeKind kind, const Edits & edits);

    //  An entry's ProDOS access byte; a DOS 3.3 entry's from its lock.
    static Byte            GetAccess      (const FileEntry & entry);

    //  ProDOS's access bits, in the order the dialog lists them.
    static constexpr Byte  kAccessRead    = 0x01;
    static constexpr Byte  kAccessWrite   = 0x02;
    static constexpr Byte  kAccessBackup  = 0x20;
    static constexpr Byte  kAccessRename  = 0x40;
    static constexpr Byte  kAccessDestroy = 0x80;

    static constexpr int   kFirstYear = 1940;
    static constexpr int   kLastYear  = 2039;
    static constexpr int   kPivot     = 40;     // two-digit years from 40 are 19xx

private:
    struct NamedType
    {
        Byte             type;
        const wchar_t  * code;
        const wchar_t  * name;
    };

    static constexpr NamedType  kDos33Types[] =
    {
        { 0x00, L"T", L"Text"           },
        { 0x01, L"I", L"Integer BASIC"  },
        { 0x02, L"A", L"Applesoft BASIC" },
        { 0x04, L"B", L"Binary"         },
        { 0x08, L"S", L"Special"        },
        { 0x10, L"R", L"Relocatable"    },
    };

    static constexpr NamedType  kDos33SpareTypes[] =
    {
        { 0x20, L"A", L"Applesoft (second A)" },
        { 0x40, L"B", L"Binary (second B)"    },
    };

    static constexpr NamedType  kProDosTypes[] =
    {
        { 0x00, L"NON", L"Typeless"                },
        { 0x04, L"TXT", L"Text"                    },
        { 0x06, L"BIN", L"Binary"                  },
        { 0x19, L"ADB", L"AppleWorks database"     },
        { 0x1A, L"AWP", L"AppleWorks word processor" },
        { 0x1B, L"ASP", L"AppleWorks spreadsheet"  },
        { 0xEF, L"PAS", L"Pascal"                  },
        { 0xF0, L"CMD", L"Command"                 },
        { 0xFA, L"INT", L"Integer BASIC"           },
        { 0xFB, L"IVR", L"Integer BASIC variables" },
        { 0xFC, L"BAS", L"Applesoft BASIC"         },
        { 0xFD, L"VAR", L"Applesoft variables"     },
        { 0xFE, L"REL", L"Relocatable"             },
        { 0xFF, L"SYS", L"System"                  },
    };

    static TypeChoice  MakeChoice (const NamedType & row);
};
