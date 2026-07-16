#include "Pch.h"

#include "WindowCommandManager.h"

#include "../AssetBootstrap.h"
#include "../EmulatorShell.h"
#include "../resource.h"
#include "../Shell/DiskMru.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/PrintDelivery.h"
#include "Devices/Printer/PrintFileNaming.h"
#include "Devices/Printer/PrintPagination.h"
#include "Window/DxuiMessageBox.h"
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

    // Trace the Windows-printer delivery path (visible in DebugView / the VS
    // Output window). A "failed to deliver" is otherwise a black box -- this puts
    // the chosen driver, page geometry, and the exact failing GDI call +
    // GetLastError into the log so a PDF / real-printer failure can be diagnosed.
    void  LogPrinter (const std::wstring & msg)
    {
        OutputDebugStringW ((L"[Printer] " + msg + L"\n").c_str ());
    }

    // The device the user picked in the print dialog (DEVNAMES holds it as an
    // offset into its own block), for the delivery trace.
    std::wstring  SelectedPrinterName (const PRINTDLGW & pd)
    {
        std::wstring   name;

        if (pd.hDevNames != nullptr)
        {
            const DEVNAMES *  dn = (const DEVNAMES *) GlobalLock (pd.hDevNames);

            if (dn != nullptr)
            {
                name = (const wchar_t *) dn + dn->wDeviceOffset;
                GlobalUnlock (pd.hDevNames);
            }
        }

        return name;
    }

    // Blit an R,G,B,A image onto a printer HDC. The strip is scaled to fit the
    // page WIDTH (uniform scale, aspect preserved) and top-aligned so the
    // fanfold continues downward across page breaks. Fitting to width -- not
    // min(width,height) -- is deliberate: the strip width is identical on every
    // page, so a width fit gives every page the same horizontal scale and left
    // edge. A min() fit would height-limit full pages but width-limit the short
    // last page, scaling their columns differently and misaligning page-to-page.
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

        scale = (double) pageW / img.width;   // fit width; identical scale on every page
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
        CBRFEx (blit != GDI_ERROR, E_FAIL,
                LogPrinter (std::format (L"StretchDIBits GDI_ERROR: src {}x{} -> dest {}x{} on page {}x{}, GetLastError={}",
                                         img.width, img.height, destW, destH, pageW, pageH, ::GetLastError ())));

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
    else if (id == IDM_PRINTER_DISCARD)                                    { OnPrinterCommand (id); }
    else if (id == IDM_PRINTER_COPY)                                       { OnPrinterCommand (id); }
    else if (id == IDM_PRINTER_PRINT)                                      { OnPrinterCommand (id); }
    else if (id == IDM_PRINTER_SAVEAS)                                     { OnPrinterCommand (id); }
    else if (id == IDM_PRINTER_PREVIEW)                                    { m_shell.ShowPrinterPanel (); }
    else if (id >= IDM_HELP_KEYMAP    && id <= IDM_HELP_ABOUT)              { OnHelpCommand (id); }
    else if (id == IDM_DRIVE_EXTERNAL_CONNECT ||
             id == IDM_DRIVE_EXTERNAL_DISCONNECT)                          { OnExternalDriveCommand (id); }
    else if (id == IDM_MOUSE_CONNECT ||
             id == IDM_MOUSE_DISCONNECT)                                   { OnMouseConnectCommand (id); }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnExternalDriveCommand
