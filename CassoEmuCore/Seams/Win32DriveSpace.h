#pragma once

#include "Pch.h"

#include "Seams/IDriveSpace.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DriveSpace
//
//  IDriveSpace on the host's drives.
//
////////////////////////////////////////////////////////////////////////////////

class Win32DriveSpace : public IDriveSpace
{
public:
    static constexpr uint64_t  kRoomMarginBytes = 1024 * 1024;

    bool      IsOnRemovableDrive (const std::filesystem::path & path) const override;
    bool      HasRoomFor         (const std::filesystem::path & folder, uint64_t bytes) const override;
    uint64_t  GetFileSize        (const std::filesystem::path & path) const override;
};
