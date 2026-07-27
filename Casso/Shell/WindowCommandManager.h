#pragma once

#include "Pch.h"

#include "ModernPrintDialog.h"


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
    // How a delivery attempt ended, reported separately from the HRESULT.
    // Backing out of a dialog is not a failure, so it must not ride on the
    // result code: an HRESULT that means "worked, but not the way you
    // assume" can only be decoded by reading the callee.
    enum class PrintOutcome
    {
        Delivered,   // reached the spooler, or the file was written
        Canceled,    // the user backed out -- nothing was delivered, no error
    };

    // Renders the strip to a PNG the user picks through IFileSaveDialog
    // (defaulting to <Pictures>\Casso Prints and a timestamped name), at the
    // configured dpi / dot style. Reports dialog cancellation through
    // outOutcome; hr means only success or failure.
    HRESULT  SavePrintoutAs (const class PrintRaster & raster, fs::path & outFile, PrintOutcome & outOutcome);

    // Delivers the strip to a Windows printer via the standard print dialog:
    // paginates (PrintPagination) and StretchDIBits each page's rendered span.
    // Cancellation -- of the dialog up front, or mid-job from the spooler --
    // comes back as outOutcome, not as a result code. Pure Win32 GDI edge.
    HRESULT  PrintToWindowsPrinter (const class PrintRaster & raster, std::wstring & failedStage, PrintOutcome & outOutcome);

    // Result dialog for the async modern print session (posted back as
    // IDM_PRINTER_MODERN_SENT / _FAILED from its completion callback).
    void     OnModernPrintResult   (bool succeeded);

    // Copies the strip to the clipboard as a bitmap (CF_DIB) and, when it fits,
    // a registered "PNG" blob, at the configured dpi / dot style. Does not
    // consume the strip. Pure Win32 clipboard edge (render/encode are core).
    HRESULT  CopyPrintoutToClipboard (const class PrintRaster & raster);

    EmulatorShell &  m_shell;

    // The modern OS print dialog with live preview (DCR-1); falls back to the
    // classic PrintDlg path when it cannot launch.
    ModernPrintDialog  m_modernPrint;
};