//
//  //c optional external drive: reveal/hide the second drive-mount widget
//  (m_driveChrome[1]). Runs on the UI thread -- it relays the chrome (menu
//  bar + drive band), which asserts UI-thread affinity -- so it is reached
//  via PostMessage(WM_COMMAND) from the settings apply sink, not the CPU
//  command queue. Disk presence is unchanged (the //c keeps its built-in
//  controller), so ReflowChromeForMachineChange does no window resize -- it
//  just re-lays the widgets + hit-test map, where ShouldShowExternalDrive()
//  gates the second widget.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnMouseConnectCommand (int id)
{
    // //c mouse peripheral connect/disconnect. No chrome change;
    // just the state gate. Disconnecting while Mouse mode is active drops
    // the mapping to Off so the mode never points at an unplugged device.
    bool  connected = (id == IDM_MOUSE_CONNECT);

    if (connected != m_shell.m_mouseConnected)
    {
        m_shell.m_mouseConnected = connected;

        if (!connected && m_shell.m_pointerMode == InputMappingMode::Mouse)
        {
            // Drops Mouse mode; SetPointerMapping re-syncs the selector as a
            // side effect (SyncInputModeUi -> SyncSelectorState + relayout).
            m_shell.SetPointerMapping (InputMappingMode::Off);
        }
        else
        {
            // Availability changed with no mode change (reconnect, or a
            // disconnect while not in Mouse mode): refresh the selector so
            // the Mouse segment reappears / disappears -- SetState flips the
            // availability flag and the relayout rebuilds the 2<->3 segment
            // geometry + hit map. UI-thread routed (posted WM_COMMAND), so
            // relaying the button here is safe.
            m_shell.SyncSelectorState();
            m_shell.RelayoutJoystickButton();
        }
    }
}




void WindowCommandManager::OnExternalDriveCommand (int id)
{
    bool  connected = (id == IDM_DRIVE_EXTERNAL_CONNECT);

    if (connected != m_shell.m_externalDriveConnected)
    {
        m_shell.m_externalDriveConnected = connected;
        m_shell.ReflowChromeForMachineChange();
    }
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
    COMDLG_FILTERSPEC                filters[2] = { { L"Disk images", L"*.dsk;*.do;*.nib;*.woz;*.po" },
                                                    { L"All files",   L"*.*" } };



    hr = CoCreateInstance (CLSID_FileOpenDialog,
                           nullptr,
                           CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS (&dialog));
    CHR (hr);

    hr = dialog->SetFileTypes (ARRAYSIZE (filters), filters);
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
                                      && !AssetBootstrap::IsForeignCheckoutDisk (p);
                           });

    AssetBootstrap::AppendSiblingDisksFromMruFolders (mruPruned);
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


