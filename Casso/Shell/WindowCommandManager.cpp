#include "Pch.h"

#include "WindowCommandManager.h"

#include "../AssetBootstrap.h"
#include "../EmulatorShell.h"
#include "../resource.h"
#include "../Shell/DiskMru.h"
#include "Version.h"
#include "Ui/Chrome/LayoutManager.h"
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
//  Public command-pump entry point. Used by the NavLayer so click
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

    if      (id >= IDM_EDIT_COPY_TEXT && id <= IDM_EDIT_PASTE)       { OnEditCommand (id); }
    else if (id >= IDM_FILE_OPEN     && id <= IDM_FILE_EXIT)          { OnFileCommand (id); }
    else if (id >= IDM_MACHINE_RESET && id <= IDM_MACHINE_INFO)       { OnMachineCommand (id); }
    else if (id >= IDM_DISK_INSERT1  && id <= IDM_DISK_WRITEPROTECT2) { OnDiskCommand (id); }
    else if (id >= IDM_VIEW_COLOR    && id <= IDM_VIEW_SETTINGS)      { OnViewCommand (id); }
    else if (id >= IDM_HELP_KEYMAP   && id <= IDM_HELP_ABOUT)         { OnHelpCommand (id); }

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
                // (linear scale), with every chrome contributor's
                // inset summed by the single source of truth. The
                // LayoutManager owns the framebuffer scale policy --
                // see ClientSizeForFramebuffer for the one-line
                // toggle to switch to integer-only scaling.
                {
                    SIZE  desired = m_shell.m_layout.ClientSizeForFramebuffer (
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
            HRESULT  hrShow = m_shell.m_settingsPanel.Show();
            IGNORE_RETURN_VALUE (hrShow, S_OK);
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
    HRESULT                          hr          = S_OK;
    DiskMru                          mru;
    std::vector<std::filesystem::path>  mruExisting;
    std::filesystem::path            diskDir;
    std::wstring                     chosenPath;
    std::string                      error;
    bool                             userBrowsed = false;


    diskDir     = AssetBootstrap::GetDiskDirectory();
    mru         = DiskMru::FromUtf8 (m_shell.m_globalPrefs.recentDisks);
    mruExisting = mru.Prune ([] (const std::filesystem::path & p)
                             {
                                 return std::filesystem::exists (p);
                             });

    hr = AssetBootstrap::PromptInsertDiskMru (GetModuleHandle (nullptr),
                                              m_shell.m_hwnd,
                                              drive,
                                              mruExisting,
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
                L"Ctrl+R -> Reset\n"
                L"Ctrl+Alt+R -> Autoboot reset (cold boot from disk)\n"
                L"Ctrl+Shift+R -> Power cycle\n"
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
