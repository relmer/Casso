#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveCommandSink
//
//  Interface implemented by `EmulatorShell` and consumed by the
//  Drive-widget chrome and the IDropTarget.
//  Decoupling the widget from the shell lets the widget unit-test path
//  inject a mock sink without dragging in the entire emulator window
//  graph (used in AutoMountTests.cpp).
//
//  Slot/drive convention matches the rest of the codebase: slot 6 +
//  drive 0/1 for the integrated Disk II. The sink ignores slots it
//  doesn't host.
//
////////////////////////////////////////////////////////////////////////////////

class IDriveCommandSink
{
public:
    virtual ~IDriveCommandSink() = default;

    // Mount `path` into (slot, drive).
    virtual HRESULT Mount (int slot, int drive, const std::wstring & path) = 0;

    // Eject the disk currently in (slot, drive).
    virtual void    Eject (int slot, int drive) = 0;
};




////////////////////////////////////////////////////////////////////////////////
//
//  NullDriveCommandSink
//
//  Does nothing, successfully. Lets DriveWidgets be laid out and painted
//  without wiring them to a running machine -- the settings theme preview
//  paints live widgets purely for appearance, and never mounts anything.
//
//  A null implementation belongs with the interface rather than with any
//  one consumer: it is a property of the contract, and header-only because
//  there is no behavior to define out of line.
//
////////////////////////////////////////////////////////////////////////////////

class NullDriveCommandSink : public IDriveCommandSink
{
public:
    HRESULT Mount (int /*slot*/, int /*drive*/, const std::wstring & /*path*/) override { return S_OK; }
    void    Eject (int /*slot*/, int /*drive*/)                                override { }
};
