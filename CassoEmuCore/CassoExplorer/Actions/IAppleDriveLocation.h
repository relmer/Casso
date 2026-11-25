#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IAppleDriveLocation
//
//  The linked Apple drive the browser's list is on, for the actions that work
//  on the drive that is open.
//
////////////////////////////////////////////////////////////////////////////////

class IAppleDriveLocation
{
public:
    virtual ~IAppleDriveLocation() = default;

    //  The list's Apple drive: its link, drive and directory. False when the
    //  list is not on one.
    virtual bool  TryGetAppleDrive (std::string & outLink, int & outDrive, std::string & outDirectory) const = 0;
};