#pragma once

#include "Pch.h"

#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/ExternalChangePolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreInterfaces
//
//  The calls store components make to DiskImageStore and to each other, one
//  interface per group of callers.
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



//  Marks a change to a mounted image's file as pending. Callable from any
//  thread.
class IImageChangeNotes
{
public:
    virtual ~IImageChangeNotes() = default;

    virtual void  NoteExternalChange (const string & path, ExternalChangeIntent intent,
                                      bool fromCassoExplorer = false) = 0;
};



//  Applies a ChangeAction to a bay.
class IChangeActions
{
public:
    virtual ~IChangeActions() = default;

    virtual void  CarryOutChangeAction (int slot, int drive, ChangeAction action,
                                        const vector<Byte> & bytes, ChangeAuthor author) = 0;
};
