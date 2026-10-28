#include "Pch.h"
#include "Shell/Components/ShellInput.h"
#include "Ui/Dialogs/DialogDefinition.h"

#include "Core/ThreadName.h"
#include "WindowCommandManager.h"

#include "../AssetBootstrap.h"
#include "Config/WindowPlacementProfile.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"
#include "Shell/ClipboardManager.h"
#include "Shell/Components/ShellWindow.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/WindowManager.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Shell/Components/ShellAudio.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellPrinter.h"
#include "Shell/Components/ShellUpdater.h"
#include "../resource.h"
#include "../Shell/DiskMru.h"
#include "../Ui/Dialogs/KeyboardMapText.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/PrintDelivery.h"
#include "Devices/Printer/PrintFileNaming.h"
#include "Devices/Printer/PrintPagination.h"
#include "Devices/Printer/PrintRaster.h"
#include "Machines/Apple2/Common/PrinterCard.h"
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
#include "Cassque/Model/KnownFolderStore.h"
#include "Cassque/Model/LaunchCommand.h"
#include "Config/Win32FileSystem.h"
#include "Core/PathResolver.h"
#include "Seams/Win32ProcessLauncher.h"
#include "Ui/Chrome/TapeDeckWidget.h"





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
//  GetCommandRoute
//
//  The handler a WM_COMMAND id goes to, or None for an id OnCommand drops.
//
//  Routing is by RANGE, not by a per-id table, which is why the IDM_ values
//  in the resource header are grouped and contiguous per menu: adding a
//  command to an existing menu needs only an id inside that menu's span and a
//  case in its handler, with nothing to update here.
//
//  The ids listed individually are outside their menu's span -- the Disk and
//  View commands added after the span filled, the printer toolbar actions and
//  the two async print-result notifications the worker posts back. A new id
//  outside every branch is dropped in SILENCE -- the menu item paints, the
//  accelerator fires, and nothing happens -- so ChromeCommandRoutingTests fail
//  on any command table id that routes to None.
//
////////////////////////////////////////////////////////////////////////////////

