#pragma once

#include "Pch.h"

#include "Seams/IDriveSpace.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeDriveSpace
//
//  IDriveSpace with results set by the test.
//
////////////////////////////////////////////////////////////////////////////////

class FakeDriveSpace : public IDriveSpace
{
public:
    bool      isRemovable = false;
    bool      hasRoom     = true;
    uint64_t  fileBytes   = 0;

    bool      IsOnRemovableDrive (const std::filesystem::path & path) const override                 { (void) path; return isRemovable; }
    bool      HasRoomFor         (const std::filesystem::path & folder, uint64_t bytes) const override { (void) folder; (void) bytes; return hasRoom; }
    uint64_t  GetFileSize        (const std::filesystem::path & path) const override                 { (void) path; return fileBytes; }
};
