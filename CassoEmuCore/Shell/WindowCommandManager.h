#pragma once

#include "Pch.h"
#include "Shell/DiskMru.h"


class EmulatorShell;





// The handler WindowCommandManager::OnCommand calls for a command id. None
// means the id is dropped.
enum class WindowCommandRoute
{
    None,
    File,
    Edit,
    Machine,
    Disk,
    View,
    Printer,
    PrinterPreview,
    PrinterModernSent,
    PrinterModernFailed,
    Help,
    ExternalDrive,
    MouseConnect,
};





////////////////////////////////////////////////////////////////////////////////
//
//  WindowCommandManager
//
//  Owner of the Win32 command dispatch path: the public command pump
//  (HandleCommand), the WM_COMMAND id-range demux (OnCommand), every
//  per-menu-group OnFooCommand handler, the WM_INITMENUPOPUP menu
//  state-caching tick, and the file-open shell dialog for drive-
//  widget click-to-browse (PromptForDiskImage). Holds a back-reference
//  to EmulatorShell and is declared a friend of that class so it can
//  reach the shell members the command handlers operate on. No new
//  global state is added; the back-reference is the only coupling.
//
////////////////////////////////////////////////////////////////////////////////

class WindowCommandManager
{
public:
    // Turn a failure HRESULT into a "0xXXXXXXXX -- <system text>" detail line
    // for an error dialog: the friendly sentence is for humans; this trailer
    // is the hr + OS message for nerds (and bug reports). A Win32-wrapped code
    // (HRESULT_FROM_WIN32) resolves to its GetLastError text; other HRESULTs
    // resolve where the system has a message and degrade to just the hex code.
    //
    // Public because it is no longer print-specific: the salvage failure
    // dialog wants the same trailer, and error dialogs should read alike.
    static std::wstring  FormatSystemError (HRESULT hr);

    static WindowCommandRoute  GetCommandRoute (int id);

    explicit WindowCommandManager (EmulatorShell & shell);

    void  HandleCommand        (WORD commandId);
    bool  OnCommand            (HWND hwnd, int id);

    void  OnFileCommand        (int id);
    void  OnEditCommand        (int id);
    void  OnMachineCommand     (int id);
    void  OnViewCommand        (int id);
    void  OnDiskCommand        (int id);
    void  OpenCassque          ();
    void  OnPrinterCommand     (int id);
    void  OnHelpCommand        (int id);
    void  OnExternalDriveCommand (int id);
    void  OnMouseConnectCommand  (int id);

    bool  OnInitMenuPopup      (HWND hwnd, HMENU hMenu, UINT itemIndex, bool isWindowMenu);

    // Both prompts report whether a mount was actually STARTED through the
    // out param -- a cancel returns S_OK by design (backing out is not an
    // error), so the HRESULT alone cannot tell "mounted" from "canceled",
    // and BrowseForDisk needs the difference to restore the drive door.
    HRESULT  PromptForDiskImage   (int drive, bool & outMountStarted);
    HRESULT  PromptInsertDiskMru  (int drive, const RECT * anchorRectPx, bool & outMountStarted);
    HRESULT  PromptInsertTapeMru  (const RECT * anchorRectPx);
    void     GetRecentMedia       (bool (* isWanted) (const std::wstring &), std::vector<DiskMru::Entry> & entries);

    //  Where new disks and tapes are created: the last create's folder while it
    //  still exists, otherwise Documents\Casso Disks, created on demand.
    std::wstring  GetDiskCreateFolder ();

    // The create-a-blank-disk flow behind the picker's <Create new disk...>
    // row: dialog -> BlankDiskBuilder -> atomic write -> Mount.
    HRESULT  CreateBlankDiskForDrive (int drive, bool & outMountStarted);

private:
    EmulatorShell &  m_shell;
};
