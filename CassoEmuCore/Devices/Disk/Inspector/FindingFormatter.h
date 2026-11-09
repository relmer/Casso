#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/Finding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindingFormatter
//
//  A finding's text for the Findings tab, its category's name, and a damage
//  reason's text. Findings about what a track records say what was found,
//  never "damaged" or "protected"; findings about the image file call it
//  damaged only for an unreadable record, a map entry past the records, or a
//  checksum that does not match.
//
////////////////////////////////////////////////////////////////////////////////

class FindingFormatter
{
public:
    static std::wstring  Format             (const Finding & finding);
    static std::wstring  FormatCategory     (FindingCategory category);
    static std::wstring  FormatDamageReason (DamageReason reason);

private:
    static std::wstring  FormatFieldFinding (const Finding & finding);
    static std::wstring  FormatTrackFinding (const Finding & finding);
    static std::wstring  FormatImageFinding (const Finding & finding);
};
