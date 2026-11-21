#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveSpace
//
//  The facts about a host drive used to place a new copy of an image:
//  whether the drive is removable, whether a folder's drive has room,
//  and a file's size.
//
////////////////////////////////////////////////////////////////////////////////

class IDriveSpace
{
public:
    virtual ~IDriveSpace() = default;

    virtual bool      IsOnRemovableDrive (const std::filesystem::path & path) const = 0;
    virtual bool      HasRoomFor         (const std::filesystem::path & folder, uint64_t bytes) const = 0;
    virtual uint64_t  GetFileSize        (const std::filesystem::path & path) const = 0;
};