// Cap the render dpi for a WHOLE-strip render (PNG file, clipboard) so a long
// fanfold banner's single RGBA image stays within a memory budget instead of
// ballooning to gigabytes (each row at 576 dpi is 4608 px * 4 B; a 60-page
// banner is ~95k rows). The native grid is only 160x144 dpi, so dropping a huge
// banner from 576 toward ~150 dpi is still well above source resolution -- no
// meaningful quality loss, and it never OOMs. Short jobs keep the full dpi.
static int WholeStripDpi (const GlobalUserPrefs & prefs, int rows)
{
    const double   kBudgetPx = 128.0 * 1024.0 * 1024.0;   // ~512 MB of RGBA
    int            dpi       = PrintDpiFromPrefs (prefs);

    if (rows > 0)
    {
        // outPx = (kDotsPerRow/160 * dpi) * (rows/144 * dpi)
        //       = kDotsPerRow * rows * dpi^2 / (160 * 144)
        double   maxDpi = std::sqrt (kBudgetPx * 160.0 * 144.0
                                     / ((double) PrinterGrid::kDotsPerRow * (double) rows));

        if ((int) maxDpi < dpi)
        {
            dpi = (std::max) (120, (int) maxDpi);
        }
    }

    return dpi;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SavePrintoutAs
//
//  The user picks the destination through IFileSaveDialog (seeded with the
//  default folder <Pictures>\Casso Prints and a timestamped name), and the
//  strip renders to that exact path at the configured dpi + dot style.
//  Returns S_FALSE when the dialog is cancelled.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::SavePrintoutAs (const PrintRaster & raster, fs::path & outFile)
{
    HRESULT                   hr          = S_OK;
    ComPtr<IFileSaveDialog>   dialog;
    ComPtr<IShellItem>        folderItem;
    ComPtr<IShellItem>        item;
    PWSTR                     pszPath     = nullptr;
    PWSTR                     picturesRaw = nullptr;
    fs::path                  folder;
    fs::path                  suggested;
    vector<Byte>              png;
    SYSTEMTIME                now         = {};
    std::error_code           ec;
    const GlobalUserPrefs &   prefs       = m_shell.m_globalPrefs;

    static const COMDLG_FILTERSPEC   s_kFilters[] =
    {
        { L"PNG image", L"*.png" },
    };

    hr = CoCreateInstance (CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS (&dialog));
    CHR (hr);

    hr = dialog->SetFileTypes (ARRAYSIZE (s_kFilters), s_kFilters);
    CHR (hr);

    hr = dialog->SetDefaultExtension (L"png");
    CHR (hr);

    // Seed the default folder <Pictures>\Casso Prints + a timestamped name.
    if (SUCCEEDED (SHGetKnownFolderPath (FOLDERID_Pictures, 0, nullptr, &picturesRaw)))
    {
        folder = fs::path (picturesRaw) / L"Casso Prints";
    }

    if (!folder.empty ())
    {
        fs::create_directories (folder, ec);

        if (SUCCEEDED (SHCreateItemFromParsingName (folder.c_str (), nullptr,
                                                    IID_PPV_ARGS (&folderItem))))
        {
            IGNORE_RETURN_VALUE (hr, dialog->SetFolder (folderItem.Get ()));
        }
    }

    GetLocalTime (&now);
    suggested = PrintFileNaming::ComposePngPath (folder, now,
                    [] (const fs::path & p) { std::error_code e; return fs::exists (p, e); });

    hr = dialog->SetFileName (suggested.filename ().c_str ());
    CHR (hr);

    hr = dialog->Show (m_shell.PrinterDialogOwner ());

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

    outFile = fs::path (pszPath);

    hr = PrintDelivery::RenderToPng (raster, 0, raster.RowsUsed () - 1,
                                     WholeStripDpi (prefs, raster.RowsUsed ()),
                                     PrintDotStyleFromPrefs (prefs), png);
    CHR (hr);

    {
        std::ofstream   out (outFile, std::ios::binary | std::ios::trunc);

        CBREx (out.is_open (), E_FAIL);
        out.write ((const char *) png.data (), (std::streamsize) png.size ());
        CBREx (out.good (), E_FAIL);
    }

Error:
    if (pszPath != nullptr)
    {
        CoTaskMemFree (pszPath);
    }
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
    int                                    pageIx  = 0;

    LogPrinter (std::format (L"deliver: {} rows -> {} page(s), dpi={}, style={}",
                             raster.RowsUsed (), pages.size (), PrintDpiFromPrefs (prefs),
                             PrintDotStyleFromPrefs (prefs) == DotStyle::Ink ? L"ink" : L"plain"));

    CBRF (!pages.empty (), LogPrinter (L"deliver: nothing to print (0 pages)"));

    pd.lStructSize = sizeof (pd);
    pd.hwndOwner   = m_shell.m_hwnd;
    pd.Flags       = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
    pd.nCopies     = 1;

    if (!PrintDlgW (&pd))
    {
        // CommDlgExtendedError == 0 means the user simply cancelled the dialog.
        DWORD   cderr = CommDlgExtendedError ();

        LogPrinter (std::format (L"PrintDlg returned false (CommDlgExtendedError=0x{:X}){}",
                                 cderr, cderr == 0 ? L" -- user cancelled" : L""));
        hr = S_FALSE;   // cancelled / closed
        goto Error;
    }
    LogPrinter (std::format (L"printer='{}'", SelectedPrinterName (pd)));
    CBRFEx (pd.hDC != nullptr, E_FAIL, LogPrinter (L"PrintDlg returned a null DC"));

    di.cbSize      = sizeof (di);
    di.lpszDocName = L"Casso Printout";

    // "Microsoft Print to PDF" pops its Save-As prompt here; cancelling it makes
    // StartDoc fail (GetLastError == ERROR_CANCELLED / 1223) rather than crash.
    CBRFEx (StartDocW (pd.hDC, &di) > 0, E_FAIL,
            LogPrinter (std::format (L"StartDoc failed, GetLastError={}", ::GetLastError ())));
    started = true;

    pageW = GetDeviceCaps (pd.hDC, HORZRES);
    pageH = GetDeviceCaps (pd.hDC, VERTRES);
    LogPrinter (std::format (L"device page: {}x{} px, {}x{} dpi",
                             pageW, pageH,
                             GetDeviceCaps (pd.hDC, LOGPIXELSX), GetDeviceCaps (pd.hDC, LOGPIXELSY)));

    for (const PrintPagination::PageRange & pr : pages)
    {
        PaperRenderer            renderer;
        PaperRenderer::Options   opt;
        RgbaImage                img;

        opt.outputDpi = PrintDpiFromPrefs (prefs);
        opt.style     = PrintDotStyleFromPrefs (prefs);

        hr = renderer.Render (raster, pr.firstRow, pr.lastRow, opt, img);
        CHRF (hr, LogPrinter (std::format (L"page {} render failed, hr=0x{:08X}", pageIx, (uint32_t) hr)));

        CBRFEx (StartPage (pd.hDC) > 0, E_FAIL,
                LogPrinter (std::format (L"page {} StartPage failed, GetLastError={}", pageIx, ::GetLastError ())));

        hr = BlitRgbaToDc (pd.hDC, img, pageW, pageH);
        CHRF (hr, LogPrinter (std::format (L"page {} blit failed, hr=0x{:08X}", pageIx, (uint32_t) hr)));

        CBRFEx (EndPage (pd.hDC) > 0, E_FAIL,
                LogPrinter (std::format (L"page {} EndPage failed, GetLastError={}", pageIx, ::GetLastError ())));

        pageIx++;
    }

    CBRFEx (EndDoc (pd.hDC) > 0, E_FAIL,
            LogPrinter (std::format (L"EndDoc failed, GetLastError={}", ::GetLastError ())));
    started = false;
    LogPrinter (std::format (L"deliver: success ({} page(s) sent)", pages.size ()));

Error:
    if (FAILED (hr))
    {
        LogPrinter (std::format (L"deliver: FAILED, hr=0x{:08X}", (uint32_t) hr));
    }

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
//  CopyPrintoutToClipboard
//
//  Copies the finished strip to the clipboard as one continuous image (the
//  fanfold metaphor -- no pagination). Offers CF_DIB (bottom-up BGRA) for
//  classic paste targets and a registered "PNG" blob for apps that prefer
//  lossless. Render/encode are core (unit-tested); only the DIB packing and
//  Win32 clipboard calls live here. Does not consume the strip.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::CopyPrintoutToClipboard (const PrintRaster & raster)
{
    HRESULT                  hr       = S_OK;
    const GlobalUserPrefs &  prefs    = m_shell.m_globalPrefs;
    PaperRenderer            renderer;
    PaperRenderer::Options   opt;
    RgbaImage                img;
    vector<Byte>             png;
    HGLOBAL                  hDib     = nullptr;
    HGLOBAL                  hPng     = nullptr;
    bool                     opened   = false;
    size_t                   px       = 0;
    size_t                   dibBytes = 0;
    // A 32bpp DIB of the whole strip must stay bounded so a huge multi-page
    // banner cannot try to place gigabytes on the clipboard. Above the cap we
    // skip the bitmap and rely on the (compressed) PNG blob instead.
    const size_t             kMaxDibBytes = (size_t) 256 * 1024 * 1024;

    // Render the whole strip exactly once, capping dpi for very tall banners so
    // neither the DIB below nor the PNG blob materializes gigabytes. The source
    // is only 160x144 dpi, so the cap is effectively lossless.
    opt.outputDpi = WholeStripDpi (prefs, raster.RowsUsed ());
    opt.style     = PrintDotStyleFromPrefs (prefs);

    hr = renderer.Render (raster, 0, raster.RowsUsed () - 1, opt, img);
    CHR (hr);
    CBREx (img.width > 0 && img.height > 0, E_FAIL);

    px       = (size_t) img.width * img.height;
    dibBytes = sizeof (BITMAPINFOHEADER) + px * 4;

    if (dibBytes <= kMaxDibBytes)
    {
        Byte *             dest = nullptr;
        BITMAPINFOHEADER   bih  = {};

        hDib = GlobalAlloc (GMEM_MOVEABLE, dibBytes);
        CPR (hDib);

        dest = (Byte *) GlobalLock (hDib);
        CPR (dest);

        bih.biSize        = sizeof (bih);
        bih.biWidth       = img.width;
        bih.biHeight      = img.height;   // positive == bottom-up rows
        bih.biPlanes      = 1;
        bih.biBitCount    = 32;
        bih.biCompression = BI_RGB;
        bih.biSizeImage   = (DWORD) (px * 4);

        memcpy (dest, &bih, sizeof (bih));
        dest += sizeof (bih);

        // Bottom-up DIB: emit rows last-to-first, swapping RGBA -> BGRA.
        for (int y = img.height - 1; y >= 0; y--)
        {
            const Byte * src = &img.rgba[(size_t) y * img.width * 4];

            for (int x = 0; x < img.width; x++)
            {
                dest[x * 4 + 0] = src[x * 4 + 2];   // B
                dest[x * 4 + 1] = src[x * 4 + 1];   // G
                dest[x * 4 + 2] = src[x * 4 + 0];   // R
                dest[x * 4 + 3] = src[x * 4 + 3];   // A
            }

            dest += (size_t) img.width * 4;
        }

        GlobalUnlock (hDib);
    }

    // Encode the PNG from the image we already rendered rather than rendering
    // the strip a second time (the old path doubled peak memory on big banners).
    if (SUCCEEDED (PngCodec::EncodeRgba (img, opt.outputDpi, png)) && !png.empty ())
    {
        Byte *   dest = nullptr;

        hPng = GlobalAlloc (GMEM_MOVEABLE, png.size ());

        if (hPng != nullptr)
        {
            dest = (Byte *) GlobalLock (hPng);

            if (dest != nullptr)
            {
                memcpy (dest, png.data (), png.size ());
                GlobalUnlock (hPng);
            }
            else
            {
                GlobalFree (hPng);
                hPng = nullptr;
            }
        }
    }

    CBREx (hDib != nullptr || hPng != nullptr, E_FAIL);

    CBREx (OpenClipboard (m_shell.m_hwnd), E_FAIL);
    opened = true;
    CBREx (EmptyClipboard (), E_FAIL);

    // On success the clipboard takes ownership, so null the handle to keep the
    // cleanup path from freeing it out from under the clipboard.
    if (hDib != nullptr && SetClipboardData (CF_DIB, hDib) != nullptr)
    {
        hDib = nullptr;
    }

    if (hPng != nullptr)
    {
        UINT   fmt = RegisterClipboardFormatW (L"PNG");

        if (fmt != 0 && SetClipboardData (fmt, hPng) != nullptr)
        {
            hPng = nullptr;
        }
    }

Error:
    if (opened)          { CloseClipboard (); }
    if (hDib != nullptr) { GlobalFree (hDib); }
    if (hPng != nullptr) { GlobalFree (hPng); }
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterCommand
//
//  Handle the strip-level printer commands. Print / Save / Copy are all
//  NON-DESTRUCTIVE: they deliver the current strip and leave the paper in
//  the printer, so one printout can be printed AND saved AND copied. Discard
//  is the one explicit tear-off (confirmed, clears the persisted pending
//  copy, FR-029). All drain the ring first so they act on the complete
//  strip, reading the raster from a quiesced worker. Print always targets a
//  Windows printer; Save always writes a PNG through the file dialog -- the
//  destination is chosen by which command runs, not a stored preference.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnPrinterCommand (int id)
{
    HRESULT        hr   = S_OK;
    PrinterJob *   job  = nullptr;
    fs::path       file;

    if ((id != IDM_PRINTER_DISCARD && id != IDM_PRINTER_COPY &&
         id != IDM_PRINTER_PRINT && id != IDM_PRINTER_SAVEAS) ||
        m_shell.m_refs.printerCard == nullptr)
    {
        return;
    }

    bool   discard = (id == IDM_PRINTER_DISCARD);
    bool   copy    = (id == IDM_PRINTER_COPY);
    bool   print   = (id == IDM_PRINTER_PRINT);
    bool   saveAs  = (id == IDM_PRINTER_SAVEAS);

    // Take ownership of the strip: stop the worker, then flush any tail bytes.
    // Every strip-level command acts on the whole page, so we drain first and
    // read the job's raster from a quiesced worker (no concurrent mutation).
    m_shell.m_printerWorker.Stop ();

    {
        vector<PrinterEvent>   events;
        m_shell.m_printerWorker.FlushNow (events);
    }

    job = m_shell.m_printerWorker.Job ();

    if (job == nullptr || !job->HasContent ())
    {
        const wchar_t * emptyMsg = copy    ? L"The printer has no page to copy yet."
                                 : discard ? L"The printer has no page to discard."
                                 : print   ? L"The printer has no page to print yet."
                                           : L"The printer has no page to save yet.";

        DxuiMessageBox (m_shell.PrinterDialogOwner (), &m_shell.m_chromeTheme, emptyMsg, L"Casso Printer", MB_OK | MB_ICONINFORMATION);
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing ());
        return;
    }

    if (copy)
    {
        hr = CopyPrintoutToClipboard (job->Raster ());

        // Copy never consumes the strip: resume on the same page regardless.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing (), job->Raster ());

        if (FAILED (hr))
        {
            DxuiMessageBox (m_shell.PrinterDialogOwner (), &m_shell.m_chromeTheme, L"Could not copy the printout to the clipboard.",
                         L"Casso Printer", MB_OK | MB_ICONWARNING);
        }
        return;
    }

    if (discard)
    {
        // Tear off and throw away the current page (FR-029). Confirm first --
        // there is no undo -- and default the dialog to "No" so a stray Enter
        // never destroys a page.
        int   choice = DxuiMessageBox (
            m_shell.PrinterDialogOwner (),
            &m_shell.m_chromeTheme,
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

        // Confirmed: play the tear-off (a random paper-tear), start a fresh
        // sheet, and drop the persisted pending copy.
        m_shell.m_printerAudio.PlayTearOff ();
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing ());
        PrintJobStore::Clear (m_shell.PendingPrintDir ());
        return;
    }

    // Print -> Windows printer; Save -> PNG via file dialog. Both are
    // non-destructive: the paper stays so it can be delivered again.
    if (print)
    {
        hr = PrintToWindowsPrinter (job->Raster ());
    }
    else
    {
        hr = SavePrintoutAs (job->Raster (), file);
    }

    if (hr == S_FALSE)
    {
        // User cancelled the print / save dialog: keep the strip, no clear.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing (), job->Raster ());
        return;
    }

    if (SUCCEEDED (hr))
    {
        std::wstring   msg = print
                                 ? std::wstring (L"Sent the printout to the printer.")
                                 : (L"Saved printout to:\n" + file.wstring ());

        DxuiMessageBox (m_shell.PrinterDialogOwner (), &m_shell.m_chromeTheme, msg.c_str (), L"Casso Printer", MB_OK | MB_ICONINFORMATION);

        // Non-destructive: keep the paper so it can also be saved / printed.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->ByteRing (), job->Raster ());
    }
    else
    {
        DxuiMessageBox (m_shell.PrinterDialogOwner (), &m_shell.m_chromeTheme, L"Could not deliver the printout; the page is kept.",
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
            def.body.push_back ({ L"\n\nImageWriter II printer sounds by Scott Lawrence (CC BY 4.0)",
                                  true, L"https://github.com/BleuLlama/ImageWriterIISimulator" });
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
