#include "Pch.h"

#include "Seams/Win32DriveSpace.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DriveSpace::IsOnRemovableDrive
//
////////////////////////////////////////////////////////////////////////////////

bool Win32DriveSpace::IsOnRemovableDrive (const std::filesystem::path & path) const
{
    wchar_t  root[MAX_PATH] = {};



    return GetVolumePathNameW (path.c_str(), root, MAX_PATH) && GetDriveTypeW (root) == DRIVE_REMOVABLE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DriveSpace::HasRoomFor
//
//  Room for the copy and a margin. A drive whose free space can't be read
//  is taken to have room, and the write reports a failure.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32DriveSpace::HasRoomFor (const std::filesystem::path & folder, uint64_t bytes) const
{
    ULARGE_INTEGER  available = {};



    return !GetDiskFreeSpaceExW (folder.c_str(), &available, nullptr, nullptr) || available.QuadPart >= bytes + kRoomMarginBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DriveSpace::GetFileSize
//
//  The largest size for a file that can't be read, so no drive has room
//  for a copy of it.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Win32DriveSpace::GetFileSize (const std::filesystem::path & path) const
{
    std::error_code  error;



    return std::filesystem::file_size (path, error);
}