WindowCommandRoute WindowCommandManager::GetCommandRoute (int id)
{
    WindowCommandRoute  route = WindowCommandRoute::None;



    if      (id >= IDM_EDIT_COPY_TEXT && id <= IDM_EDIT_PASTE)              { route = WindowCommandRoute::Edit; }
    else if (id >= IDM_FILE_OPEN      && id <= IDM_FILE_EXIT)               { route = WindowCommandRoute::File; }
    else if (id == IDM_FILE_SAVE_STATE || id == IDM_FILE_LOAD_STATE)        { route = WindowCommandRoute::File; }
    else if (id >= IDM_MACHINE_RESET  && id <= IDM_MACHINE_ARROWS_PADDLE)   { route = WindowCommandRoute::Machine; }
    else if (id >= IDM_DISK_INSERT1   && id <= IDM_DISK_WP2)                { route = WindowCommandRoute::Disk; }
    else if (id == IDM_DISK_SALVAGE1  || id == IDM_DISK_SALVAGE2)           { route = WindowCommandRoute::Disk; }
    else if (id == IDM_DISK_OPEN_CASSQUE)                                  { route = WindowCommandRoute::Disk; }
    else if (id >= IDM_TAPE_INSERT    && id <= IDM_TAPE_FASTFORWARD)        { route = WindowCommandRoute::Disk; }
    else if (id == IDM_STORAGE_DRIVE2 || id == IDM_STORAGE_RECORDER)        { route = WindowCommandRoute::Disk; }
    else if (id >= IDM_VIEW_COLOR     && id <= IDM_VIEW_SETTINGS)           { route = WindowCommandRoute::View; }
    else if (id == IDM_VIEW_DRIVE_STRIP)                                   { route = WindowCommandRoute::View; }
    else if (id == IDM_VIEW_FRAME_RATE)                                    { route = WindowCommandRoute::View; }
    else if (id == IDM_VIEW_SCENE_VIEW)                                    { route = WindowCommandRoute::View; }
    else if (id == IDM_VIEW_DEBUGGER)                                      { route = WindowCommandRoute::View; }
    else if (id == IDM_DEBUG_RESTART)                                      { route = WindowCommandRoute::View; }
    else if (id == IDM_VIEW_CONTROLLER_SETTINGS)                           { route = WindowCommandRoute::View; }
    else if (id == IDM_DEBUG_SAVE_TRACE)                                   { route = WindowCommandRoute::Machine; }
    else if (id == IDM_PRINTER_DISCARD)                                    { route = WindowCommandRoute::Printer; }
    else if (id == IDM_PRINTER_COPY)                                       { route = WindowCommandRoute::Printer; }
    else if (id == IDM_PRINTER_PRINT)                                      { route = WindowCommandRoute::Printer; }
    else if (id == IDM_PRINTER_SAVEAS)                                     { route = WindowCommandRoute::Printer; }
    else if (id == IDM_PRINTER_MODERN_SENT)                                { route = WindowCommandRoute::PrinterModernSent; }
    else if (id == IDM_PRINTER_MODERN_FAILED)                              { route = WindowCommandRoute::PrinterModernFailed; }
    else if (id == IDM_PRINTER_PREVIEW)                                    { route = WindowCommandRoute::PrinterPreview; }
    else if (id >= IDM_HELP_KEYMAP    && id <= IDM_HELP_ABOUT)              { route = WindowCommandRoute::Help; }
    else if (id == IDM_DRIVE_EXTERNAL_CONNECT ||
             id == IDM_DRIVE_EXTERNAL_DISCONNECT)                          { route = WindowCommandRoute::ExternalDrive; }
    else if (id == IDM_MOUSE_CONNECT ||
             id == IDM_MOUSE_DISCONNECT)                                   { route = WindowCommandRoute::MouseConnect; }

    return route;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCommand
//
//  Routes a WM_COMMAND id to the handler GetCommandRoute picks.
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

    switch (GetCommandRoute (id))
    {
        case WindowCommandRoute::File:                OnFileCommand (id);                             break;
        case WindowCommandRoute::Edit:                OnEditCommand (id);                             break;
        case WindowCommandRoute::Machine:             OnMachineCommand (id);                          break;
        case WindowCommandRoute::Disk:                OnDiskCommand (id);                             break;
        case WindowCommandRoute::View:                OnViewCommand (id);                             break;
        case WindowCommandRoute::Printer:             m_shell.m_printer->OnPrinterCommand (id);       break;
        case WindowCommandRoute::PrinterPreview:      m_shell.m_printer->ShowPrinterPanel();          break;
        case WindowCommandRoute::PrinterModernSent:   m_shell.m_printer->OnModernPrintResult (true);  break;
        case WindowCommandRoute::PrinterModernFailed: m_shell.m_printer->OnModernPrintResult (false); break;
        case WindowCommandRoute::Help:                OnHelpCommand (id);                             break;
        case WindowCommandRoute::ExternalDrive:       OnExternalDriveCommand (id);                    break;
        case WindowCommandRoute::MouseConnect:        OnMouseConnectCommand (id);                     break;
        case WindowCommandRoute::None:                                                                break;
    }

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



    if (connected != m_shell.m_input->IsMouseConnected())
    {
        m_shell.m_input->SetMouseConnected (connected);

        if (!connected && m_shell.m_input->GetPointerMode() == InputMappingMode::Mouse)
        {
            // Drops Mouse mode; SetPointerMapping re-syncs the selector as a
            // side effect (SyncInputModeUi -> SyncSelectorState).
            m_shell.m_input->SetPointerMapping (InputMappingMode::Off);
        }
        else
        {
            // Availability changed with no mode change (reconnect, or a
            // disconnect while not in Mouse mode): refresh the selector so
            // the Mouse segment reappears / disappears -- SetState flips the
            // availability flag and the toolbar rebuilds the 2<->3 segment
            // geometry + hit map. UI-thread routed (posted WM_COMMAND).
            m_shell.m_input->SyncPaddleSourceList();
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



    if (connected != m_shell.m_disks->IsExternalDriveConnected())
    {
        m_shell.m_disks->SetExternalDriveConnected (connected);
        fChanged = true;
    }

    // A carded machine's answer lives in the card's second connector, so the
    // RUNNING config has to move too -- ShouldShowExternalDrive reads the
    // attached count from there for anything that is not a //c, and would
    // otherwise keep reporting the old one however the flag above is set.
    if (m_shell.m_machine.GetConfig().SetDiskIiPortAttached (1, connected))
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
    if (!connected && m_shell.m_disks->GetManager() != nullptr)
    {
        m_shell.m_disks->GetManager()->Eject (6, 1);
    }

    m_shell.m_chrome->ReflowChromeForMachineChange();
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

        case IDM_FILE_SAVE_STATE:
        {
            m_shell.ShowSaveStateDialog();
            break;
        }

        case IDM_FILE_LOAD_STATE:
        {
            m_shell.ShowLoadStateDialog();
            break;
        }

        // Through WM_CLOSE like every other way out, so the automatic state's
        // prompt runs for this one too.
        case IDM_FILE_EXIT:
        {
            PostMessageW (m_shell.m_hwnd, WM_CLOSE, 0, 0);
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
            // Main RAM comes in only where an MMU banks it: there the bus may
            // be serving aux at the text page.
            m_shell.m_clipboardManager->CopyScreenText (m_shell.m_hwnd, m_shell.GetMainRamBuffer(), m_shell.GetAuxRamBuffer());
            break;
        }

        case IDM_EDIT_COPY_SCREENSHOT:
        {
            m_shell.m_renderer->TakeScreenshot();
            break;
        }

        case IDM_EDIT_PASTE:
        {
            m_shell.m_input->PasteClipboardText();
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
//  Step is posted too, and only while paused. A paused CPU thread still
//  drains its queue: a posted command wakes it from the pause wait without
//  resuming the machine, so the step runs on the CPU thread, between
//  instructions, like every other command that touches the machine.
//
//  Speed and pause are plain CpuManager calls -- flags the CPU thread reads,
//  the pause one before every instruction -- so they need no marshalling at
//  all. A pause then waits, briefly, for the CPU thread to park.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnMachineCommand (int id)
{
    switch (id)
    {
        case IDM_MACHINE_RESET:
        {
            m_shell.RequestReset();
            break;
        }

        case IDM_MACHINE_POWERCYCLE:
        {
            m_shell.PostCommand (static_cast<WORD> (id));
            break;
        }

        case IDM_MACHINE_PAUSE:
        {
            constexpr std::chrono::milliseconds  kParkWait { 100 };
            bool                                 isParked  = false;



            m_shell.m_cpuManager.TogglePaused();

            // The pause is acted on before anything reports it: the CPU thread
            // stops the machine on the next instruction boundary and parks.
            // The wait is bounded: the thread may first be busy with a long
            // command, and the UI waits a moment for it rather than hanging.
            if (m_shell.m_cpuManager.IsPaused())
            {
                isParked = m_shell.m_cpuManager.TryWaitUntilParked (kParkWait);
                IGNORE_RETURN_VALUE (isParked, false);
            }

            // An attached debugger hears about it on the CPU thread, where the
            // session and any run in progress live.
            m_shell.m_cpuManager.PostCommand (IDM_DEBUG_PAUSE_CHANGED, m_shell.m_cpuManager.IsPaused() ? "1" : "0");

            m_shell.m_window->UpdateWindowTitle();
            break;
        }

        case IDM_DEBUG_SAVE_TRACE:
        {
            // The ring belongs to the CPU thread, and pausing only sets a
            // flag, so the write is queued to run there between slices.
            if (m_shell.IsTracing())
            {
                m_shell.PostCommand (static_cast<WORD> (id));
            }
            else
            {
                m_shell.m_chrome->PostNotice (L"Start Casso with --trace to record a CPU trace.");
            }

            break;
        }

        case IDM_MACHINE_STEP:
        {
            if (! m_shell.m_cpuManager.IsPaused())
            {
                break;
            }

            // The step runs on the CPU thread, where the machine always runs.
            // A paused CPU thread still drains its queue, so it arrives.
            m_shell.PostCommand (static_cast<WORD> (id));
            break;
        }

        case IDM_MACHINE_SPEED_1X:
        {
            m_shell.m_cpuManager.SetSpeedMode (SpeedMode::Authentic, SpeedChooser::User);
            break;
        }

        case IDM_MACHINE_SPEED_2X:
        {
            m_shell.m_cpuManager.SetSpeedMode (SpeedMode::Double, SpeedChooser::User);
            break;
        }

        case IDM_MACHINE_SPEED_MAX:
        {
            m_shell.m_cpuManager.SetSpeedMode (SpeedMode::Maximum, SpeedChooser::User);
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
                std::wstring (m_shell.m_machine.GetConfig().name.begin(), m_shell.m_machine.GetConfig().name.end()),
                std::wstring (m_shell.m_machine.GetConfig().cpu.begin(), m_shell.m_machine.GetConfig().cpu.end()),
                m_shell.m_machine.GetConfig().clockSpeed,
                (m_shell.m_machine.GetConfig().ram.size() + 1 + m_shell.m_machine.GetConfig().slots.size()),
                (m_shell.m_machine.GetConfig().internalDevices.size() + m_shell.m_machine.GetConfig().slots.size()));

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
            m_shell.m_input->ToggleInputMappingMode (InputMappingMode::Joystick);
            break;
        }

        case IDM_MACHINE_ARROWS_PADDLE:
        {
            m_shell.m_input->ToggleInputMappingMode (InputMappingMode::Paddle);
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
//  Reset-view resets the SIZE and the scene pose, never the position: the
//  window stays exactly where the user put it. Sizing it around a fixed
//  top-left also keeps the caption where the pointer already is, so the fit
//  to the work area below is about the size alone. It is a no-op while
//  fullscreen, where the size is not the window's to choose.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnViewCommand (int id)
{
    HMONITOR    hMon  = nullptr;
    MONITORINFO mi    = { sizeof (mi) };
    int         w     = 0;
    int         h     = 0;



    switch (id)
    {
        case IDM_VIEW_COLOR:
        {
            m_shell.m_renderer->SetColorMode (ColorMode::Color);
            break;
        }

        case IDM_VIEW_GREEN:
        {
            m_shell.m_renderer->SetColorMode (ColorMode::GreenMono);
            break;
        }

        case IDM_VIEW_AMBER:
        {
            m_shell.m_renderer->SetColorMode (ColorMode::AmberMono);
            break;
        }

        case IDM_VIEW_WHITE:
        {
            m_shell.m_renderer->SetColorMode (ColorMode::WhiteMono);
            break;
        }

        case IDM_VIEW_FULLSCREEN:
        {
            RECT  rcClient = {};

            m_shell.m_renderer->GetD3D().ToggleFullscreen (m_shell.m_hwnd);

            // The transition's WM_SIZE runs while the fullscreen flag is
            // deliberately still held (placement persistence), so the layout
            // it triggered used the OLD presentation. Re-run it now that the
            // flag reflects the final state -- the desk scene's fullscreen
            // branch keys off it.
            if (m_shell.m_hwnd != nullptr && GetClientRect (m_shell.m_hwnd, &rcClient))
            {
                (void) m_shell.OnSize ((UINT) (rcClient.right - rcClient.left),
                                       (UINT) (rcClient.bottom - rcClient.top));
                m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
            }

            break;
        }

        case IDM_VIEW_FRAME_RATE:
        {
            // Persisted, so the choice outlives the session in either build.
            m_shell.m_settings->GetPrefs().showFrameRate = !m_shell.m_settings->GetPrefs().showFrameRate;
            m_shell.m_settings->SaveGlobalPrefs();
            break;
        }

        case IDM_VIEW_SCENE_VIEW:
        {
            m_shell.m_settings->GetPrefs().showSceneView = !m_shell.m_settings->GetPrefs().showSceneView;
            m_shell.m_settings->SaveGlobalPrefs();
            break;
        }

        case IDM_VIEW_RESET_SIZE:
        {
            // One key puts everything back: the machine faces front at its
            // default zoom, and the window goes to 100%. Splitting these
            // across two chords asked the user to know which of the two
            // ways a view can be off was the one they were looking at.
            //
            // Outside the fullscreen guard because a fullscreen window has
            // no size to reset but the scene can still be turned around.
            // Harmless where no scene exists: ResetSceneView returns early
            // at identity, which a themeless scene never leaves.
            m_shell.m_scene->ResetSceneView();

            if (!m_shell.m_renderer->GetD3D().IsFullscreen())
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
                    SIZE  desired = m_shell.m_window->GetClientSizeForFramebufferPx (
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
                // rather than a bare framebuffer. Fit the SIZE to the work
                // area, uniformly, so an oversized reset shrinks to the
                // monitor instead of running off it.
                //
                // Only the size: SWP_NOMOVE holds the top-left where the
                // user dragged it. Resetting the view used to re-center the
                // window on its monitor, which moved a window the user had
                // deliberately parked -- a reset of the VIEW has no business
                // moving the window. Keeping the top-left is also what keeps
                // the caption reachable, which is why the fit needs no
                // position rule of its own any more.
                hMon = MonitorFromWindow (m_shell.m_hwnd, MONITOR_DEFAULTTONEAREST);

                if (hMon != nullptr && GetMonitorInfo (hMon, &mi))
                {
                    RECT  sized = WindowPlacementProfile::FitToWorkArea (mi.rcWork, w, h);

                    w = (int) (sized.right  - sized.left);
                    h = (int) (sized.bottom - sized.top);

                    SetWindowPos (m_shell.m_hwnd, nullptr, 0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE);

                    // Persisted here because nothing else will: placement is saved
                    // only at the end of the OS drag loop or on a user maximize or
                    // restore, precisely so a programmatic SetWindowPos cannot
                    // stomp it. This one is the user's, so it has to say so.
                    m_shell.m_window->GetWindowManager().SaveWindowPlacement (m_shell.m_hwnd, false);
                }
            }

            break;
        }

        case IDM_VIEW_DEBUGGER:
        {
            m_shell.m_debugger->OpenDebuggerWindow();
            break;
        }

        case IDM_VIEW_DISK2_LOG:
        case IDM_VIEW_INPUT_LOG:
        {
            m_shell.m_debugger->OpenDebuggerLog ((id == IDM_VIEW_DISK2_LOG) ? DebugLogKind::Disk : DebugLogKind::Input);
            break;
        }

        case IDM_DEBUG_RESTART:
        {
            // Both go to the CPU queue in order: the channel opens, which
            // starts the call-stack recorder, before the power cycle runs.
            m_shell.m_debugger->OpenDebuggerWindow();
            m_shell.PostCommand (IDM_MACHINE_POWERCYCLE);
            break;
        }

        case IDM_VIEW_SETTINGS:
        {
            m_shell.m_settings->OpenSettings();
            break;
        }

        case IDM_VIEW_CONTROLLER_SETTINGS:
        {
            m_shell.m_settings->OpenSettings (true);
            break;
        }

        case IDM_VIEW_DRIVE_STRIP:
        {
            // Only meaningful in fullscreen; the FSM consumes the edge on its
            // next tick (releasing a guest capture if one is held). Windowed,
            // the drives are already on screen.
            if (m_shell.m_renderer->GetD3D().IsFullscreen())
            {
                m_shell.m_scene->RequestStripHotkey();
                m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
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
    HRESULT         hr     = S_OK;
    FileDialogSpec  spec;
    fs::path        chosen;
    bool            picked = false;



    outMountStarted = false;

    spec.filters = { { L"Disk images", L"*.dsk;*.do;*.woz;*.po" },
                     { L"All files",   L"*.*" } };

    hr = m_shell.GetHostDialogs().PickFileToOpen (m_shell.m_hwnd, spec, chosen, picked);
    CHR (hr);

    // Backing out of the file picker means there is nothing to mount -- not
    // an error; outMountStarted (still false) is what tells the caller no
    // mount happened.
    BAIL_OUT_IF (!picked, S_OK);

    hr = m_shell.m_disks->Mount (6, drive - 1, chosen.wstring());
    CHR (hr);

    outMountStarted = true;

Error:
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
    int                        choice    = IDYES;
    bool                       occupied  = false;
    std::wstring               folder;
    std::wstring               message;
    std::vector<std::wstring>  mountedPaths;
    std::vector<int>           mountedDrives;
    vector<Byte>               imageBytes;
    std::string                imageContent;
    BootPayload                payload;
    FileBrowseModel            model;
    CreateDiskDialog           dialog;
    DxuiWindow::CreateParams   params;



    outMountStarted = false;

    folder = GetDiskCreateFolder();

    // The model refuses a target that is currently mounted in any drive; the
    // store's backing paths are UTF-8 and go wide through the same u8string
    // interpretation the MRU uses.
    for (const DiskImageStore::MountedSource & mounted : m_shell.m_machine.GetDiskStore().GetMountedSourcePaths())
    {
        std::u8string  u8 (reinterpret_cast<const char8_t *> (mounted.path.data()),
                           mounted.path.size());

        mountedPaths.push_back (std::filesystem::path (u8).wstring());
        mountedDrives.push_back (mounted.drive);
    }

    model.Bind (&m_shell.m_settings->GetFileSystem());
    model.SetExtensionFilter (L".woz");
    model.SetMountedPaths (std::move (mountedPaths), std::move (mountedDrives));

    hr = model.SetFolder (folder);
    CHRF (hr, DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chrome->GetTheme(),
                              L"Could not open the disk folder.",
                              L"Create new disk", MB_OK | MB_ICONERROR));

    // Boot-payload plumbing: availability answers from the download cache;
    // the download callback runs on the dialog's explicit button click.
    dialog.Configure (&model, &m_shell.m_chrome->GetTheme(),
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

    params.title                    = std::format (L"Create new disk (Drive {})", drive);
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = m_shell.m_hwnd;
    params.initialSizeDip           = { 560, 480 };
    params.minSizeDip               = { 420, 360 };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (&m_shell.m_chrome->GetTheme());
    dialog.ShowModalDialog (IDOK);

    // Cancel / Escape / close box: nothing to do, and no mount started.
    BAIL_OUT_IF (!dialog.GetOutcome().confirmed, S_OK);

    // The target drive may already hold a disk; replacing it needs a yes.
    occupied = m_shell.m_machine.GetDiskStore().IsMounted (6, drive - 1);

    if (occupied)
    {
        message = std::format (L"Drive {} already has a disk. Replace it with the new disk?", drive);
        choice  = DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chrome->GetTheme(), message.c_str(),
                                  L"Create new disk", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING);

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

        CBRF (opened, DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chrome->GetTheme(),
                                      L"The OS master disk is missing from the download cache.",
                                      L"Create new disk", MB_OK | MB_ICONERROR));

        vector<Byte> &  dest = isProDos ? payload.proDosUsersDisk
                                        : payload.dosMasterSectors;

        dest.assign (std::istreambuf_iterator<char> (master),
                     std::istreambuf_iterator<char> ());
    }

    hr = BlankDiskBuilder::Build (dialog.GetOutcome().spec, payload, imageBytes);
    CHRF (hr, DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chrome->GetTheme(),
                              L"Could not build the new disk image.",
                              L"Create new disk", MB_OK | MB_ICONERROR));

    // Atomic: the filesystem stages a sibling temp file and swaps it in, so
    // a failure here leaves no partial image behind.
    imageContent.assign (reinterpret_cast<const char *> (imageBytes.data()), imageBytes.size());

    hr = m_shell.m_settings->GetFileSystem().WriteAllText (dialog.GetOutcome().targetPath, imageContent);
    CHRF (hr, DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chrome->GetTheme(),
                              (L"Could not write \"" + dialog.GetOutcome().targetPath + L"\".\n\n"
                               + FormatSystemError (hr)).c_str(),
                              L"Create new disk", MB_OK | MB_ICONERROR));

    // Remember where this disk landed (the user may have navigated away
    // from the starting folder); the next create opens there.
    {
        std::u8string  u8folder = std::filesystem::path (dialog.GetOutcome().targetPath)
                                      .parent_path().u8string();

        m_shell.m_settings->GetPrefs().lastDiskCreateFolder.assign (u8folder.begin(), u8folder.end());
        m_shell.m_settings->SaveGlobalPrefs();
    }

    hr = m_shell.m_disks->Mount (6, drive - 1, dialog.GetOutcome().targetPath);
    CHRF (hr, DxuiMessageBox (m_shell.m_hwnd, &m_shell.m_chrome->GetTheme(),
                              L"The disk was created but could not be mounted.",
                              L"Create new disk", MB_OK | MB_ICONERROR));

    outMountStarted = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDiskCreateFolder
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WindowCommandManager::GetDiskCreateFolder()
{
    HRESULT          hr      = S_OK;
    PWSTR            docsRaw = nullptr;
    std::wstring     folder;
    std::error_code  ec;



    if (!m_shell.m_settings->GetPrefs().lastDiskCreateFolder.empty())
    {
        const std::string &  stored = m_shell.m_settings->GetPrefs().lastDiskCreateFolder;
        std::u8string        u8     (reinterpret_cast<const char8_t *> (stored.data()), stored.size());
        std::wstring         last   = std::filesystem::path (u8).wstring();

        if (std::filesystem::exists (last, ec))
        {
            folder = last;
        }
    }

    BAIL_OUT_IF (!folder.empty(), S_OK);

    hr = SHGetKnownFolderPath (FOLDERID_Documents, 0, nullptr, &docsRaw);
    CHRA (hr);

    folder = docsRaw;
    folder += L"\\Casso Disks";
    std::filesystem::create_directories (folder, ec);

Error:
    if (docsRaw != nullptr)
    {
        CoTaskMemFree (docsRaw);
    }

    return folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRecentMedia
//
//  The picker's list for one kind of media: the recent entries of that kind
//  that still exist, then the files of that kind in the folders of every
//  recent entry, then the bundled demos. Disks and tapes share the recent list
//  and the folders; the filter is the only difference.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::GetRecentMedia (MediaFilter isWanted, std::vector<DiskMru::Entry> & entries)
{
    DiskMru                      mru      = DiskMru::FromUtf8 (m_shell.m_settings->GetPrefs().recentDisks,
                                                               m_shell.m_settings->GetPrefs().recentDiskLoadedAt);
    std::vector<DiskMru::Entry>  existing = mru.Prune ([] (const std::filesystem::path & p)
                                                       {
                                                           return std::filesystem::exists (p)
                                                                  && !AssetBootstrap::IsForeignCheckoutDisk (p);
                                                       });
    Win32FileSystem              fs;
    int64_t                      nowUnix  = (int64_t) std::chrono::duration_cast<std::chrono::seconds> (
                                                std::chrono::system_clock::now().time_since_epoch()).count();



    entries.clear();

    for (const DiskMru::Entry & entry : existing)
    {
        if (isWanted (entry.path.wstring()))
        {
            entries.push_back (entry);
        }
    }

    AssetBootstrap::AppendSiblingDisksFromFolders (
        KnownFolderStore::LoadPickerFolders (fs, AssetBootstrap::GetAssetBaseDirectory().wstring(), existing, nowUnix),
        entries,
        isWanted);
    AssetBootstrap::AppendSiblingDisksFromMruFolders (entries, existing, isWanted);
    AssetBootstrap::AppendBundledDemoDisks (entries, isWanted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromptInsertTapeMru
//
//  The disk picker, choosing a tape: same recent list, same folders, tape
//  files only, and no stock downloads. Browse and create-new go to the tape
//  versions of those flows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WindowCommandManager::PromptInsertTapeMru (const RECT * anchorRectPx)
{
    HRESULT                      hr            = S_OK;
    std::vector<DiskMru::Entry>  entries;
    std::wstring                 chosenPath;
    std::string                  error;
    bool                         userBrowsed   = false;
    bool                         userCreateNew = false;



    GetRecentMedia (TapeImageLoader::IsTapeFileExtension, entries);

    hr = AssetBootstrap::PromptInsertDiskMru (GetModuleHandle (nullptr),
                                              m_shell.m_hwnd,
                                              AssetBootstrap::MakeTapePickerKind(),
                                              anchorRectPx,
                                              entries,
                                              AssetBootstrap::GetDiskDirectory(),
                                              m_shell.m_settings->GetPrefs().activeTheme,
                                              chosenPath,
                                              userBrowsed,
                                              userCreateNew,
                                              error);
    CHR (hr);

    if (userBrowsed)
    {
        m_shell.m_tapeDeck->BrowseForTape();
    }
    else if (userCreateNew)
    {
        m_shell.m_tapeDeck->CreateBlankTape();
    }
    else if (!chosenPath.empty())
    {
        m_shell.m_tapeDeck->InsertTape (chosenPath);
    }

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

HRESULT WindowCommandManager::PromptInsertDiskMru (int drive, const RECT * anchorRectPx, bool & outMountStarted)
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



    diskDir = AssetBootstrap::GetDiskDirectory();
    GetRecentMedia (IsSupportedDiskImageExtension, mruPruned);

    hr = AssetBootstrap::PromptInsertDiskMru (GetModuleHandle (nullptr),
                                              m_shell.m_hwnd,
                                              AssetBootstrap::MakeDiskPickerKind (drive),
                                              anchorRectPx,
                                              mruPruned,
                                              diskDir,
                                              m_shell.m_settings->GetPrefs().activeTheme,
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
        hr = m_shell.m_disks->Mount (6, drive - 1, chosenPath);
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
            m_shell.m_disks->BrowseForDisk ((id == IDM_DISK_INSERT1) ? 0 : 1);
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

        case IDM_DISK_SALVAGE1:
        case IDM_DISK_SALVAGE2:
        {
            // Stays on the UI thread: the flow opens Dxui modals.
            m_shell.m_disks->RunSalvageFlow ((id == IDM_DISK_SALVAGE1) ? 0 : 1);
            break;
        }

        case IDM_DISK_OPEN_CASSQUE:
        {
            OpenCassque();
            break;
        }

        // The tape items open their pickers here; the deck commands are
        // queued to the CPU thread by the tape manager.
        case IDM_TAPE_INSERT:      m_shell.m_tapeDeck->PickTape();                                     break;
        case IDM_TAPE_NEW:         m_shell.m_tapeDeck->CreateBlankTape();                              break;
        case IDM_TAPE_EJECT:       m_shell.m_tapeDeck->HandleTapeClick (TapeDeckRegion::Eject);        break;
        case IDM_TAPE_PLAY:        m_shell.m_tapeDeck->HandleTapeClick (TapeDeckRegion::Play);         break;
        case IDM_TAPE_STOP:        m_shell.m_tapeDeck->HandleTapeClick (TapeDeckRegion::Stop);         break;
        case IDM_TAPE_REWIND:      m_shell.m_tapeDeck->HandleTapeClick (TapeDeckRegion::Rewind);       break;
        case IDM_TAPE_FASTFORWARD: m_shell.m_tapeDeck->HandleTapeClick (TapeDeckRegion::FastForward);  break;

        // Each flips its device, connected to disconnected or back.
        case IDM_STORAGE_DRIVE2:   m_shell.SetSecondDriveConnected  (!m_shell.m_disks->ShouldShowExternalDrive()); break;
        case IDM_STORAGE_RECORDER: m_shell.SetTapeRecorderConnected (!m_shell.m_tapeDeck->IsRecorderConnected());  break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenCassque
//
//  Starts the disk browser beside this executable, telling it which emulator
//  launched it. A browser this emulator already launched fronts itself and
//  exits, so a second click brings the first window forward.
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OpenCassque()
{
    Win32ProcessLauncher     launcher;
    std::wstring             moduleDir = PathResolver::GetExecutableDirectory().wstring();
    LaunchCommand::Outcome   outcome   = LaunchCommand::Outcome::Launched;
    std::wstring             message;



    outcome = LaunchCommand::LaunchCassque (launcher, moduleDir, m_shell.m_hwnd, m_shell.m_window->GetWindowTitlePrefix());

    if (outcome == LaunchCommand::Outcome::Missing)
    {
        message = LaunchCommand::DescribeMissing (LaunchCommand::GetSiblingPath (moduleDir, LaunchCommand::kCassqueExe));
        EhmNotifyUser (message.c_str());
    }
    else if (outcome == LaunchCommand::Outcome::Failed)
    {
        message = L"Cassque could not be started\nWindows could not start "
                + LaunchCommand::GetSiblingPath (moduleDir, LaunchCommand::kCassqueExe) + L".";
        EhmNotifyUser (message.c_str());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHelpCommand
//
//  The Help menu: the keyboard map, the update check, and the About box.
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
            DialogDefinition          def     = {};
            KeyboardMapText::Machine  machine = {};

            // Read the capabilities off the live devices rather than a model
            // name, so a machine gains its row the moment it gains the
            // hardware. The //e keyboard is what carries the two Apple keys.
            machine.hasAppleKeys = m_shell.m_machine.GetRefs().iieKeyboard != nullptr;
            machine.hasGamePort  = m_shell.m_machine.GetRefs().iieSoftSwitches != nullptr ||
                                   m_shell.m_machine.GetRefs().gamePort != nullptr;

            def.title = L"Keyboard map";
            def.icon  = DialogIcon::Info;
            def.body  = KeyboardMapText::BuildBody (machine);
            def.buttons.push_back ({ L"OK", 0, true, true });
            (void) m_shell.ShowModalDialog (def);
            break;
        }

        case IDM_HELP_CHECK_UPDATES:
        {
            // Runs on the update service's thread; the result comes back to
            // the shell, which reports it whatever it is.
            m_shell.GetUpdater().CheckForUpdatesNow();
            break;
        }

        case IDM_HELP_ABOUT:
        {
            DialogDefinition def = {};
            def.title = L"About Casso";
            def.icon  = DialogIcon::AppPhotoreal;
            def.iconSizeOverrideDp = 128.0f;
            def.body.push_back ({ L"Casso emulator\nCopyright (C) by Robert Elmer"
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
