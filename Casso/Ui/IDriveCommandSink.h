#pragma once

#include "Pch.h"

#include <string>





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveCommandSink
//
//  Interface implemented by `EmulatorShell` and consumed by the RmlUi
//  drive-widget custom element (P6-T3) and the IDropTarget (P6-T4).
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

    // Mount `path` into (slot, drive). Returns S_OK on success.
    // Caller is responsible for any UI notification on failure.
    virtual HRESULT Mount  (int slot, int drive, const std::wstring & path) = 0;

    // Eject the disk (if any) currently in (slot, drive).
    virtual void    Eject  (int slot, int drive) = 0;
};
