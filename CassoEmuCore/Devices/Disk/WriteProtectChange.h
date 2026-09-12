#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WriteProtectChange
//
//  What the Disk menu's write-protect command changes on a mounted image, and
//  the words the user is shown about it.
//
//  TWO MECHANISMS, ONE COMMAND. A WOZ stores its own write-protect flag; every
//  other format has none, so the host file's read-only attribute stands in.
//  A WOZ can carry both at once, and write-enabling it clears both, since
//  leaving either one set leaves the disk protected.
//
//  The decision and the text live here rather than in the shell so every
//  branch is reachable by a test. The shell carries out the plan it is handed
//  and shows the strings it is given.
//
////////////////////////////////////////////////////////////////////////////////

struct WriteProtectChange
{
    bool     protecting        = false;
    bool     changesImageFlag  = false;
    bool     changesAttribute  = false;
    wstring  fileName;

    //  Protecting when neither mechanism is set, write-enabling otherwise.
    static WriteProtectChange  MakePlan         (bool isWoz, bool imageFlag, bool readOnlyAttribute, const wstring & fileName);

    //  Whether the command would write-enable rather than write-protect.
    static bool                IsImageProtected (const WriteProtectInfo & wp);

    //  "Write-protect "x.dsk"" or "Write-enable "x.dsk"", with a long file
    //  name shortened in the middle so the row fits the dropdown.
    static wstring             GetMenuLabel     (bool isProtected, const wstring & fileName);

    //  The notice text for a change that was carried out.
    static wstring             DescribeResult   (const WriteProtectChange & change);
};
