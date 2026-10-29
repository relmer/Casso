#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LongPath
//
//  A path in the form Windows takes at any length, for the code that passes
//  one to Windows. Everywhere else a path stays as the user gave it: in the
//  store, the prefs, the recent-disks list and on screen.
//
//  The \\?\ form has no MAX_PATH limit, whatever the LongPathsEnabled
//  setting, but Windows does not normalize it: the path must be absolute,
//  with backslashes, and with no . or .. segments. ToWin32 makes it so.
//
////////////////////////////////////////////////////////////////////////////////

class LongPath
{
public:
    //  The path absolute and normalized, with \\?\ before it, or \\?\UNC\ in
    //  place of a share's leading \\. An empty path, and one in a device form
    //  already (\\?\ or \\.\), are returned as given.
    static std::wstring  ToWin32          (const std::wstring & path);
    static std::wstring  ToWin32          (const std::string  & path);

    //  The path without the prefix ToWin32 adds, for one Windows returned.
    static std::wstring  FromWin32        (const std::wstring & path);

    //  The path with each 8.3 short name replaced by the long name, for one the
    //  shell returned in the short form to fit MAX_PATH. The last segment of a
    //  file not yet created is returned as given.
    static std::wstring  ExpandShortNames (const std::wstring & path);
};
