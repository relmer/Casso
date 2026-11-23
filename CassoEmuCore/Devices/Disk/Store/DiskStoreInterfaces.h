#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreInterfaces
//
//  The calls store components make back into DiskImageStore, one interface
//  per group of callers.
//
////////////////////////////////////////////////////////////////////////////////

//  Moves a mounted bay onto a different file. The disk in the drive does not
//  change.
class IBayRepoint
{
public:
    virtual ~IBayRepoint() = default;

    virtual HRESULT  RepointBayToFile (int slot, int drive, const string & newPath) = 0;
};



//  Takes a disk out of a bay, or swaps the image in it for one loaded from
//  new bytes.
class IBayLifecycle
{
public:
    virtual ~IBayLifecycle() = default;

    virtual void     Eject                       (int slot, int drive)                            = 0;
    virtual void     EjectLostImage              (int slot, int drive)                            = 0;
    virtual HRESULT  MountExternallyModifiedDisk (int slot, int drive, const vector<Byte> & bytes) = 0;
};
