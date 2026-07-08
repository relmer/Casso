#include "Pch.h"

#include "WindowCommandManager.h"

#include "../AssetBootstrap.h"
#include "../EmulatorShell.h"
#include "../resource.h"
#include "../Shell/DiskMru.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PrintDelivery.h"
#include "Devices/Printer/PrintFileNaming.h"
#include "Devices/Printer/PrintPagination.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/PrinterCard.h"
#include "Devices/Printer/RgbaImage.h"
#include "Print/PrintJobStore.h"
#include "Version.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Shell/CpuManager.h"
#include "Shell/DiskManager.h"
#include "Shell/MachineManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    using namespace ChromeMetrics;

    // Blit an R,G,B,A image onto a printer HDC, scaled to fit the page while
    // preserving aspect ratio and top-aligned (fanfold continues downward).
    // GDI DIBs are BGRA, so the channels are swapped into a scratch buffer.
    HRESULT BlitRgbaToDc (HDC hdc, const RgbaImage & img, int pageW, int pageH)
    {
        HRESULT        hr    = S_OK;
        vector<Byte>   bgra;
        BITMAPINFO     bmi   = {};
        size_t         count = 0;
        size_t         i     = 0;
        double         scale = 1.0;
        int            destW = 0;
        int            destH = 0;
        int            blit  = 0;

        CBR (img.width > 0 && img.height > 0 && pageW > 0 && pageH > 0);

        count = (size_t) img.width * img.height;
        bgra.resize (count * 4);
        for (i = 0; i < count; i++)
        {
            bgra[i * 4 + 0] = img.rgba[i * 4 + 2];   // B
            bgra[i * 4 + 1] = img.rgba[i * 4 + 1];   // G
            bgra[i * 4 + 2] = img.rgba[i * 4 + 0];   // R
            bgra[i * 4 + 3] = img.rgba[i * 4 + 3];   // A
        }

        scale = (std::min) ((double) pageW / img.width, (double) pageH / img.height);
        if (scale <= 0.0) { scale = 1.0; }
        destW = (std::max) (1, (int) (img.width  * scale));
        destH = (std::max) (1, (int) (img.height * scale));

        bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = img.width;
        bmi.bmiHeader.biHeight      = -img.height;   // negative == top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        SetStretchBltMode (hdc, HALFTONE);
        SetBrushOrgEx     (hdc, 0, 0, nullptr);

        blit = StretchDIBits (hdc,
                              (pageW - destW) / 2, 0, destW, destH,
                              0, 0, img.width, img.height,
                              bgra.data (), &bmi, DIB_RGB_COLORS, SRCCOPY);
        CBREx (blit != GDI_ERROR, E_FAIL);

    Error:
        return hr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowCommandManager
//
////////////////////////////////////////////////////////////////////////////////

WindowCommandManager::WindowCommandManager (EmulatorShell & shell)
    : m_shell (shell)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleCommand
//
//  Public command-pump entry point. Used by the MainMenu so click
//  routing from the chrome funnels through the same dispatch path as
//  a Win32 menu pick. Intentionally a thin wrapper -- OnCommand owns
//  the real id-range demux.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::HandleCommand (WORD commandId)
{
    OnCommand (m_shell.m_hwnd, (int) commandId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCommand
//
////////////////////////////////////////////////////////////////////////////////

bool WindowCommandManager::OnCommand (HWND hwnd, int id)
{
    UNREFERENCED_PARAMETER (hwnd);

    if      (id >= IDM_EDIT_COPY_TEXT && id <= IDM_EDIT_PASTE)              { OnEditCommand (id); }
    else if (id >= IDM_FILE_OPEN      && id <= IDM_FILE_EXIT)               { OnFileCommand (id); }
    else if (id >= IDM_MACHINE_RESET  && id <= IDM_MACHINE_ARROWS_PADDLE)   { OnMachineCommand (id); }
    else if (id >= IDM_DISK_INSERT1   && id <= IDM_DISK_WRITEPROTECT2)      { OnDiskCommand (id); }
    else if (id >= IDM_VIEW_COLOR     && id <= IDM_VIEW_SETTINGS)           { OnViewCommand (id); }
    else if (id == IDM_PRINTER_EJECT)                                      { OnPrinterCommand (id); }
    else if (id == IDM_PRINTER_DISCARD)                                    { OnPrinterCommand (id); }
    else if (id >= IDM_HELP_KEYMAP    && id <= IDM_HELP_ABOUT)              { OnHelpCommand (id); }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnFileCommand
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnFileCommand (int id)
{
    switch (id)
    {
        case IDM_FILE_OPEN:
        {
            m_shell.ShowMachinePicker();
            break;
        }

        case IDM_FILE_EXIT:
        {
            DestroyWindow (m_shell.m_hwnd);
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnEditCommand
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnEditCommand (int id)
{
    switch (id)
    {
        case IDM_EDIT_COPY_TEXT:
        {
            m_shell.m_clipboardManager->CopyScreenText (m_shell.m_hwnd);
            break;
        }

        case IDM_EDIT_COPY_SCREENSHOT:
        {
            m_shell.m_clipboardManager->CopyScreenshot (m_shell.m_hwnd);
            break;
        }

        case IDM_EDIT_PASTE:
        {
            m_shell.m_clipboardManager->PasteFromClipboard (m_shell.m_hwnd);
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMachineCommand
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnMachineCommand (int id)
{
    bool paused = false;



    switch (id)
    {
        case IDM_MACHINE_RESET:
        case IDM_MACHINE_POWERCYCLE:
        {
            m_shell.PostCommand (static_cast<WORD> (id));
            break;
        }

        case IDM_MACHINE_PAUSE:
        {
            paused = m_shell.m_cpuManager.TogglePaused();
            (void) paused;
            m_shell.UpdateWindowTitle();
            break;
        }

        case IDM_MACHINE_STEP:
        {
            if (! m_shell.m_cpuManager.IsPaused())
            {
                break;
            }
            // CPU thread is provably idle (blocked on pauseCV.wait), so
            // it's safe to drive the step directly from the UI thread.
            // Routing through PostCommand+queue would never run -- the
            // CPU thread can't drain its queue while paused. Delegated
            // through the shell to avoid pulling Disk2Controller's full
            // definition into this header.
            m_shell.StepInstructionWhilePaused();
            break;
        }

        case IDM_MACHINE_SPEED_1X:
        {
            m_shell.m_cpuManager.SetSpeedMode (SpeedMode::Authentic);
            break;
        }

        case IDM_MACHINE_SPEED_2X:
        {
            m_shell.m_cpuManager.SetSpeedMode (SpeedMode::Double);
            break;
        }

        case IDM_MACHINE_SPEED_MAX:
        {
            m_shell.m_cpuManager.SetSpeedMode (SpeedMode::Maximum);
            break;
        }

        case IDM_MACHINE_INFO:
        {
            std::wstring info = std::format (
                L"Machine: {}\n"
                L"CPU: {}\n"
                L"Clock speed: {} Hz\n"
                L"Memory regions: {}\n"
                L"Devices: {}",
                std::wstring (m_shell.m_config.name.begin(), m_shell.m_config.name.end()),
                std::wstring (m_shell.m_config.cpu.begin(), m_shell.m_config.cpu.end()),
                m_shell.m_config.clockSpeed,
                (m_shell.m_config.ram.size() + 1 + m_shell.m_config.slots.size()),
                (m_shell.m_config.internalDevices.size() + m_shell.m_config.slots.size()));

            DialogDefinition def = {};
            def.title = L"Machine info";
            def.icon  = DialogIcon::Info;
            def.body.push_back ({ info, false, L"" });
            def.buttons.push_back ({ L"OK", 0, true, true });
            (void) m_shell.ShowModalDialog (def);
            break;
        }

        case IDM_MACHINE_ARROWS_JOYSTICK:
        {
            m_shell.ToggleInputMappingMode (InputMappingMode::Joystick);
            break;
        }

        case IDM_MACHINE_ARROWS_PADDLE:
        {
            m_shell.ToggleInputMappingMode (InputMappingMode::Paddle);
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnViewCommand
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnViewCommand (int id)
{
    HMONITOR    hMon  = nullptr;
    MONITORINFO mi    = { sizeof (mi) };
    int         w     = 0;
    int         h     = 0;
    int         x     = 0;
    int         y     = 0;



    switch (id)
    {
        case IDM_VIEW_COLOR:
        {
            m_shell.m_colorMode.store (ColorMode::Color, std::memory_order_release);
            break;
        }

        case IDM_VIEW_GREEN:
        {
            m_shell.m_colorMode.store (ColorMode::GreenMono, std::memory_order_release);
            break;
        }

        case IDM_VIEW_AMBER:
        {
            m_shell.m_colorMode.store (ColorMode::AmberMono, std::memory_order_release);
            break;
        }

        case IDM_VIEW_WHITE:
        {
            m_shell.m_colorMode.store (ColorMode::WhiteMono, std::memory_order_release);
            break;
        }

        case IDM_VIEW_FULLSCREEN:
        {
            m_shell.m_d3dRenderer.ToggleFullscreen (m_shell.m_hwnd);
            break;
        }

        case IDM_VIEW_RESET_SIZE:
        {
            if (!m_shell.m_d3dRenderer.IsFullscreen())
            {
                RECT  rcCurrentClient = {};
                RECT  rcCurrentWindow = {};
                int   desiredClientW  = 0;
                int   desiredClientH  = 0;
                int   ncOverheadW     = 0;
                int   ncOverheadH     = 0;


                // Target client area: framebuffer at the current DPI
                // (linear scale), with the chrome band insets summed by
                // the single source of truth. EmulatorShell::
                // ClientSizeForFramebufferPx owns the framebuffer scale
                // policy -- see it for the one-line toggle to switch to
                // integer-only scaling.
                {
                    SIZE  desired = m_shell.ClientSizeForFramebufferPx (
                                        kFramebufferWidthPx,
                                        kFramebufferHeightPx);
                    desiredClientW = (int) desired.cx;
                    desiredClientH = (int) desired.cy;
                }

                // Measure the current window's non-client overhead and
                // size the new window from that, rather than computing
                // it theoretically with AdjustWindowRectExForDpi. Our
                // WM_NCCALCSIZE handler doesn't match the stock
                // calculation, so the AdjustWindowRect path used to
                // land on a wrong size and need a follow-up nudge --
                // and that nudge was visible as a jitter on every
                // Ctrl+0 press. Style/DPI don't change between the
                // measurement and the SetWindowPos, so the real
                // overhead is a stable input.
                if (GetClientRect (m_shell.m_hwnd, &rcCurrentClient) && GetWindowRect (m_shell.m_hwnd, &rcCurrentWindow))
                {
                    ncOverheadW = (rcCurrentWindow.right  - rcCurrentWindow.left)
                                  - (rcCurrentClient.right  - rcCurrentClient.left);
                    ncOverheadH = (rcCurrentWindow.bottom - rcCurrentWindow.top)
                                  - (rcCurrentClient.bottom - rcCurrentClient.top);
                }

                w = desiredClientW + ncOverheadW;
                h = desiredClientH + ncOverheadH;

                hMon = MonitorFromWindow (m_shell.m_hwnd, MONITOR_DEFAULTTONEAREST);
                GetMonitorInfo (hMon, &mi);

                x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
                y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;

                SetWindowPos (m_shell.m_hwnd, nullptr, x, y, w, h, SWP_NOZORDER);
            }
            break;
        }

        case IDM_VIEW_DISK2_DEBUG:
        {
            m_shell.OpenDisk2DebugDialog();
            break;
        }

        case IDM_VIEW_INPUT_DEBUG:
        {
            m_shell.OpenInputDebugDialog();
            break;
        }

        case IDM_VIEW_SETTINGS:
        {
            m_shell.OpenSettings();
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptForDiskImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::PromptForDiskImage (int drive)
{
    HRESULT                          hr         = S_OK;
    ComPtr<IFileOpenDialog>          dialog;
    ComPtr<IShellItem>               item;
    PWSTR                            pszPath    = nullptr;
    COMDLG_FILTERSPEC                filters[1] = { { L"Disk images", L"*.dsk;*.nib;*.woz;*.po" } };



    hr = CoCreateInstance (CLSID_FileOpenDialog,
                           nullptr,
                           CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS (&dialog));
    CHR (hr);

    hr = dialog->SetFileTypes (1, filters);
    CHR (hr);

    hr = dialog->Show (m_shell.m_hwnd);
    if (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        hr = S_FALSE;
        goto Error;
    }
    CHR (hr);

    hr = dialog->GetResult (&item);
    CHR (hr);

    hr = item->GetDisplayName (SIGDN_FILESYSPATH, &pszPath);
    CHR (hr);

    hr = m_shell.Mount (6, drive - 1, pszPath);
    CHR (hr);

Error:
    if (pszPath != nullptr)
    {
        CoTaskMemFree (pszPath);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptInsertDiskMru
//
//  Shows the themed disk MRU picker. Routes the user's chosen disk
//  (recent image or stock master download) to Mount(); if the user
//  clicks "Browse..." this falls through to the IFileOpenDialog path
//  via PromptForDiskImage.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::PromptInsertDiskMru (int drive)
{
    HRESULT                      hr          = S_OK;
    DiskMru                      mru;
    std::vector<DiskMru::Entry>  mruPruned;
    std::filesystem::path        diskDir;
    std::wstring                 chosenPath;
    std::string                  error;
    bool                         userBrowsed = false;


    diskDir   = AssetBootstrap::GetDiskDirectory();
    mru       = DiskMru::FromUtf8 (m_shell.m_globalPrefs.recentDisks,
                                   m_shell.m_globalPrefs.recentDiskLoadedAt);
    mruPruned = mru.Prune ([] (const std::filesystem::path & p)
                           {
                               return std::filesystem::exists (p)
                                      && !AssetBootstrap::IsForeignWorktreeDisk (p);
                           });

    AssetBootstrap::AppendBundledDemoDisks (mruPruned);

    hr = AssetBootstrap::PromptInsertDiskMru (GetModuleHandle (nullptr),
                                              m_shell.m_hwnd,
                                              drive,
                                              mruPruned,
                                              diskDir,
                                              m_shell.m_globalPrefs.activeTheme,
                                              chosenPath,
                                              userBrowsed,
                                              error);
    CHR (hr);

    if (userBrowsed)
    {
        hr = PromptForDiskImage (drive);
        CHR (hr);
    }
    else if (!chosenPath.empty())
    {
        hr = m_shell.Mount (6, drive - 1, chosenPath);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDiskCommand
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnDiskCommand (int id)
{
    HRESULT  hr    = S_OK;
    int      drive = 0;



    switch (id)
    {
        case IDM_DISK_INSERT1:
        case IDM_DISK_INSERT2:
        {
            drive = (id == IDM_DISK_INSERT1) ? 1 : 2;

            hr = PromptInsertDiskMru (drive);
            IGNORE_RETURN_VALUE (hr, S_OK);
            break;
        }

        case IDM_DISK_EJECT1:
        case IDM_DISK_EJECT2:
        {
            m_shell.PostCommand (static_cast<WORD> (id));
            break;
        }
    }
}





// Settings > Printing knobs, read live from GlobalUserPrefs at each eject.
static int PrintDpiFromPrefs (const GlobalUserPrefs & p)
{
    return (p.printOutputDpi == 288) ? 288 : 576;   // only 288 / 576 valid (FR-028)
}

static DotStyle PrintDotStyleFromPrefs (const GlobalUserPrefs & p)
{
    return (p.printDotStyle == "plain") ? DotStyle::Plain : DotStyle::Ink;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SavePrintout
//
//  Renders the finished strip to a PNG under the configured folder (Settings >
//  Printing; default <Pictures>\Casso Prints) at the chosen dpi + dot style,
//  and hands back the path written. Render/encode/naming are core (unit-
//  tested); this contributes only the irreducible edges -- known-folder
//  resolution, directory creation, and the file write. COM is live on the UI
//  thread (OLE drag-drop), so PngCodec's WIC calls are valid here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::SavePrintout (const PrintRaster & raster, fs::path & outFile)
{
    HRESULT                   hr          = S_OK;
    PWSTR                     picturesRaw = nullptr;
    vector<Byte>              png;
    fs::path                  folder;
    SYSTEMTIME                now         = {};
    std::error_code           ec;
    const GlobalUserPrefs &   prefs       = m_shell.m_globalPrefs;

    hr = PrintDelivery::RenderToPng (raster, 0, raster.RowsUsed () - 1,
                                     PrintDpiFromPrefs (prefs),
                                     PrintDotStyleFromPrefs (prefs), png);
    CHR (hr);

    if (!prefs.printPngFolder.empty ())
    {
        folder = fs::path (prefs.printPngFolder);
    }
    else
    {
        hr = SHGetKnownFolderPath (FOLDERID_Pictures, 0, nullptr, &picturesRaw);
        CHR (hr);

        folder = fs::path (picturesRaw) / L"Casso Prints";
    }

    fs::create_directories (folder, ec);

    GetLocalTime (&now);
    outFile = PrintFileNaming::ComposePngPath (folder, now,
                  [] (const fs::path & p) { std::error_code e; return fs::exists (p, e); });

    {
        std::ofstream   out (outFile, std::ios::binary | std::ios::trunc);

        CBREx (out.is_open (), E_FAIL);
        out.write ((const char *) png.data (), (std::streamsize) png.size ());
        CBREx (out.good (), E_FAIL);
    }

Error:
    if (picturesRaw != nullptr)
    {
        CoTaskMemFree (picturesRaw);
    }
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintToWindowsPrinter
//
//  Delivers the strip to a Windows printer through the standard print dialog.
//  The strip is paginated (PrintPagination -- core, unit-tested) and each
//  page's row span is rendered (PaperRenderer -- core) and StretchDIBits'd onto
//  the printer DC. Only the dialog + GDI job are here (the untestable Win32
//  edge). Returns S_FALSE if the user cancels the dialog.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::PrintToWindowsPrinter (const PrintRaster & raster)
{
    HRESULT                                hr      = S_OK;
    const GlobalUserPrefs &                prefs   = m_shell.m_globalPrefs;
    vector<PrintPagination::PageRange>     pages   = PrintPagination::Paginate (raster);
    PRINTDLGW                              pd      = {};
    DOCINFOW                               di      = {};
    bool                                   started = false;
    int                                    pageW   = 0;
    int                                    pageH   = 0;

    CBR (!pages.empty ());

    pd.lStructSize = sizeof (pd);
    pd.hwndOwner   = m_shell.m_hwnd;
    pd.Flags       = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
    pd.nCopies     = 1;

    if (!PrintDlgW (&pd))
    {
        hr = S_FALSE;   // cancelled / closed
        goto Error;
    }
    CBREx (pd.hDC != nullptr, E_FAIL);

    di.cbSize      = sizeof (di);
    di.lpszDocName = L"Casso Printout";

    CBREx (StartDocW (pd.hDC, &di) > 0, E_FAIL);
    started = true;

    pageW = GetDeviceCaps (pd.hDC, HORZRES);
    pageH = GetDeviceCaps (pd.hDC, VERTRES);

    for (const PrintPagination::PageRange & pr : pages)
    {
        PaperRenderer            renderer;
        PaperRenderer::Options   opt;
        RgbaImage                img;

        opt.outputDpi = PrintDpiFromPrefs (prefs);
        opt.style     = PrintDotStyleFromPrefs (prefs);

        hr = renderer.Render (raster, pr.firstRow, pr.lastRow, opt, img);
        CHR (hr);

        CBREx (StartPage (pd.hDC) > 0, E_FAIL);

        hr = BlitRgbaToDc (pd.hDC, img, pageW, pageH);
        CHR (hr);

        CBREx (EndPage (pd.hDC) > 0, E_FAIL);
    }

    CBREx (EndDoc (pd.hDC) > 0, E_FAIL);
    started = false;

Error:
    if (started && pd.hDC != nullptr)
    {
        AbortDoc (pd.hDC);
    }
    if (pd.hDC != nullptr)
    {
        DeleteDC (pd.hDC);
    }
    if (pd.hDevMode != nullptr)
    {
        GlobalFree (pd.hDevMode);
    }
    if (pd.hDevNames != nullptr)
    {
        GlobalFree (pd.hDevNames);
    }
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterCommand
//
//  Handle the strip-level printer commands. Eject finishes the current job:
//  stop the drain, render+save the strip, then resume onto a fresh sheet
//  (the fanfold metaphor), so a failed save loses the page rather than
//  stalling the guest. Discard tears off and throws away the current page
//  after a confirm, clearing the persisted pending copy (FR-029). Both drain
//  the ring first so they act on the complete strip.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnPrinterCommand (int id)
{
    HRESULT        hr   = S_OK;
    PrinterJob *   job  = nullptr;
    fs::path       file;

    if ((id != IDM_PRINTER_EJECT && id != IDM_PRINTER_DISCARD) ||
        m_shell.m_refs.printerCard == nullptr)
    {
        return;
    }

    bool   discard = (id == IDM_PRINTER_DISCARD);

    // Take ownership of the strip: stop the worker, then flush any tail bytes.
    // Both eject and discard act on the whole strip, so we drain first either way.
    m_shell.m_printerWorker.Stop ();

    {
        vector<PrinterEvent>   events;
        m_shell.m_printerWorker.FlushNow (events);
    }

    job = m_shell.m_printerWorker.Job ();

    if (job == nullptr || !job->HasContent ())
    {
        MessageBoxW (m_shell.m_hwnd,
                     discard ? L"The printer has no page to discard."
                             : L"The printer has no page to finish yet.",
                     L"Casso Printer", MB_OK | MB_ICONINFORMATION);
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing ());
        return;
    }

    if (discard)
    {
        // Tear off and throw away the current page (FR-029). Confirm first --
        // there is no undo -- and default the dialog to "No" so a stray Enter
        // never destroys a page.
        int   choice = MessageBoxW (
            m_shell.m_hwnd,
            L"Tear off and discard the current printout?\n\n"
            L"The page in the printer will be thrown away without saving. "
            L"This cannot be undone.",
            L"Discard Printout", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

        if (choice != IDYES)
        {
            // Cancelled: keep the strip and resume on the same page.
            m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing (), job->Raster ());
            return;
        }

        // Confirmed: start a fresh sheet and drop the persisted pending copy.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing ());
        PrintJobStore::Clear (m_shell.PendingPrintDir ());
        return;
    }

    // Deliver to the configured destination (Settings > Printing).
    bool   toPrinter = (m_shell.m_globalPrefs.printDestination == "windowsPrinter");

    if (toPrinter)
    {
        hr = PrintToWindowsPrinter (job->Raster ());
    }
    else
    {
        hr = SavePrintout (job->Raster (), file);
    }

    if (hr == S_FALSE)
    {
        // User cancelled the print dialog: keep the strip, no fresh sheet.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing (), job->Raster ());
        return;
    }

    if (SUCCEEDED (hr))
    {
        std::wstring   msg = toPrinter
                                 ? std::wstring (L"Sent the printout to the printer.")
                                 : (L"Saved printout to:\n" + file.wstring ());

        MessageBoxW (m_shell.m_hwnd, msg.c_str (), L"Casso Printer", MB_OK | MB_ICONINFORMATION);

        // Delivered: start a fresh sheet and drop the persisted pending copy.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing ());
        PrintJobStore::Clear (m_shell.PendingPrintDir ());
    }
    else
    {
        MessageBoxW (m_shell.m_hwnd, L"Could not deliver the printout; the page is kept.",
                     L"Casso Printer", MB_OK | MB_ICONWARNING);

        // Keep the strip so the user can retry -- reseed the worker with it
        // (copied before the old job is replaced). It re-persists on exit.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing (), job->Raster ());
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnHelpCommand
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnHelpCommand (int id)
{
    switch (id)
    {
        case IDM_HELP_KEYMAP:
        {
            DialogDefinition def = {};
            def.title = L"Keyboard map";
            def.icon  = DialogIcon::Info;
            def.body.push_back ({
                L"PC key mapping:\n\n"
                L"Arrow keys -> Apple ][ cursor movement\n"
                L"Enter -> Return\n"
                L"Escape -> Escape\n"
                L"Delete -> Delete\n"
                L"Ctrl+Reset -> Warm reset\n"
                L"Left Alt -> Open Apple (//e)\n"
                L"Right Alt -> Closed Apple (//e)\n\n"
                L"Emulator controls:\n"
                L"Ctrl+Shift+R -> Reset\n"
                L"Ctrl+Shift+P -> Power cycle\n"
                L"Pause -> Pause/resume\n"
                L"F11 -> Step (when paused)\n"
                L"Alt+Enter -> Fullscreen\n"
                L"Ctrl+0 -> Reset window size",
                false, L"" });
            def.buttons.push_back ({ L"OK", 0, true, true });
            (void) m_shell.ShowModalDialog (def);
            break;
        }

        case IDM_HELP_ABOUT:
        {
            DialogDefinition def = {};
            def.title = L"About Casso";
            def.icon  = DialogIcon::AppPhotoreal;
            def.iconSizeOverrideDp = 128.0f;
            def.body.push_back ({ L"Casso Emulator\n\nVersion " _CRT_WIDE (VERSION_STRING)
                                  L"\nBuilt " _CRT_WIDE (VERSION_BUILD_TIMESTAMP)
                                  L"\n\nAn Apple ][, Apple ][ plus, and Apple //e platform emulator "
                                  L"built on the Casso 6502 assembler/emulator project.\n\n",
                                  false, L"" });
            def.body.push_back ({ L"https://github.com/relmer/Casso",
                                  true, L"https://github.com/relmer/Casso" });
            def.body.push_back ({ L"\nCopyright (C) by Robert Elmer\n",
                                  false, L"" });
            def.body.push_back ({ L"MIT License",
                                  true, L"https://github.com/relmer/Casso/blob/master/LICENSE" });
            def.buttons.push_back ({ L"OK", 0, true, true });
            (void) m_shell.ShowModalDialog (def);
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnInitMenuPopup
//
//  Recomputes the dynamic menu items (enable/disable, checkmarks)
//  every time Windows opens a popup so a SwitchMachine that swaps the
//  active config between menu opens picks up the controller delta on
//  the next click.
//
////////////////////////////////////////////////////////////////////////////////

bool WindowCommandManager::OnInitMenuPopup (HWND hwnd, HMENU hMenu, UINT itemIndex, bool isWindowMenu)
{
    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (hMenu);
    UNREFERENCED_PARAMETER (itemIndex);
    UNREFERENCED_PARAMETER (isWindowMenu);

    return true;
}
