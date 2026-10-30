#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/AppleFileProperties.h"
#include "Core/AppleSingleCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostAppleFile
//
//  An Apple file on the PC, as the Apple file properties dialog edits it: one
//  whose name is in the Descriptive or CiderPress form, with the type and
//  address in it, or an AppleSingle container, which also has the access and
//  dates. Its properties are given as a ProDOS catalog entry, since a file
//  on the PC uses ProDOS's vocabulary. Pure: the bytes are read and written by
//  the caller.
//
////////////////////////////////////////////////////////////////////////////////

class HostAppleFile
{
public:
    enum class Form { None, Descriptive, CiderPress, AppleSingle };

    //  What the edits make of the file: its new name on the PC, which is its
    //  name when it doesn't change, and for an AppleSingle file, its new bytes.
    struct Change
    {
        std::wstring       leafName;
        std::vector<Byte>  bytes;
    };

    //  The file's form and its entry, from its name and, for an AppleSingle
    //  file, its bytes; Form::None for a file that is neither.
    static HostAppleFile  Read (const std::wstring & leafName, const std::vector<Byte> & bytes);

    //  For an AppleSingle file, the new name renames the file on the PC too
    //  when `renameHostFile` is set. Fails with ERROR_NOT_SUPPORTED for an
    //  AppleSingle file with parts a rewrite would drop, such as a resource fork.
    HRESULT  Apply (const AppleFileProperties::Edits & edits, bool renameHostFile, Change & outChange) const;

    //  The name an AppleSingle file is given on the PC for an Apple name.
    static std::wstring  MakeAppleSingleName (const std::string & appleName);

    //  AppleSingle's dates, seconds from 2000-01-01, and ProDOS's date and
    //  time words, both in the same clock with no time zone.
    static void     ToProDosDate   (int32_t seconds, Word & outDate, Word & outTime);
    static int32_t  FromProDosDate (Word date, Word time);

    Form             form = Form::None;
    std::wstring     leafName;
    FileEntry        entry;
    AppleSingleFile  single;
};
