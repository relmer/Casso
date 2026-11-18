#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/VolumeTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NativeListing
//
//  Apple Pascal and CP/M catalogs as the Apple displays them: the Filer's
//  Extended list and CP/M's STAT *.*. The preview pane and `disk list` both
//  print these lines.
//
////////////////////////////////////////////////////////////////////////////////

class NativeListing
{
public:
    static void  FormatPascal (const VolumeListing & listing, std::vector<std::string> & outLines);
    static void  FormatCpm    (const VolumeListing & listing, std::vector<std::string> & outLines);

    //  The word the Filer prints for a file kind.
    static const char *  GetPascalKindName (Byte kind);

private:
    static constexpr int  kDateWidth = 9;

    static std::string  FormatPascalDate (const FileEntry & entry);
    static std::string  MakeCpmSortKey   (const std::string & name);
    static std::string  GetCpmName       (const FileEntry & entry);
    static void         AppendCpmRows    (const std::vector<const FileEntry *> & files, std::vector<std::string> & inOutLines);
};
