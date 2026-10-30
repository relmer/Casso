#include "Pch.h"

#include "CassoExplorer/Model/AppleFileProperties.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::MakeChoice
//
////////////////////////////////////////////////////////////////////////////////

AppleFileProperties::TypeChoice AppleFileProperties::MakeChoice (const NamedType & row)
{
    return TypeChoice { row.type, std::format (L"{}\t{:02X}\t{}", row.code, row.type, row.name) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::GetTypeChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<AppleFileProperties::TypeChoice> AppleFileProperties::GetTypeChoices (VolumeKind kind, Byte current)
{
    std::vector<TypeChoice>  choices;



    if (kind == VolumeKind::Dos33)
    {
        for (const NamedType & row : kDos33Types)
        {
            choices.push_back (MakeChoice (row));
        }

        for (const NamedType & row : kDos33SpareTypes)
        {
            if (row.type == current)
            {
                choices.push_back (MakeChoice (row));
            }
        }
    }
    else
    {
        for (const NamedType & row : kProDosTypes)
        {
            choices.push_back (MakeChoice (row));
        }
    }

    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::FindChoice
//
////////////////////////////////////////////////////////////////////////////////

int AppleFileProperties::FindChoice (const std::vector<TypeChoice> & choices, Byte type)
{
    for (size_t i = 0; i < choices.size(); i++)
    {
        if (choices[i].type == type)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::GetAuxKind
//
//  DOS 3.3 stores a load address in a binary only. ProDOS has an aux type on
//  every file: the load address of a binary or system file, the record length
//  of a text file, and a number whose meaning depends on the type elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

AppleFileProperties::AuxKind AppleFileProperties::GetAuxKind (VolumeKind kind, Byte type)
{
    if (kind == VolumeKind::Dos33)
    {
        return (type == Dos33Volume::kTypeBinary) ? AuxKind::LoadAddress : AuxKind::None;
    }

    return (type == ProDosVolume::kTypeBinary || type == 0xFF) ? AuxKind::LoadAddress
         : (type == ProDosVolume::kTypeText)                   ? AuxKind::RecordLength
                                                               : AuxKind::AuxType;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::GetAuxLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleFileProperties::GetAuxLabel (AuxKind kind)
{
    return (kind == AuxKind::LoadAddress)  ? L"Load address:"
         : (kind == AuxKind::RecordLength) ? L"Record length:"
         : (kind == AuxKind::AuxType)      ? L"Aux type:"
                                           : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::TryParseHex
//
////////////////////////////////////////////////////////////////////////////////

bool AppleFileProperties::TryParseHex (const std::wstring & text, size_t digits, unsigned & outValue)
{
    unsigned  value = 0;



    if (text.empty() || text.size() > digits)
    {
        return false;
    }

    for (wchar_t c : text)
    {
        int  nibble = (c >= L'0' && c <= L'9') ? c - L'0'
                    : (c >= L'A' && c <= L'F') ? c - L'A' + 10
                    : (c >= L'a' && c <= L'f') ? c - L'a' + 10
                                               : -1;

        if (nibble < 0)
        {
            return false;
        }

        value = (value << 4) | (unsigned) nibble;
    }

    outValue = value;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::FormatHex
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleFileProperties::FormatHex (unsigned value, size_t digits)
{
    return std::format (L"{:0{}X}", value, digits);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::FormatDate
//
//  ProDOS's date word is the year in its top seven bits, then the month and
//  day; its time word is the hour in the high byte and the minute in the low.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleFileProperties::FormatDate (Word date, Word time)
{
    SYSTEMTIME  when      = {};
    int         year      = (date >> 9) & 0x7F;
    wchar_t     day[64]   = {};
    wchar_t     clock[64] = {};



    if (date == 0)
    {
        return std::wstring();
    }

    when.wYear   = (WORD) ((year < kPivot) ? 2000 + year : 1900 + year);
    when.wMonth  = (WORD) ((date >> 5) & 0x0F);
    when.wDay    = (WORD) (date & 0x1F);
    when.wHour   = (WORD) ((time >> 8) & 0x1F);
    when.wMinute = (WORD) (time & 0x3F);

    if (GetDateFormatEx (LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &when, nullptr, day, (int) std::size (day), nullptr) == 0
     || GetTimeFormatEx (LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &when, nullptr, clock, (int) std::size (clock)) == 0)
    {
        return std::wstring();
    }

    return std::format (L"{} {}", day, clock);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::TryParseDate
//
////////////////////////////////////////////////////////////////////////////////

bool AppleFileProperties::TryParseDate (const std::wstring & text, Word & outDate, Word & outTime)
{
    DATE        parsed = 0;
    SYSTEMTIME  when   = {};
    HRESULT     hr     = S_OK;
    bool        inside = false;



    if (text.empty())
    {
        outDate = 0;
        outTime = 0;
        return true;
    }

    hr = VarDateFromStr (text.c_str(), LOCALE_USER_DEFAULT, 0, &parsed);

    if (FAILED (hr) || !VariantTimeToSystemTime (parsed, &when))
    {
        return false;
    }

    inside = when.wYear >= kFirstYear && when.wYear <= kLastYear;

    if (!inside)
    {
        return false;
    }

    outDate = (Word) (((when.wYear % 100) << 9) | (when.wMonth << 5) | when.wDay);
    outTime = (Word) ((when.wHour << 8) | when.wMinute);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::IsValidName
//
////////////////////////////////////////////////////////////////////////////////

bool AppleFileProperties::IsValidName (VolumeKind kind, const std::wstring & name)
{
    std::string  narrow = TextEncoding::WideToNarrow (name);



    return (kind == VolumeKind::ProDos) ? ProDosVolume::IsValidName (narrow) : Dos33Volume::IsValidName (narrow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::GetAccess
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleFileProperties::GetAccess (const FileEntry & entry)
{
    if (entry.hasAccess)
    {
        return entry.access;
    }

    return entry.isLocked ? FilePayload::kAccessLocked : FilePayload::kAccessUnlocked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileProperties::MakeAttributes
//
////////////////////////////////////////////////////////////////////////////////

FileAttributes AppleFileProperties::MakeAttributes (const FileEntry & entry, VolumeKind kind, const Edits & edits)
{
    FileAttributes  attributes;
    Byte            type       = edits.typeEdited ? edits.type : entry.type;
    Byte            access     = GetAccess (entry);
    Byte            lockBits   = kAccessDestroy | kAccessRename | kAccessWrite;



    if (edits.typeEdited)
    {
        attributes.type    = edits.type;
        attributes.hasType = true;
    }

    if (edits.auxEdited && GetAuxKind (kind, type) != AuxKind::None)
    {
        attributes.auxType    = edits.aux;
        attributes.hasAuxType = true;
    }

    if (edits.accessMask != 0)
    {
        access = (Byte) ((access & ~edits.accessMask) | (edits.access & edits.accessMask));
    }

    if (edits.lockedEdited)
    {
        access = edits.locked ? (Byte) (access & ~lockBits) : (Byte) (access | lockBits | kAccessRead);
    }

    if (edits.accessMask != 0 || edits.lockedEdited)
    {
        attributes.access    = access;
        attributes.hasAccess = true;
    }

    if (edits.createdEdited)
    {
        attributes.createdDate = edits.createdDate;
        attributes.createdTime = edits.createdTime;
        attributes.hasCreated  = true;
    }

    if (edits.modifiedEdited)
    {
        attributes.modifiedDate = edits.modifiedDate;
        attributes.modifiedTime = edits.modifiedTime;
        attributes.hasModified  = true;
    }

    return attributes;
}
