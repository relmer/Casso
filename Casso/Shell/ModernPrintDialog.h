#pragma once

#include "Pch.h"

#include "../../CassoEmuCore/Devices/Printer/PrinterTypes.h"   // DotStyle

#include <eventtoken.h>   // EventRegistrationToken (WinRT event cookie)

class PrintRaster;




////////////////////////////////////////////////////////////////////////////////
//
//  ModernPrintDialog
//
//  The Windows modern print UI with a live preview (spec 015 DCR-1). The
//  classic PrintDlg path leaves the OS preview pane reading "this app doesn't
//  support print preview"; this launches the Windows.Graphics.Printing dialog
//  instead and fills that pane with the real paginated fanfold pages.
//
//  Plumbing (all raw-ABI WRL in the .cpp -- no C++/WinRT dependency):
//  RoGetActivationFactory("Windows.Graphics.Printing.PrintManager") ->
//  IPrintManagerInterop::GetForWindow + ShowPrintUIForWindowAsync. The
//  PrintTaskRequested callback hands the task a document source implementing
//  IPrintPreviewPageCollection (preview pages drawn via D2D onto the DXGI
//  preview target) and IPrintDocumentPageSource (final print through
//  ID2D1PrintControl -- the D2D print API, no hand-rolled XPS). Pages come
//  from the same PrintPagination + PaperRenderer spans the classic GDI path
//  prints, so preview and paper always match.
//
//  ShowAsync COPIES the strip into the print session and returns immediately
//  (the caller resumes the printer worker right away; Print is
//  non-destructive). Completion is posted back to the window as a WM_COMMAND
//  (IDM_PRINTER_MODERN_SENT / _FAILED) so result dialogs run on the UI
//  thread; a cancelled dialog posts nothing, matching the classic S_FALSE.
//  Any launch failure returns FAILED so the caller falls back to the classic
//  dialog -- older Windows keeps working unchanged.
//
////////////////////////////////////////////////////////////////////////////////

class ModernPrintDialog
{
public:
    ModernPrintDialog  () = default;
    ~ModernPrintDialog ();

    // Launch the OS print UI over `hwnd` for a copy of `raster`, rendering at
    // `outputDpi` in `style` (the user's Printing prefs). S_OK == the dialog
    // is up and the session owns its data; any failure == use the classic
    // path. A second call while a session is in flight re-shows the UI.
    HRESULT  ShowAsync (HWND hwnd, const PrintRaster & raster, int outputDpi, DotStyle style);

private:
    // ABI-typed internals live in the .cpp; the header keeps only opaque
    // ownership so no Windows.Graphics.Printing headers leak into includers.
    Microsoft::WRL::ComPtr<IUnknown>  m_interop;      // IPrintManagerInterop
    Microsoft::WRL::ComPtr<IUnknown>  m_manager;      // IPrintManager (per window)
    Microsoft::WRL::ComPtr<IUnknown>  m_session;      // active document source
    EventRegistrationToken            m_taskToken     = {};
    bool                              m_registered    = false;
    HWND                              m_hwnd          = nullptr;
};
