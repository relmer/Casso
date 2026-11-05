#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IMachineControl
//
//  The emulator's side of RESET, PASTE, DISK INSERT and EJECT, SCREEN,
//  SCREENSHOT and EXIT: each does what the menu command of the same purpose
//  does, through the same path, so a reset or a disk insert is in history as
//  the menu's is. CPU thread. A batch run doesn't have one, and the commands
//  are not available there.
//
////////////////////////////////////////////////////////////////////////////////

class IMachineControl
{
public:
    virtual ~IMachineControl() = default;

    //  Ctrl+Reset, or a power cycle.
    virtual void     Reset          (bool isPowerCycle) = 0;

    //  Drive 0 or 1. The insert is reported by the emulator's notice when
    //  the image can't be read, as the Disk menu's is.
    virtual void     InsertDisk     (int drive, const std::wstring & path) = 0;
    virtual void     EjectDisk      (int drive) = 0;

    //  Typed as Edit > Paste types the clipboard's text.
    virtual void     PasteText      (const std::wstring & text) = 0;

    //  The text screen as displayed: 40 or 80 columns, one string a row.
    virtual bool     GetScreenText  (int & columns, std::vector<std::string> & rows) = 0;

    //  The machine's picture saved as a PNG file.
    virtual HRESULT  SaveScreenshot (const std::wstring & path, int & width, int & height) = 0;

    //  Closes Casso as File > Exit does, after the current reply is sent.
    virtual void     Exit           () = 0;
};
