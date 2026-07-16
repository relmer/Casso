#pragma once

#include "Pch.h"


class EmulatorShell;





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
    explicit WindowCommandManager (EmulatorShell & shell);

    void  HandleCommand        (WORD commandId);
    bool  OnCommand            (HWND hwnd, int id);

    void  OnFileCommand        (int id);
    void  OnEditCommand        (int id);
    void  OnMachineCommand     (int id);
    void  OnViewCommand        (int id);
    void  OnDiskCommand        (int id);
    void  OnPrinterCommand     (int id);
    void  OnHelpCommand        (int id);
    void  OnExternalDriveCommand (int id);
    void  OnMouseConnectCommand  (int id);

    bool  OnInitMenuPopup      (HWND hwnd, HMENU hMenu, UINT itemIndex, bool isWindowMenu);

    HRESULT  PromptForDiskImage   (int drive);
    HRESULT  PromptInsertDiskMru  (int drive);

private:
    // Renders the strip to a PNG the user picks through IFileSaveDialog
    // (defaulting to <Pictures>\Casso Prints and a timestamped name), at the
    // configured dpi / dot style. Returns S_FALSE when the dialog is
    // cancelled.
    HRESULT  SavePrintoutAs (const class PrintRaster & raster, fs::path & outFile);

    // Delivers the strip to a Windows printer via the standard print dialog:
    // paginates (PrintPagination) and StretchDIBits each page's rendered span.
    // Returns S_FALSE if the user cancels the dialog. Pure Win32 GDI edge.
    HRESULT  PrintToWindowsPrinter (const class PrintRaster & raster);

    // Copies the strip to the clipboard as a bitmap (CF_DIB) and, when it fits,
    // a registered "PNG" blob, at the configured dpi / dot style. Does not
    // consume the strip. Pure Win32 clipboard edge (render/encode are core).
    HRESULT  CopyPrintoutToClipboard (const class PrintRaster & raster);

    EmulatorShell &  m_shell;
};
