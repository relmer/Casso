#include "Pch.h"

#include "WindowCommandManager.h"

#include "../AssetBootstrap.h"
#include "../Config/WindowPlacementProfile.h"
#include "../EmulatorShell.h"
#include "../resource.h"
#include "../Shell/DiskMru.h"
#include "Devices/Disk/BlankDiskBuilder.h"
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
#include "Core/UnicodeSymbols.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/Dialogs/CreateDiskDialog.h"
#include "Ui/FileBrowseModel.h"
#include "Shell/CpuManager.h"
#include "Shell/DiskManager.h"
#include "Shell/MachineManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope helpers
//
//  Error-message plumbing shared by the command handlers: turning an HRESULT
//  or a spool result into something a dialog can show.
//
//  Every user-facing failure in this file is reported as a friendly sentence
//  plus a technical trailer. The sentence tells the user what went wrong; the
//  trailer carries the raw code and the OS text, which is what actually
//  survives being pasted into a bug report. Neither alone is enough -- a bare
//  hex code is useless to the user, and a friendly sentence is useless to
//  whoever has to fix it.
//
//  A Win32-wrapped HRESULT is unwrapped before lookup, so a code produced by
//  HRESULT_FROM_WIN32 resolves to its GetLastError text rather than to
//  nothing. Codes the system has no message for degrade to just the hex, which
//  is still the useful half.
//
////////////////////////////////////////////////////////////////////////////////

using namespace ChromeMetrics;

// Turn a failure HRESULT into a "0xXXXXXXXX -- <system text>" detail line for
// the error dialog: the friendly sentence is for humans; this trailer is the
// hr + OS message for nerds (and bug reports). A Win32-wrapped code
// (HRESULT_FROM_WIN32) resolves to its GetLastError text; other HRESULTs
// resolve where the system has a message and degrade to just the hex code.
std::wstring  WindowCommandManager::FormatSystemError (HRESULT hr)
{
    LPWSTR         text   = nullptr;



    std::wstring   detail = std::format (L"0x{:08X}", (uint32_t) hr);
    DWORD          code   = (HRESULT_FACILITY (hr) == FACILITY_WIN32)
                                ? (DWORD) HRESULT_CODE (hr)
                                : (DWORD) hr;

    DWORD  n = FormatMessageW (
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR) &text, 0, nullptr);

    if (n != 0 && text != nullptr)
    {
        std::wstring   sys (text);

        // Trim the trailing ". \r\n" the system appends.
        while (!sys.empty() &&
               (sys.back() == L'\r' || sys.back() == L'\n' || sys.back() == L'.' || sys.back() == L' '))
        {
            sys.pop_back();
        }

        if (!sys.empty()) { detail += L" -- " + sys; }
    }

    if (text != nullptr) { LocalFree (text); }

    return detail;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HrFromSpoolResult
