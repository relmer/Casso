#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderNames
//
//  The PC's drive roots, and the display name in a folder's desktop.ini,
//  for the folder tree.
//
////////////////////////////////////////////////////////////////////////////////

class HostFolderNames
{
public:
    //  The host's drive roots, such as C:\, in drive-letter order.
    static std::vector<std::wstring>  GetDriveRoots();

    //  The display name in a folder's desktop.ini, such as "Camera Roll", or
    //  empty for none.
    static std::wstring  GetLocalizedFolderName (const std::wstring & path);
    static std::wstring  GetPlainFolderName     (const std::wstring & path);
};
