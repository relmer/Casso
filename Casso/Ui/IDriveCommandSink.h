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
