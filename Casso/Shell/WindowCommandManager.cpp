#include "Pch.h"

#include "WindowCommandManager.h"

#include "../EmulatorShell.h"
#include "../resource.h"
#include "Version.h"
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
    constexpr int  s_kFramebufferWidthPx   = 560;
    constexpr int  s_kFramebufferHeightPx  = 384;
    constexpr int  s_kNavStripHeightDp     = 28;
    constexpr int  s_kBaselineDpi          = 96;


    int  ComputeChromeTopInsetPx (UINT dpi)
    {
        int  navStrip = MulDiv (s_kNavStripHeightDp, static_cast<int> (dpi), s_kBaselineDpi);

        return navStrip;
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
    else if (id >= IDM_VIEW_COLOR    && id <= IDM_VIEW_DISKII_DEBUG)  { OnViewCommand (id); }
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
            m_shell.m_menuSystem.SetPaused (paused);
            m_shell.UpdateWindowTitle();
            break;
        }

        case IDM_MACHINE_STEP:
        {
            if (m_shell.m_cpuManager.IsPaused())
            {
                m_shell.PostCommand (static_cast<WORD> (id));
            }
            break;
        }

        case IDM_MACHINE_SPEED_1X:
        {
            m_shell.m_cpuManager.SetSpeedMode (SpeedMode::Authentic);
            m_shell.m_menuSystem.SetSpeedMode (SpeedMode::Authentic);
            break;
        }

        case IDM_MACHINE_SPEED_2X:
        {
            m_shell.m_cpuManager.SetSpeedMode (SpeedMode::Double);
            m_shell.m_menuSystem.SetSpeedMode (SpeedMode::Double);
            break;
        }

        case IDM_MACHINE_SPEED_MAX:
        {
            m_shell.m_cpuManager.SetSpeedMode (SpeedMode::Maximum);
            m_shell.m_menuSystem.SetSpeedMode (SpeedMode::Maximum);
            break;
        }

        case IDM_MACHINE_INFO:
        {
            std::wstring info = std::format (
                L"Machine: {}\n"
                L"CPU: {}\n"
                L"Clock Speed: {} Hz\n"
                L"Memory Regions: {}\n"
                L"Devices: {}",
                std::wstring (m_shell.m_config.name.begin(), m_shell.m_config.name.end()),
                std::wstring (m_shell.m_config.cpu.begin(), m_shell.m_config.cpu.end()),
                m_shell.m_config.clockSpeed,
                (m_shell.m_config.ram.size() + 1 + m_shell.m_config.slots.size()),
                (m_shell.m_config.internalDevices.size() + m_shell.m_config.slots.size()));

            MessageBoxW (m_shell.m_hwnd, info.c_str(), L"Machine Info", MB_ICONINFORMATION | MB_OK);
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
    UINT        dpi   = 0;
    int         scale = 0;
    RECT        rc    = {};
    DWORD       style = 0;
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
            m_shell.m_menuSystem.SetColorMode (ColorMode::Color);
            break;
        }

        case IDM_VIEW_GREEN:
        {
            m_shell.m_colorMode.store (ColorMode::GreenMono, std::memory_order_release);
            m_shell.m_menuSystem.SetColorMode (ColorMode::GreenMono);
            break;
        }

        case IDM_VIEW_AMBER:
        {
            m_shell.m_colorMode.store (ColorMode::AmberMono, std::memory_order_release);
            m_shell.m_menuSystem.SetColorMode (ColorMode::AmberMono);
            break;
        }

        case IDM_VIEW_WHITE:
        {
            m_shell.m_colorMode.store (ColorMode::WhiteMono, std::memory_order_release);
            m_shell.m_menuSystem.SetColorMode (ColorMode::WhiteMono);
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
                dpi   = GetDpiForWindow (m_shell.m_hwnd);
                scale = (dpi + 48) / 96;

                if (scale < 1)
                {
                    scale = 1;
                }

                rc    = { 0, 0,
                          s_kFramebufferWidthPx * scale,
                          s_kFramebufferHeightPx * scale + ComputeChromeTopInsetPx (dpi) };
                style = static_cast<DWORD> (GetWindowLong (m_shell.m_hwnd, GWL_STYLE));
                AdjustWindowRectExForDpi (&rc, style, FALSE, 0, dpi);

                w = rc.right - rc.left;
                h = rc.bottom - rc.top;

                hMon = MonitorFromWindow (m_shell.m_hwnd, MONITOR_DEFAULTTONEAREST);
                GetMonitorInfo (hMon, &mi);

                x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
                y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;

                SetWindowPos (m_shell.m_hwnd, nullptr, x, y, w, h, SWP_NOZORDER);
            }
            break;
        }

        case IDM_VIEW_DISKII_DEBUG:
        {
            m_shell.OpenDiskIIDebugDialog();
            break;
        }

        case IDM_VIEW_SETTINGS:
        {
            m_shell.ShowMachinePicker();
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
    COMDLG_FILTERSPEC                filters[1] = { { L"Disk Images", L"*.dsk;*.nib;*.woz;*.po" } };



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

    hr = m_shell.Mount (6, drive, pszPath);
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
//  OnDiskCommand
//
////////////////////////////////////////////////////////////////////////////////

void WindowCommandManager::OnDiskCommand (int id)
{
    WCHAR          filePath[MAX_PATH] = {};
    OPENFILENAMEW  ofn                = {};



    switch (id)
    {
        case IDM_DISK_INSERT1:
        case IDM_DISK_INSERT2:
        {
            ofn.lStructSize = sizeof (ofn);
            ofn.hwndOwner   = m_shell.m_hwnd;
            ofn.lpstrFilter = L"Disk Images (*.dsk)\0*.dsk\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile   = filePath;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrTitle  = (id == IDM_DISK_INSERT1) ?
                L"Insert Disk in Drive 1" : L"Insert Disk in Drive 2";
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileNameW (&ofn))
            {
                m_shell.PostCommand (static_cast<WORD> (id), fs::path (filePath).string());
            }
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
        case IDM_HELP_DEBUG:
        {
            if (m_shell.m_debugConsole.IsVisible())
            {
                m_shell.m_debugConsole.Hide();
            }
            else
            {
                if (m_shell.m_debugConsole.Show (m_shell.m_hInstance))
                {
                    m_shell.m_debugConsole.LogConfig (
                        std::format ("Machine: {}\nCPU: {}\nClock: {} Hz\nDevices: {}",
                            m_shell.m_config.name, m_shell.m_config.cpu, m_shell.m_config.clockSpeed,
                            (m_shell.m_config.internalDevices.size() + m_shell.m_config.slots.size())));
                }
            }
            break;
        }

        case IDM_HELP_KEYMAP:
        {
            MessageBoxW (m_shell.m_hwnd,
                L"PC Key Mapping:\n\n"
                L"Arrow Keys -> Apple ][ cursor movement\n"
                L"Enter -> Return\n"
                L"Escape -> Escape\n"
                L"Delete -> Delete\n"
                L"Ctrl+Reset -> Warm Reset\n"
                L"Left Alt -> Open Apple (//e)\n"
                L"Right Alt -> Closed Apple (//e)\n\n"
                L"Emulator Controls:\n"
                L"Ctrl+R -> Reset\n"
                L"Ctrl+Alt+R -> Autoboot Reset (cold boot from disk)\n"
                L"Ctrl+Shift+R -> Power Cycle\n"
                L"Pause -> Pause/Resume\n"
                L"F11 -> Step (when paused)\n"
                L"Alt+Enter -> Fullscreen\n"
                L"Ctrl+0 -> Reset Window Size\n"
                L"Ctrl+D -> Debug Console",
                L"Keyboard Map", MB_ICONINFORMATION | MB_OK);
            break;
        }

        case IDM_HELP_ABOUT:
        {
            MessageBoxW (m_shell.m_hwnd,
                L"Casso Apple ][ Emulator\n"
                L"Version " _CRT_WIDE (VERSION_STRING) L"\n"
                L"Built " _CRT_WIDE (VERSION_BUILD_TIMESTAMP) L"\n\n"
                L"An Apple ][ / ][+ / //e platform emulator built on\n"
                L"the Casso 6502 assembler/emulator project.\n\n"
                L"https://github.com/relmer/Casso",
                L"About Casso", MB_ICONINFORMATION | MB_OK);
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

    m_shell.m_menuSystem.UpdateDynamicMenuItems (m_shell.m_config);

    return true;
}
