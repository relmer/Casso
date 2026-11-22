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

    //  False for a removable drive with no media in it, such as an empty card
    //  reader. Windows can go on listing a FAT volume's files from its cache
    //  after the card is taken out, so a drive root existing isn't enough.
    static bool  IsMediaPresent (const std::wstring & path);

    //  A folder that exists and is on a drive with its media in it.
    static bool  IsReadableDirectory (const std::wstring & path);

    //  The display name in a folder's desktop.ini, such as "Camera Roll", or
    //  empty for none.
    static std::wstring  GetLocalizedFolderName (const std::wstring & path);
    static std::wstring  GetPlainFolderName     (const std::wstring & path);
};