//
//  StartPage / EndPage / EndDoc report failure as a return value <= 0 using
//  the SP_* family, and frequently do NOT set GetLastError -- which is how a
//  print failure used to degrade to "Unspecified error" (or worse: CWRF's
//  HRESULT_FROM_WIN32(0) == S_OK, a silent false success). Map the result to
//  an honest HRESULT: a user abort (the Print-to-PDF Save-As cancel can
//  surface mid-job on modern Windows, not just at StartDoc) becomes
//  HRESULT_FROM_WIN32 (ERROR_CANCELLED), which names itself at the call
//  site; disk-full / out-of-memory get their real codes; anything else uses
//  GetLastError when present and only degrades to E_FAIL when the driver
//  reported nothing at all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::HrFromSpoolResult (int ret, const wchar_t * call, int pageIx)
{
    HRESULT   hr  = S_OK;
    DWORD     gle = ::GetLastError();   // capture before logging can clobber it



    if (ret <= 0)
    {
        if      (ret == SP_USERABORT)                                    { hr = HRESULT_FROM_WIN32 (ERROR_CANCELLED); }
        else if (ret == SP_APPABORT)                                     { hr = E_ABORT;                              }
        else if (ret == SP_OUTOFDISK)                                    { hr = HRESULT_FROM_WIN32 (ERROR_DISK_FULL); }
        else if (ret == SP_OUTOFMEMORY)                                  { hr = E_OUTOFMEMORY;                        }
        else if (gle == ERROR_CANCELLED || gle == ERROR_PRINT_CANCELLED) { hr = HRESULT_FROM_WIN32 (ERROR_CANCELLED); }
        else if (gle != 0)                                               { hr = HRESULT_FROM_WIN32 (gle);             }
        else                                                             { hr = E_FAIL;                               }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrimeDefaultPrinterDriver
//
//  Load ("prime") the default printer's driver on a short-lived no-COM (MTA)
//  thread. v4 / XPS print drivers -- notably Microsoft Print to PDF -- create
//  their device context through cross-process COM that aborts with 995
//  (ERROR_OPERATION_ABORTED) when first touched from our STA UI thread at
//  Medium integrity, yet succeeds from an MTA thread. Doing one MTA CreateDC
//  loads the driver so a subsequent PrintDlg on the UI thread creates its DC
//  normally. PD_RETURNDEFAULT gives us the default printer with no UI and no
//  DC of its own. Best-effort; any failure is ignored (the real path still
//  has its own fallback).
//
////////////////////////////////////////////////////////////////////////////////

void  WindowCommandManager::PrimeDefaultPrinterDriver()
{
    PRINTDLGW   def = {};



    def.lStructSize = sizeof (def);
    def.Flags       = PD_RETURNDEFAULT | PD_NOPAGENUMS | PD_NOSELECTION;

    if (PrintDlgW (&def) && def.hDevNames != nullptr)
    {
        std::wstring       driver;
        std::wstring       device;
        const DEVNAMES *   dnp = (const DEVNAMES *) GlobalLock (def.hDevNames);

        if (dnp != nullptr)
        {
            const wchar_t *  b = (const wchar_t *) dnp;
            driver = b + dnp->wDriverOffset;
            device = b + dnp->wDeviceOffset;
            GlobalUnlock (def.hDevNames);
        }

        std::thread ([driver, device] ()
        {
            HDC  h = CreateDCW (driver.c_str(), device.c_str(), nullptr, nullptr);
            if (h != nullptr) { DeleteDC (h); }
        }).join();
    }

    if (def.hDevMode  != nullptr) { GlobalFree (def.hDevMode); }
    if (def.hDevNames != nullptr) { GlobalFree (def.hDevNames); }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateDcFromDevNames
//
//  Build a printer DC from the DEVNAMES + DEVMODE the print dialog returned,
//  when PrintDlg's own PD_RETURNDC came back null (a v4 driver flaking on the
//  STA UI thread). Created on a plain (no-COM / MTA) thread for the same
//  reason priming works -- an STA CreateDC aborts (995) where an MTA one
//  succeeds. NULL port: the Print-to-PDF file prompt belongs at StartDoc, not
//  DC creation. Returns null only if the names are missing or CreateDC fails.
//
////////////////////////////////////////////////////////////////////////////////

HDC  WindowCommandManager::CreateDcFromDevNames (const PRINTDLGW & pd)
{
    std::wstring                 driver;
    std::wstring                 device;
    std::vector<unsigned char>   devmode;
    HDC                          hdc = nullptr;



    // The driver / device names are mandatory: without them there is nothing
    // to hand CreateDC. Note the GlobalUnlock is paired inside the same
    // branch that locked -- a bare return between the two would leak the lock.
    if (pd.hDevNames != nullptr)
    {
        const DEVNAMES *  dn = (const DEVNAMES *) GlobalLock (pd.hDevNames);

        if (dn != nullptr)
        {
            const wchar_t *  base = (const wchar_t *) dn;

            driver = base + dn->wDriverOffset;
            device = base + dn->wDeviceOffset;
            GlobalUnlock (pd.hDevNames);
        }
    }

    // Copy the (variable-length) DEVMODE so the worker thread owns stable
    // memory independent of the caller's global handle lock.
    if (!device.empty() && pd.hDevMode != nullptr)
    {
        const DEVMODEW *  dm = (const DEVMODEW *) GlobalLock (pd.hDevMode);

        if (dm != nullptr)
        {
            const unsigned char *  p  = (const unsigned char *) dm;
            size_t                 sz = (size_t) dm->dmSize + dm->dmDriverExtra;

            devmode.assign (p, p + sz);
            GlobalUnlock (pd.hDevMode);
        }
    }

    // A null DEVMODE is fine (driver defaults); an empty device name is not.
    if (!device.empty())
    {
        std::thread ([&] ()
        {
            const DEVMODEW *  dmp = devmode.empty() ? nullptr : (const DEVMODEW *) devmode.data();

            hdc = CreateDCW (driver.c_str(), device.c_str(), nullptr, dmp);
        }).join();
    }

    return hdc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlitRgbaToDc
//
//  Blit an R,G,B,A image onto a printer HDC. The strip is scaled to fit the
//  page WIDTH (uniform scale, aspect preserved) and top-aligned so the
//  fanfold continues downward across page breaks. Fitting to width -- not
//  min(width,height) -- is deliberate: the strip width is identical on every
//  page, so a width fit gives every page the same horizontal scale and left
//  edge. A min() fit would height-limit full pages but width-limit the short
//  last page, scaling their columns differently and misaligning page-to-page.
//  GDI DIBs are BGRA, so the channels are swapped into a scratch buffer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::BlitRgbaToDc (HDC hdc, const RgbaImage & img, int pageW, int pageH, int outputDpi)
{
    HRESULT                    hr    = S_OK;
    vector<Byte>               bgra;
    BITMAPINFO                 bmi   = {};
    size_t                     count = 0;
    size_t                     i     = 0;
    PrintPagination::PageFit   fit;
    int                        destW = 0;
    int                        destH = 0;
    int                        blit  = 0;



    CBR (img.width > 0 && img.height > 0);
    CBR (pageW > 0 && pageH > 0);

    count = (size_t) img.width * img.height;
    bgra.resize (count * 4);
    for (i = 0; i < count; i++)
    {
        bgra[i * 4 + 0] = img.rgba[i * 4 + 2];   // B
        bgra[i * 4 + 1] = img.rgba[i * 4 + 1];   // G
        bgra[i * 4 + 2] = img.rgba[i * 4 + 0];   // R
        bgra[i * 4 + 3] = img.rgba[i * 4 + 3];   // A
    }

    // Fit a FULL page to the printable height, capped by width, at one uniform
    // scale (device pixels, so the output dpi is the box's vertical unit). The
    // shared fanfold fit, so classic print, modern print and preview all agree.
    fit   = PrintPagination::FitFullPageToBox ((double) img.width, (double) img.height,
                                               (double) pageW, (double) pageH, (double) outputDpi);
    destW = (std::max) (1, (int) (img.width  * fit.scale));
    destH = (std::max) (1, (int) (img.height * fit.scale));

    bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = img.width;
    bmi.bmiHeader.biHeight      = -img.height;   // negative == top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode (hdc, HALFTONE);
    SetBrushOrgEx     (hdc, 0, 0, nullptr);

    // Failure is GDI_ERROR (-1 as an int) *or* zero scan lines copied (the
    // destination always spans >= 1 line, so a genuine success copies at
    // least one) -- blit <= 0 covers both. GDI often reports these with
    // GetLastError()==0; CWRF would turn that into HRESULT_FROM_WIN32(0)
    // == S_OK -- a silent false success -- so fall back to E_FAIL
    // explicitly when there is no error code to keep.
    blit = StretchDIBits (hdc,
                          (pageW - destW) / 2, 0, destW, destH,
                          0, 0, img.width, img.height,
                          bgra.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
    if (blit <= 0)
    {
        DWORD   gle = ::GetLastError();

        hr = (gle != 0) ? HRESULT_FROM_WIN32 (gle) : E_FAIL;
    }

Error:
    return hr;
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
//  Routes a WM_COMMAND id to the handler for its menu.
//
//  Dispatch is by RANGE, not by a per-id table, which is why the IDM_ values
//  in the resource header are grouped and contiguous per menu: adding a
//  command to an existing menu needs only an id inside that menu's span and a
//  case in its handler, with nothing to update here.
//
//  The ids handled individually are the ones that belong to no menu -- printer
//  toolbar actions and the two async print-result notifications the worker
//  posts back -- so they have no range to fall into.
//
//  Always returns false: the shell reports every command as not fully handled
//  so default processing still runs. The return is not a "was it recognized"
//  signal, and an unrecognized id is silently ignored rather than asserted,
//  since WM_COMMAND also carries ids from system menus and accelerators.
//
////////////////////////////////////////////////////////////////////////////////

bool WindowCommandManager::OnCommand (HWND hwnd, int id)
{
    UNREFERENCED_PARAMETER (hwnd);

    if      (id >= IDM_EDIT_COPY_TEXT && id <= IDM_EDIT_PASTE)              { OnEditCommand (id); }
    else if (id >= IDM_FILE_OPEN      && id <= IDM_FILE_EXIT)               { OnFileCommand (id); }
    else if (id >= IDM_MACHINE_RESET  && id <= IDM_MACHINE_ARROWS_PADDLE)   { OnMachineCommand (id); }
    else if (id >= IDM_DISK_INSERT1   && id <= IDM_DISK_WP2)                { OnDiskCommand (id); }
    else if (id >= IDM_VIEW_COLOR     && id <= IDM_VIEW_SETTINGS)           { OnViewCommand (id); }
    // The View range above stops at IDM_VIEW_SETTINGS, so every View command
    // added since needs naming here. A new id that falls outside every branch
    // is dropped in SILENCE -- the menu item paints, the accelerator fires,
    // and nothing happens -- so these one-offs are load-bearing, not clutter.
    else if (id == IDM_VIEW_DRIVE_STRIP)                                   { OnViewCommand (id); }
    else if (id == IDM_VIEW_RESET_SCENE)                                   { OnViewCommand (id); }
    else if (id == IDM_VIEW_FRAME_RATE)                                    { OnViewCommand (id); }
    else if (id == IDM_VIEW_SCENE_VIEW)                                    { OnViewCommand (id); }
    else if (id == IDM_PRINTER_DISCARD)                                    { OnPrinterCommand (id); }
    else if (id == IDM_PRINTER_COPY)                                       { OnPrinterCommand (id); }
    else if (id == IDM_PRINTER_PRINT)                                      { OnPrinterCommand (id); }
    else if (id == IDM_PRINTER_SAVEAS)                                     { OnPrinterCommand (id); }
    else if (id == IDM_PRINTER_MODERN_SENT)                                { OnModernPrintResult (true); }
    else if (id == IDM_PRINTER_MODERN_FAILED)                              { OnModernPrintResult (false); }
    else if (id == IDM_PRINTER_PREVIEW)                                    { m_shell.ShowPrinterPanel(); }
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
            // side effect (SyncInputModeUi -> SyncSelectorState).
            m_shell.SetPointerMapping (InputMappingMode::Off);
        }
        else
        {
            // Availability changed with no mode change (reconnect, or a
            // disconnect while not in Mouse mode): refresh the selector so
            // the Mouse segment reappears / disappears -- SetState flips the
            // availability flag and the toolbar rebuilds the 2<->3 segment
            // geometry + hit map. UI-thread routed (posted WM_COMMAND).
            m_shell.SyncSelectorState();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnExternalDriveCommand
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnExternalDriveCommand (int id)
{
    bool  connected = (id == IDM_DRIVE_EXTERNAL_CONNECT);
    bool  fChanged  = false;



    if (connected != m_shell.m_externalDriveConnected)
    {
        m_shell.m_externalDriveConnected = connected;
        fChanged = true;
    }

    // A carded machine's answer lives in the card's second connector, so the
    // RUNNING config has to move too -- ShouldShowExternalDrive reads the
    // attached count from there for anything that is not a //c, and would
    // otherwise keep reporting the old one however the flag above is set.
    if (m_shell.m_config.SetDiskIiPortAttached (1, connected))
    {
        fChanged = true;
    }

    if (!fChanged)
    {
        return;
    }

    // A detached drive cannot still be holding a disk. Ejecting flushes it
    // through DiskImageStore, so pulling the drive does not quietly strand
    // unwritten changes in an image the user can no longer reach -- and it
    // routes through the CPU command queue like every other eject, rather
    // than tearing state out from under the running machine.
    if (!connected && m_shell.m_diskManager != nullptr)
    {
        m_shell.m_diskManager->Eject (6, 1);
    }

    m_shell.ReflowChromeForMachineChange();
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
            m_shell.m_clipboardManager->CopyScreenText (m_shell.m_hwnd, m_shell.GetAuxRamBuffer());
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
//  The Machine menu: reset, power cycle, pause, step, speed, and the input
//  mapping toggles.
//
//  These commands do NOT all reach the CPU the same way, and the difference is
//  what to understand here.
//
//  Reset and power cycle are POSTED to the CPU thread's queue, because they
//  mutate device state the CPU thread is actively using and must land between
//  instructions rather than mid-execution.
//
//  Step is driven DIRECTLY from the UI thread, which looks like a violation of
//  that rule and is not. Step only runs while paused, and a paused CPU thread
//  is provably idle -- blocked in pauseCV.wait -- so there is no concurrent
//  access to race with. Posting it would in fact deadlock: the CPU thread
//  cannot drain its command queue while it is parked. It is delegated back
//  through the shell to keep Disk2Controller's full definition out of this
//  header.
//
//  Speed and pause are plain CpuManager calls -- atomics the CPU thread reads
//  each frame -- so they need no marshalling at all.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnMachineCommand (int id)
{
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
            m_shell.m_cpuManager.TogglePaused();
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
            DialogDefinition  def;

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

            def = {};
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
//  The View menu: monitor color, fullscreen, reset window size, the debug
//  panels, and Settings.
//
//  Color mode is stored with release ordering because the CPU thread reads it
//  while rendering; everything else here is UI-thread-only.
//
//  Reset-size is the substantial case. The new window size is derived by
//  MEASURING the current window's non-client overhead -- window rect minus
//  client rect -- rather than computing it with AdjustWindowRectExForDpi. The
//  custom WM_NCCALCSIZE handler does not match the stock calculation, so the
//  theoretical path landed on a wrong size and needed a follow-up correction,
//  which was visible as a jitter on every Ctrl+0. Neither style nor DPI change
//  between the measurement and the SetWindowPos, so the measured overhead is a
//  stable input and the window lands right the first time.
//
//  The target client area comes from GetClientSizeForFramebufferPx, which owns
//  the framebuffer scaling policy for the whole shell, so this command cannot
//  disagree with what a normal resize would produce.
//
//  The result is centered on the window's CURRENT monitor, not the primary, so
//  Ctrl+0 does not fling the window across a multi-monitor desktop. It is a
//  no-op while fullscreen, where the size is not the window's to choose.
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
            RECT  rcClient = {};

            m_shell.m_d3dRenderer.ToggleFullscreen (m_shell.m_hwnd);

            // The transition's WM_SIZE runs while the fullscreen flag is
            // deliberately still held (placement persistence), so the layout
            // it triggered used the OLD presentation. Re-run it now that the
            // flag reflects the final state -- the desk scene's fullscreen
            // branch keys off it.
            if (m_shell.m_hwnd != nullptr && GetClientRect (m_shell.m_hwnd, &rcClient))
            {
                (void) m_shell.OnSize ((UINT) (rcClient.right - rcClient.left),
                                       (UINT) (rcClient.bottom - rcClient.top));
                m_shell.m_d3dRenderer.MarkRedrawNeeded();
            }

            break;
        }

        case IDM_VIEW_RESET_SCENE:
        {
            m_shell.ResetSceneView();
            break;
        }

        case IDM_VIEW_FRAME_RATE:
        {
            // Persisted, so the choice outlives the session in either build.
            m_shell.m_globalPrefs.showFrameRate = !m_shell.m_globalPrefs.showFrameRate;
            m_shell.SaveGlobalPrefs();
            break;
        }

        case IDM_VIEW_SCENE_VIEW:
        {
            m_shell.m_globalPrefs.showSceneView = !m_shell.m_globalPrefs.showSceneView;
            m_shell.SaveGlobalPrefs();
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


                // Leave the maximized state before sizing. SetWindowPos on a
                // zoomed window moves and resizes it but leaves WS_MAXIMIZE
                // set, so the window keeps claiming to be maximized: the
                // caption still offers Restore, and the next Restore snaps
                // back to a stale rect. Restoring first also makes the
                // non-client measurement below describe the presentation the
                // window is about to have.
                if (IsZoomed (m_shell.m_hwnd))
                {
                    ShowWindow (m_shell.m_hwnd, SW_RESTORE);
                }

                // Target client area: framebuffer at the current DPI
                // (linear scale), with the chrome band insets summed by
                // the single source of truth. EmulatorShell::
                // GetClientSizeForFramebufferPx owns the framebuffer scale
                // policy -- see it for the one-line toggle to switch to
                // integer-only scaling.
                {
                    SIZE  desired = m_shell.GetClientSizeForFramebufferPx (
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

                // The 100%-emulator framing can ask for a window larger than
                // the display -- more so now that the scene is a full desk
                // rather than a bare framebuffer. Fit it to the work area
                // and let the placement rule hold the caption's top-left on
                // screen; centering is what gives way, not reachability.
                // Without this the oversized window was centered on the work
                // area, which put its top-left off the top-left of it: a
                // window the pointer could no longer grab.
                hMon = MonitorFromWindow (m_shell.m_hwnd, MONITOR_DEFAULTTONEAREST);

                if (hMon != nullptr && GetMonitorInfo (hMon, &mi))
                {
                    RECT  placed = WindowPlacementProfile::FitToWorkArea (mi.rcWork, w, h);

                    x = (int) placed.left;
                    y = (int) placed.top;
                    w = (int) (placed.right  - placed.left);
                    h = (int) (placed.bottom - placed.top);

                    SetWindowPos (m_shell.m_hwnd, nullptr, x, y, w, h, SWP_NOZORDER);
                }
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

        case IDM_VIEW_DRIVE_STRIP:
        {
            // Only meaningful in fullscreen; the FSM consumes the edge on its
            // next tick (releasing a guest capture if one is held). Windowed,
            // the drives are already on screen.
            if (m_shell.m_d3dRenderer.IsFullscreen())
            {
                m_shell.m_stripHotkeyPending = true;
                m_shell.m_d3dRenderer.MarkRedrawNeeded();
            }

            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptForDiskImage
//
//  Shows the file picker and mounts whatever the user chose into the given
//  drive.
//
//  A CANCEL is reported as success. Backing out of a picker is a decision, not
//  a failure -- there is simply nothing to mount -- and the sole caller only
//  tests FAILED, so returning a distinct code would have told nobody anything
//  while inviting a spurious error dialog.
//
//  The drive number is converted from the menu's 1-based numbering to the
//  controller's 0-based index here, so the command handlers can keep speaking
//  in the numbers printed on the drives.
//
//  The path buffer is shell-allocated and is freed on every exit, including
//  the failure paths that never reach the mount.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::PromptForDiskImage (int drive, bool & outMountStarted)
{
    HRESULT                          hr         = S_OK;
    ComPtr<IFileOpenDialog>          dialog;
    ComPtr<IShellItem>               item;
    PWSTR                            pszPath    = nullptr;
    COMDLG_FILTERSPEC                filters[2] = { { L"Disk images", L"*.dsk;*.do;*.woz;*.po" },
                                                    { L"All files",   L"*.*" } };



    outMountStarted = false;

    hr = CoCreateInstance (CLSID_FileOpenDialog,
                           nullptr,
                           CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS (&dialog));
    CHR (hr);

    hr = dialog->SetFileTypes (std::size (filters), filters);
    CHR (hr);

    hr = dialog->Show (m_shell.m_hwnd);

    // Backing out of the file picker means there is nothing to mount -- not
    // an error, so it normalizes to S_OK; outMountStarted (still false) is
    // what tells the caller no mount happened.
    BAIL_OUT_IF (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED), S_OK);
    CHR (hr);

    hr = dialog->GetResult (&item);
    CHR (hr);

    hr = item->GetDisplayName (SIGDN_FILESYSPATH, &pszPath);
    CHR (hr);

    hr = m_shell.Mount (6, drive - 1, pszPath);
    CHR (hr);

    outMountStarted = true;

Error:
    if (pszPath != nullptr)
    {
        CoTaskMemFree (pszPath);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateBlankDiskForDrive
//
//  The create-a-blank-disk flow: a themed save-style dialog over a
//  FileBrowseModel picks the target; BlankDiskBuilder produces the image
//  bytes; the file lands through the filesystem's atomic temp+rename write;
//  and the standard Mount path takes it from there (drive widget, MRU,
//  persistence, door choreography all behave as for any insert).
//
//  Every backing-out path -- cancel, refused target, replace declined --
//  returns S_OK with no mount started, so the caller's door choreography
//  restores the drive. A failed build or write reports the cause and leaves
//  the prior mount and the host filesystem untouched (the atomic write can
//  never leave a partial image).
//
//  The default folder is Documents\Casso Disks, created on demand; the
//  dialog's model refuses any target currently mounted in a drive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::CreateBlankDiskForDrive (int drive, bool & outMountStarted)
{
    HRESULT                    hr        = S_OK;
    PWSTR                      docsRaw   = nullptr;
    int                        choice    = IDYES;
    bool                       occupied  = false;
    std::wstring               folder;
    std::wstring               message;
    std::vector<std::wstring>  mountedPaths;
    std::vector<int>           mountedDrives;
    vector<Byte>               imageBytes;
    std::string                imageContent;
    std::error_code            ec;
    BootPayload                payload;
    FileBrowseModel            model;
    CreateDiskDialog           dialog;
    DxuiWindow::CreateParams   params;



    outMountStarted = false;

    // The last create's folder wins while it still exists; otherwise the
    // default Documents\Casso Disks, created on demand.
    if (!m_shell.m_globalPrefs.lastDiskCreateFolder.empty())
    {
        const std::string &  stored = m_shell.m_globalPrefs.lastDiskCreateFolder;
        std::u8string        u8     (reinterpret_cast<const char8_t *> (stored.data()),
                                     stored.size());
        std::wstring         last   = std::filesystem::path (u8).wstring();

        if (std::filesystem::exists (last, ec))
        {
            folder = last;
        }
    }

    if (folder.empty())
    {
        hr = SHGetKnownFolderPath (FOLDERID_Documents, 0, nullptr, &docsRaw);
        CHRA (hr);

        folder = docsRaw;
        CoTaskMemFree (docsRaw);
        docsRaw = nullptr;

        folder += L"\\Casso Disks";
        std::filesystem::create_directories (folder, ec);
    }

    // The model refuses a target that is currently mounted in any drive; the
    // store's backing paths are UTF-8 and go wide through the same u8string
    // interpretation the MRU uses.
    for (const DiskImageStore::MountedSource & mounted : m_shell.m_diskStore.GetMountedSourcePaths())
    {
        std::u8string  u8 (reinterpret_cast<const char8_t *> (mounted.path.data()),
                           mounted.path.size());

        mountedPaths.push_back (std::filesystem::path (u8).wstring());
        mountedDrives.push_back (mounted.drive);
    }

    model.Bind (&m_shell.m_uiFs);
    model.SetExtensionFilter (L".woz");
    model.SetMountedPaths (std::move (mountedPaths), std::move (mountedDrives));

    hr = model.SetFolder (folder);
    CHRF (hr, DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chromeTheme,
                              L"Could not open the disk folder.",
                              L"Create New Disk", MB_OK | MB_ICONERROR));

    // Boot-payload plumbing: availability answers from the download cache;
    // the download callback runs on the dialog's explicit button click.
    dialog.Configure (&model, &m_shell.m_chromeTheme,
        [] (BlankDiskContents contents)
        {
            return AssetBootstrap::IsStockBootDiskCached (
                (contents == BlankDiskContents::ProDos)
                    ? AssetBootstrap::StockBootDisk::ProDosUsersDisk
                    : AssetBootstrap::StockBootDisk::Dos33Master);
        },
        [] (BlankDiskContents contents)
        {
            std::wstring  path;
            std::string   error;

            return AssetBootstrap::EnsureStockBootDisk (
                (contents == BlankDiskContents::ProDos)
                    ? AssetBootstrap::StockBootDisk::ProDosUsersDisk
                    : AssetBootstrap::StockBootDisk::Dos33Master,
                path, error);
        });

    params.title                    = std::format (L"Create New Disk (Drive {})", drive);
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = m_shell.m_hwnd;
    params.initialSizeDip           = { 560, 480 };
    params.minSizeDip               = { 420, 360 };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (&m_shell.m_chromeTheme);
    dialog.ShowModalDialog (IDOK);

    // Cancel / Escape / close box: nothing to do, and no mount started.
    BAIL_OUT_IF (!dialog.GetOutcome().confirmed, S_OK);

    // The target drive may already hold a disk; replacing it needs a yes.
    occupied = m_shell.m_diskStore.IsMounted (6, drive - 1);

    if (occupied)
    {
        message = std::format (L"Drive {} already has a disk. Replace it with the new disk?", drive);
        choice  = DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chromeTheme, message.c_str(),
                                  L"Create New Disk", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING);

        BAIL_OUT_IF (choice != IDYES, S_OK);
    }

    // A bootable spec needs the OS master's bytes. The dialog only enables
    // the toggle when the cache has them, but the file is re-read here so a
    // master that vanished in between reports instead of failing silently.
    if (dialog.GetOutcome().spec.bootable)
    {
        bool  isProDos = (dialog.GetOutcome().spec.contents == BlankDiskContents::ProDos);

        std::filesystem::path  masterPath = AssetBootstrap::GetStockBootDiskPath (
            isProDos ? AssetBootstrap::StockBootDisk::ProDosUsersDisk
                     : AssetBootstrap::StockBootDisk::Dos33Master);

        std::ifstream  master (masterPath, std::ios::binary);

        bool  opened = master.good();

        CBRF (opened, DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chromeTheme,
                                      L"The OS master disk is missing from the download cache.",
                                      L"Create New Disk", MB_OK | MB_ICONERROR));

        vector<Byte> &  dest = isProDos ? payload.proDosUsersDisk
                                        : payload.dosMasterSectors;

        dest.assign (std::istreambuf_iterator<char> (master),
                     std::istreambuf_iterator<char> ());
    }

    hr = BlankDiskBuilder::Build (dialog.GetOutcome().spec, payload, imageBytes);
    CHRF (hr, DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chromeTheme,
                              L"Could not build the new disk image.",
                              L"Create New Disk", MB_OK | MB_ICONERROR));

    // Atomic: the filesystem stages a sibling temp file and swaps it in, so
    // a failure here leaves no partial image behind.
    imageContent.assign (reinterpret_cast<const char *> (imageBytes.data()), imageBytes.size());

    hr = m_shell.m_uiFs.WriteAllText (dialog.GetOutcome().targetPath, imageContent);
    CHRF (hr, DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chromeTheme,
                              (L"Could not write \"" + dialog.GetOutcome().targetPath + L"\".\n\n"
                               + FormatSystemError (hr)).c_str(),
                              L"Create New Disk", MB_OK | MB_ICONERROR));

    // Remember where this disk landed (the user may have navigated away
    // from the starting folder); the next create opens there.
    {
        std::u8string  u8folder = std::filesystem::path (dialog.GetOutcome().targetPath)
                                      .parent_path().u8string();

        m_shell.m_globalPrefs.lastDiskCreateFolder.assign (u8folder.begin(), u8folder.end());
        m_shell.SaveGlobalPrefs();
    }

    hr = m_shell.Mount (6, drive - 1, dialog.GetOutcome().targetPath);
    CHRF (hr, DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chromeTheme,
                              L"The disk was created but could not be mounted.",
                              L"Create New Disk", MB_OK | MB_ICONERROR));

    outMountStarted = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptInsertDiskMru
//
//  Shows the themed disk MRU picker. Routes the user's chosen disk
//  (recent image or stock master download) to Mount(); if the user
//  clicks "Browse..." this falls through to the IFileOpenDialog path
//  via PromptForDiskImage. The pinned <Create new disk...> row routes to
//  CreateBlankDiskForDrive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::PromptInsertDiskMru (int drive, bool & outMountStarted)
{
    HRESULT                      hr            = S_OK;
    DiskMru                      mru;
    std::vector<DiskMru::Entry>  mruPruned;
    std::filesystem::path        diskDir;
    std::wstring                 chosenPath;
    std::string                  error;
    bool                         userBrowsed   = false;
    bool                         userCreateNew = false;



    outMountStarted = false;



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
                                              userCreateNew,
                                              error);
    CHR (hr);

    if (userBrowsed)
    {
        hr = PromptForDiskImage (drive, outMountStarted);
        CHR (hr);
    }
    else if (userCreateNew)
    {
        hr = CreateBlankDiskForDrive (drive, outMountStarted);
        CHR (hr);
    }
    else if (!chosenPath.empty())
    {
        hr = m_shell.Mount (6, drive - 1, chosenPath);
        CHR (hr);

        outMountStarted = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDiskCommand
//
//  The Disk menu: insert and eject, per drive.
//
//  Insert and eject take different routes for the same reason as reset versus
//  step in the Machine menu. Insert runs on the UI thread because it opens a
//  file picker -- a modal shell dialog that must be on the thread owning the
//  window -- and the mount it performs is itself queued internally. Eject is
//  POSTED to the CPU thread, since it detaches a disk the drive engine may be
//  actively reading.
//
//  Insert routes through BrowseForDisk rather than calling the picker
//  directly, so the menu gets the same drive-door choreography as a click
//  on the drive widget: door opens under the modal keep-alive while the
//  picker is up, closes back on cancel, and the picker path already
//  reports anything worth reporting.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnDiskCommand (int id)
{
    switch (id)
    {
        case IDM_DISK_INSERT1:
        case IDM_DISK_INSERT2:
        {
            m_shell.BrowseForDisk ((id == IDM_DISK_INSERT1) ? 0 : 1);
            break;
        }

        case IDM_DISK_EJECT1:
        case IDM_DISK_EJECT2:
        {
            m_shell.PostCommand (static_cast<WORD> (id));
            break;
        }

        case IDM_DISK_WP1:
        case IDM_DISK_WP2:
        {
            // The toggle runs on the CPU thread (like mount / eject) so its
            // flush never races the drive engine.
            m_shell.PostCommand (static_cast<WORD> (id));
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintDpiFromPrefs
//
//  Settings > Printing knobs, read live from GlobalUserPrefs at each eject.
//
////////////////////////////////////////////////////////////////////////////////

static int PrintDpiFromPrefs (const GlobalUserPrefs & p)
{
    return (p.printOutputDpi == 288) ? 288 : 576;   // only 288 / 576 valid (FR-028)
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintDotStyleFromPrefs
//
////////////////////////////////////////////////////////////////////////////////

static DotStyle PrintDotStyleFromPrefs (const GlobalUserPrefs & p)
{
    return (p.printDotStyle == "plain") ? DotStyle::Plain : DotStyle::Ink;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WholeStripDpi
//
//  Cap the render dpi for a WHOLE-strip render (PNG file, clipboard) so a long
//  fanfold banner's single RGBA image stays within a memory budget instead of
//  ballooning to gigabytes (each row at 576 dpi is 4608 px * 4 B; a 60-page
//  banner is ~95k rows). The native grid is only 160x144 dpi, so dropping a huge
//  banner from 576 toward ~150 dpi is still well above source resolution -- no
//  meaningful quality loss, and it never OOMs. Short jobs keep the full dpi.
//
////////////////////////////////////////////////////////////////////////////////

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
//  Cancellation is reported through outOutcome, not the result code.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::SavePrintoutAs (const PrintRaster & raster, fs::path & outFile, PrintOutcome & outOutcome)
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
    bool                      isOpen      = false;
    bool                      wroteWell   = false;
    HRESULT                   hrPictures  = S_OK;
    HRESULT                   hrItem      = S_OK;
    HRESULT                   hrFolder    = S_OK;
    std::error_code           ec;
    const GlobalUserPrefs &   prefs       = m_shell.m_globalPrefs;



    outOutcome = PrintOutcome::Delivered;

    static const COMDLG_FILTERSPEC   s_kFilters[] =
    {
        { L"PNG image", L"*.png" },
    };

    hr = CoCreateInstance (CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS (&dialog));
    CHR (hr);

    hr = dialog->SetFileTypes (std::size (s_kFilters), s_kFilters);
    CHR (hr);

    hr = dialog->SetDefaultExtension (L"png");
    CHR (hr);

    // Seed the default folder <Pictures>\Casso Prints + a timestamped name.
    hrPictures = SHGetKnownFolderPath (FOLDERID_Pictures, 0, nullptr, &picturesRaw);

    if (SUCCEEDED (hrPictures))
    {
        folder = fs::path (picturesRaw) / L"Casso Prints";
    }

    if (!folder.empty())
    {
        fs::create_directories (folder, ec);

        hrItem = SHCreateItemFromParsingName (folder.c_str(), nullptr,
                                              IID_PPV_ARGS (&folderItem));

        if (SUCCEEDED (hrItem))
        {
            // Best-effort: an unsettable start folder just means the dialog
            // opens wherever the shell last left it.
            hrFolder = dialog->SetFolder (folderItem.Get());
            IGNORE_RETURN_VALUE (hrFolder, S_OK);
        }
    }

    GetLocalTime (&now);
    suggested = PrintFileNaming::ComposePngPath (folder, now,
                    [] (const fs::path & p) { std::error_code e; return fs::exists (p, e); });

    hr = dialog->SetFileName (suggested.filename().c_str());
    CHR (hr);

    hr = dialog->Show (m_shell.GetPrinterDialogOwner());

    CHR (hr);

    hr = dialog->GetResult (&item);
    CHR (hr);

    hr = item->GetDisplayName (SIGDN_FILESYSPATH, &pszPath);
    CHR (hr);

    outFile = fs::path (pszPath);

    hr = PrintDelivery::RenderToPng (raster, 0, raster.GetRowsUsed() - 1,
                                     WholeStripDpi (prefs, raster.GetRowsUsed()),
                                     PrintDotStyleFromPrefs (prefs), png);
    CHR (hr);

    {
        std::ofstream   out (outFile, std::ios::binary | std::ios::trunc);

        isOpen = out.is_open();
        CBR (isOpen);

        out.write ((const char *) png.data(), (std::streamsize) png.size());

        wroteWell = out.good();
        CBR (wroteWell);
    }

Error:
    // A user cancel is not a delivery failure. Mapping it here rather than at
    // the exit itself keeps one owner for the rule.
    if (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        outOutcome = PrintOutcome::Canceled;
        hr         = S_OK;
    }

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
//  edge). Cancellation -- of the dialog, the Save-As
//  prompt inside StartDoc, or (modern Print-to-PDF) an abort surfacing at a
//  later spool call. On failure `failedStage` names the call that failed so
//  the error dialog's Details line pinpoints it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::PrintToWindowsPrinter (const PrintRaster & raster, std::wstring & failedStage, PrintOutcome & outOutcome)
{
    HRESULT                               hr       = S_OK;
    const GlobalUserPrefs               & prefs    = m_shell.m_globalPrefs;
    vector<PrintPagination::PageRange>    pages    = PrintPagination::Paginate (raster);
    PRINTDLGW                             pd       = {};
    DOCINFOW                              di       = {};
    bool                                  started  = false;
    BOOL                                  dlgOk    = FALSE;
    int                                   pageW    = 0;
    int                                   pageH    = 0;
    int                                   pageIx   = 0;
    bool                                  hasPages = false;



    outOutcome = PrintOutcome::Delivered;

    hasPages = !pages.empty();
    CBRF (hasPages,
          failedStage = L"pagination (the page has no printable content)");

    // Microsoft Print to PDF (and other v4 / XPS print drivers) create their
    // device context through cross-process COM that aborts with 995
    // (ERROR_OPERATION_ABORTED) the first time it is touched from our STA UI
    // thread (OleInitialize) at Medium integrity, but succeeds from a plain MTA
    // thread. Loading the driver once on a short-lived no-COM thread lets the
    // PrintDlg below then create its DC on the UI thread normally. Diagnosed
    // live: the STA CreateDC fails 995, an MTA CreateDC succeeds, and the real
    // PrintDlg then returns a valid DC and the job completes. Best-effort.
    PrimeDefaultPrinterDriver();

    pd.lStructSize = sizeof (pd);
    pd.hwndOwner   = m_shell.m_hwnd;
    pd.Flags       = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
    pd.nCopies     = 1;

    // A false return is overwhelmingly the user closing the dialog
    // (CommDlgExtendedError() == 0); the Error label turns ERROR_CANCELLED
    // into outOutcome = Canceled with a success code.
    dlgOk = PrintDlgW (&pd);
    BAIL_OUT_IF (!dlgOk, HRESULT_FROM_WIN32 (ERROR_CANCELLED));

    // PD_RETURNDC should have created the DC. When it flakes (TRUE + null hDC +
    // no error -- seen intermittently with Print-to-PDF in a long-lived
    // process), build the DC ourselves from the dialog's DEVNAMES/DEVMODE rather
    // than surfacing a spurious failure. We own this DC exactly as we would the
    // PrintDlg-created one (the Error cleanup DeleteDC's pd.hDC either way).
    if (pd.hDC == nullptr)
    {
        pd.hDC = CreateDcFromDevNames (pd);
    }

    CBRF (pd.hDC != nullptr,
            failedStage = L"PrintDlg (the chosen printer returned no device context)");

    di.cbSize      = sizeof (di);
    di.lpszDocName = L"Casso Printout";

    // "Microsoft Print to PDF" (and some drivers) pop a Save-As prompt inside
    // StartDoc; canceling it is a user cancel, not a delivery failure -- so it
    // reports exactly like a canceled print dialog (outOutcome = Canceled: keep the page, no
    // scary "could not deliver" popup). HrFromSpoolResult owns that mapping,
    // including the SP_* codes and the GetLastError()==0 case.
    hr = HrFromSpoolResult (StartDocW (pd.hDC, &di), L"StartDoc", 0);
    CHRF (hr, failedStage = L"StartDoc (starting the print job)");

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
        CHRF (hr, failedStage = std::format (L"rendering page {}", pageIx + 1));

        // Every spool call routes through HrFromSpoolResult: SP_* return codes
        // and GetLastError map to honest HRESULTs, and a mid-job user abort
        // (Print-to-PDF Save-As cancel on modern Windows) surfaces as ERROR_CANCELLED --
        // exit quietly, keep the page, no failure dialog.
        hr = HrFromSpoolResult (StartPage (pd.hDC), L"StartPage", pageIx);
        CHRF (hr, failedStage = std::format (L"StartPage (page {})", pageIx + 1));

        hr = BlitRgbaToDc (pd.hDC, img, pageW, pageH, opt.outputDpi);
        CHRF (hr, failedStage = std::format (L"drawing page {} onto the printer", pageIx + 1));

        hr = HrFromSpoolResult (EndPage (pd.hDC), L"EndPage", pageIx);
        CHRF (hr, failedStage = std::format (L"EndPage (page {})", pageIx + 1));

        pageIx++;
    }

    hr = HrFromSpoolResult (EndDoc (pd.hDC), L"EndDoc", pageIx);
    CHRF (hr, failedStage = L"EndDoc (finishing the print job)");

    started = false;

Error:
    // A user cancel anywhere in the job -- the print dialog up front, or the
    // Print-to-PDF Save-As prompt that surfaces mid-job -- is not a delivery
    // failure. Every stage reports it the same way, so the mapping lives here
    // once rather than being repeated at each spool call. failedStage is
    // cleared because there is no failing stage to name.
    if (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        outOutcome = PrintOutcome::Canceled;
        failedStage.clear();
        hr         = S_OK;
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
    bool                     didOpen  = false;
    bool                     didEmpty = false;
    HRESULT                  hrEncode = S_OK;
    size_t                   px       = 0;
    size_t                   dibBytes = 0;
    // A 32bpp DIB of the whole strip must stay bounded so a huge multi-page
    // banner cannot try to place gigabytes on the clipboard. Above the cap we
    // skip the bitmap and rely on the (compressed) PNG blob instead.
    const size_t             kMaxDibBytes = (size_t) 256 * 1024 * 1024;



    // Render the whole strip exactly once, capping dpi for very tall banners so
    // neither the DIB below nor the PNG blob materializes gigabytes. The source
    // is only 160x144 dpi, so the cap is effectively lossless.
    opt.outputDpi = WholeStripDpi (prefs, raster.GetRowsUsed());
    opt.style     = PrintDotStyleFromPrefs (prefs);

    hr = renderer.Render (raster, 0, raster.GetRowsUsed() - 1, opt, img);
    CHR (hr);
    CBR (img.width > 0 && img.height > 0);

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
    hrEncode = PngCodec::EncodeRgba (img, opt.outputDpi, png);

    if (SUCCEEDED (hrEncode) && !png.empty())
    {
        Byte *   dest = nullptr;

        hPng = GlobalAlloc (GMEM_MOVEABLE, png.size());

        if (hPng != nullptr)
        {
            dest = (Byte *) GlobalLock (hPng);

            if (dest != nullptr)
            {
                memcpy (dest, png.data(), png.size());
                GlobalUnlock (hPng);
            }
            else
            {
                GlobalFree (hPng);
                hPng = nullptr;
            }
        }
    }

    CBR (hDib != nullptr || hPng != nullptr);

    didOpen = OpenClipboard (m_shell.m_hwnd);
    CBR (didOpen);

    opened = true;

    didEmpty = EmptyClipboard();
    CBR (didEmpty);

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
    if (opened)          { CloseClipboard(); }
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
    PrinterJob *   job   = nullptr;
    bool           known = (id == IDM_PRINTER_DISCARD || id == IDM_PRINTER_COPY ||
                            id == IDM_PRINTER_PRINT   || id == IDM_PRINTER_SAVEAS)
                           && m_shell.m_refs.printerCard != nullptr;



    if (known)
    {
        // Take ownership of the strip: stop the worker, then flush any tail
        // bytes. Every strip-level command acts on the whole page, so we drain
        // first and read the job's raster from a quiesced worker (no
        // concurrent mutation). Every arm below is responsible for restarting
        // the worker -- that is why they take the job rather than re-reading it.
        m_shell.m_printerWorker.Stop();

        {
            vector<PrinterEvent>   events;
            m_shell.m_printerWorker.FlushNow (events);
        }

        job = m_shell.m_printerWorker.GetJob();

        // "No page" also covers a strip whose drained bytes left nothing on the
        // paper (no ink AND no feed -- e.g. a bare escape preamble): HasContent
        // is true but GetRowsUsed is 0, which would otherwise reach delivery,
        // paginate to zero pages, and surface as a scary "something went wrong".
        if (job == nullptr || !job->HasContent() || job->GetRaster().GetRowsUsed() <= 0)
        {
            OnPrinterNoPage (id, job);
        }
        else if (id == IDM_PRINTER_COPY)
        {
            OnPrinterCopy (job);
        }
        else if (id == IDM_PRINTER_DISCARD)
        {
            OnPrinterDiscard (job);
        }
        else
        {
            OnPrinterDeliver (job, id == IDM_PRINTER_PRINT);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterNoPage
//
//  Nothing on the paper: say so in the command's own words and resume. A
//  null job means the worker never had one, so it restarts empty.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnPrinterNoPage (int id, PrinterJob * job)
{
    const wchar_t * emptyMsg =
        (id == IDM_PRINTER_COPY)    ? L"The printer has no page to copy yet."
      : (id == IDM_PRINTER_DISCARD) ? L"The printer has no page to discard."
      : (id == IDM_PRINTER_PRINT)   ? L"The printer has no page to print yet."
                                    : L"The printer has no page to save yet.";



    DxuiMessageBox (m_shell.GetPrinterDialogOwner(), &m_shell.m_chromeTheme, emptyMsg, L"Casso Printer", MB_OK | MB_ICONINFORMATION);

    if (job != nullptr)
    {
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->GetByteRing(), job->GetRaster());
    }
    else
    {
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->GetByteRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterCopy
//
//  Copy never consumes the strip: the worker resumes on the same page
//  whether or not the clipboard accepted it.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnPrinterCopy (PrinterJob * job)
{
    HRESULT  hr = CopyPrintoutToClipboard (job->GetRaster());



    m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->GetByteRing(), job->GetRaster());
    m_shell.NotePrinterDeliveryResult (FAILED (hr));

    if (FAILED (hr))
    {
        DxuiMessageBox (m_shell.GetPrinterDialogOwner(), &m_shell.m_chromeTheme, L"Could not copy the printout to the clipboard.",
                     L"Casso Printer", MB_OK | MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterDiscard
//
//  Tear off and throw away the current page (FR-029). Confirm first --
//  there is no undo -- and default the dialog to "No" so a stray Enter
//  never destroys a page.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnPrinterDiscard (PrinterJob * job)
{
    int   choice = DxuiMessageBox (
        m_shell.GetPrinterDialogOwner(),
        &m_shell.m_chromeTheme,
        L"Tear off and discard the current printout?\n\n"
        L"The page in the printer will be thrown away without saving. "
        L"This cannot be undone.",
        L"Discard Printout", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);



    if (choice != IDYES)
    {
        // Canceled: keep the strip and resume on the same page.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->GetByteRing(), job->GetRaster());
    }
    else
    {
        // Confirmed: play the tear-off (a random paper-tear), start a fresh
        // sheet, and drop the persisted pending copy. The problem page (if
        // any) went with it, so a latched delivery error clears too.
        m_shell.m_printerAudio.PlayTearOff();
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->GetByteRing());
        PrintJobStore::Clear (m_shell.GetPendingPrintDir());
        m_shell.NotePrinterDeliveryResult (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterDeliver
//
//  Print -> Windows printer; Save -> PNG via file dialog. Both are
//  non-destructive: the paper stays so it can be delivered again, which is
//  why every arm here restarts the worker with the SAME raster.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnPrinterDeliver (PrinterJob * job, bool print)
{
    HRESULT       hr              = S_OK;
    PrintOutcome  outcome         = PrintOutcome::Delivered;
    fs::path      file;
    std::wstring  failedStage;
    wchar_t       forceClassic[8] = {};
    bool          modernUp        = false;



    if (print && GetEnvironmentVariableW (L"CASSO_CLASSIC_PRINT", forceClassic, 8) == 0)
    {
        // DCR-1: the modern OS print UI with a live preview is the default --
        // it follows the documented PrintManagerInterop contract and delivers a
        // real paginated preview, then posts IDM_PRINTER_MODERN_SENT / _FAILED
        // on completion. ShowAsync returns S_OK once the experience is up; a
        // hard failure returns FAILED and we fall back to the classic dialog
        // (which prints with honest error reporting, just no preview pane). The
        // CASSO_CLASSIC_PRINT env var forces the classic path -- a support
        // hatch for the rare machine whose print stack misbehaves.
        const GlobalUserPrefs &  prefs  = m_shell.m_globalPrefs;
        HRESULT                  hrShow = m_modernPrint.ShowAsync (m_shell.m_hwnd, job->GetRaster(),
                                                                   PrintDpiFromPrefs (prefs),
                                                                   PrintDotStyleFromPrefs (prefs));

        // The async session owns the outcome from here; resume and let its
        // completion callback post the result.
        modernUp = SUCCEEDED (hrShow);
    }

    if (!modernUp)
    {
        hr = print ? PrintToWindowsPrinter (job->GetRaster(), failedStage, outcome)
                   : SavePrintoutAs (job->GetRaster(), file, outcome);
    }

    if (modernUp || outcome == PrintOutcome::Canceled)
    {
        // Modern session up, or the user canceled the print / save dialog:
        // keep the strip either way, no clear.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->GetByteRing(), job->GetRaster());
    }
    else if (SUCCEEDED (hr))
    {
        std::wstring   msg = print
                                 ? std::wstring (L"Sent the printout to the printer.")
                                 : (L"Saved printout to:\n" + file.wstring());

        m_shell.NotePrinterDeliveryResult (false);
        DxuiMessageBox (m_shell.GetPrinterDialogOwner(), &m_shell.m_chromeTheme, msg.c_str(), L"Casso Printer", MB_OK | MB_ICONINFORMATION);

        // Non-destructive: keep the paper so it can also be saved / printed.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->GetByteRing(), job->GetRaster());
    }
    else
    {
        // Human sentence first, then a "Details:" trailer with the failing stage
        // + hr + OS message so a nerd (or a bug report) can see exactly what
        // failed without the dialog reading like a stack trace.
        std::wstring   msg = print
                                 ? std::wstring (L"Something went wrong while sending your printout, so it is still waiting in the printer. Please try printing again.")
                                 : std::wstring (L"Something went wrong while saving your printout, so it is still waiting in the printer. Please try again.");

        msg += L"\n\nDetails: ";
        if (!failedStage.empty())
        {
            msg += failedStage + L"\n";
        }

        msg += FormatSystemError (hr);

        m_shell.NotePrinterDeliveryResult (true);   // toolbar LED: red until resolved

        DxuiMessageBox (m_shell.GetPrinterDialogOwner(), &m_shell.m_chromeTheme, msg.c_str(),
                     L"Casso Printer", MB_OK | MB_ICONWARNING);

        // Keep the strip so the user can retry -- reseed the worker with it
        // (copied before the old job is replaced). It re-persists on exit.
        m_shell.m_printerWorker.Start (m_shell.m_refs.printerCard->GetByteRing(), job->GetRaster());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnModernPrintResult
//
//  Posted back by the modern print session's completion callback (which runs
//  on a print-system thread and must not raise UI itself). The strip was
//  copied into the session and the worker already resumed, so there is
//  nothing to reseed here -- just tell the user how it went. Cancel posts
//  nothing, matching the classic dialog's silent cancellation.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnModernPrintResult (bool succeeded)
{
    m_shell.NotePrinterDeliveryResult (!succeeded);   // toolbar LED tracks the outcome

    if (succeeded)
    {
        DxuiMessageBox (m_shell.GetPrinterDialogOwner(), &m_shell.m_chromeTheme,
                        L"Sent the printout to the printer.",
                        L"Casso Printer", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        DxuiMessageBox (m_shell.GetPrinterDialogOwner(), &m_shell.m_chromeTheme,
                        L"Something went wrong while sending your printout, so it is still "
                        L"waiting in the printer. Please try printing again.",
                        L"Casso Printer", MB_OK | MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHelpCommand
//
//  The Help menu: the keyboard map and the About box.
//
//  Both are built as DialogDefinition data and handed to the shared modal
//  renderer rather than being dialog resources, so they pick up the active
//  theme and DPI like the rest of the chrome instead of appearing as stock
//  Win32 dialogs in the middle of custom skeuomorphic UI.
//
//  About stamps the version and build timestamp from the compiled-in macros,
//  so the reported version is necessarily the running binary's rather than
//  something maintained by hand.
//
//  The link layout depends on how body runs flow: they stack at the body line
//  height with no inter-run gap, so an EMPTY run is exactly one blank line.
//  That is what groups GitHub with "Log a bug", and the license with the
//  third-party attribution, as two visually distinct pairs.
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
            def.body.push_back ({ L"Casso Emulator\nCopyright (C) by Robert Elmer"
                                  L"\n\nVersion " _CRT_WIDE (VERSION_STRING)
                                  L"\nBuilt " _CRT_WIDE (VERSION_BUILD_TIMESTAMP)
                                  L"\n\nAn Apple 2 family emulator.",
                                  false, L"" });

            // Body runs flow at the body line height with no inter-run gap, so
            // links stack like text lines and an empty run is one blank line.
            // Links: GitHub + Log a bug tight; blank line; MIT + attribution tight.
            std::wstring githubBlurb = L"Casso on GitHub ";
            githubBlurb += s_kchEmDash;
            githubBlurb += L" ";
            githubBlurb += s_kpszStar;
            githubBlurb += L" stars welcome";

            def.body.push_back ({ L"", false, L"" });
            def.body.push_back ({ githubBlurb,
                                  true, L"https://github.com/relmer/Casso" });
            def.body.push_back ({ L"Log a bug",
                                  true, L"https://github.com/relmer/Casso/issues/new?template=bug_report.yml" });
            def.body.push_back ({ L"", false, L"" });
            def.body.push_back ({ L"MIT License",
                                  true, L"https://github.com/relmer/Casso/blob/master/LICENSE" });
            def.body.push_back ({ L"ImageWriter II sounds by Scott Lawrence (CC BY 4.0)",
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
