#include "Pch.h"

#include "MenuSystem.h"
#include "Resources/resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MenuSystem
//
////////////////////////////////////////////////////////////////////////////////

MenuSystem::MenuSystem ()
    : m_menuBar     (nullptr),
      m_fileMenu    (nullptr),
      m_machineMenu (nullptr),
      m_diskMenu    (nullptr),
      m_viewMenu    (nullptr),
      m_helpMenu    (nullptr),
      m_speedMode   (SpeedMode::Authentic),
      m_colorMode   (ColorMode::Color),
      m_hwnd        (nullptr)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateMenuBar
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MenuSystem::CreateMenuBar (HWND hwnd)
{
    HRESULT hr = S_OK;

    m_hwnd = hwnd;

    m_menuBar = CreateMenu ();
    CPRA (m_menuBar);

    // File menu
    m_fileMenu = CreatePopupMenu ();
    CPRA (m_fileMenu);
    AppendMenu (m_fileMenu, MF_STRING | MF_GRAYED, IDM_FILE_OPEN, L"&Open Machine Config...\tCtrl+O");
    AppendMenu (m_fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu (m_fileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit");
    AppendMenu (m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR> (m_fileMenu), L"&File");

    // Machine menu
    m_machineMenu = CreatePopupMenu ();
    CPRA (m_machineMenu);
    AppendMenu (m_machineMenu, MF_STRING, IDM_MACHINE_RESET, L"&Reset\tCtrl+R");
    AppendMenu (m_machineMenu, MF_STRING, IDM_MACHINE_POWERCYCLE, L"&Power Cycle\tCtrl+Shift+R");
    AppendMenu (m_machineMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu (m_machineMenu, MF_STRING, IDM_MACHINE_PAUSE, L"P&ause\tPause");
    AppendMenu (m_machineMenu, MF_STRING, IDM_MACHINE_STEP, L"&Step\tF11");
    AppendMenu (m_machineMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu (m_machineMenu, MF_STRING | MF_CHECKED, IDM_MACHINE_SPEED_1X, L"Speed: &1x (Authentic)");
    AppendMenu (m_machineMenu, MF_STRING, IDM_MACHINE_SPEED_2X, L"Speed: &2x");
    AppendMenu (m_machineMenu, MF_STRING, IDM_MACHINE_SPEED_MAX, L"Speed: &Maximum");
    AppendMenu (m_machineMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu (m_machineMenu, MF_STRING, IDM_MACHINE_INFO, L"Machine &Info...");
    AppendMenu (m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR> (m_machineMenu), L"&Machine");

    // Disk menu
    m_diskMenu = CreatePopupMenu ();
    CPRA (m_diskMenu);
    AppendMenu (m_diskMenu, MF_STRING, IDM_DISK_INSERT1, L"Insert Drive &1...\tCtrl+1");
    AppendMenu (m_diskMenu, MF_STRING, IDM_DISK_EJECT1, L"Eject Drive 1\tCtrl+Shift+1");
    AppendMenu (m_diskMenu, MF_STRING, IDM_DISK_WRITEPROTECT1, L"Write Protect Drive 1");
    AppendMenu (m_diskMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu (m_diskMenu, MF_STRING, IDM_DISK_INSERT2, L"Insert Drive &2...\tCtrl+2");
    AppendMenu (m_diskMenu, MF_STRING, IDM_DISK_EJECT2, L"Eject Drive 2\tCtrl+Shift+2");
    AppendMenu (m_diskMenu, MF_STRING, IDM_DISK_WRITEPROTECT2, L"Write Protect Drive 2");
    AppendMenu (m_diskMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu (m_diskMenu, MF_STRING | MF_CHECKED, IDM_DISK_WRITEMODE_BUFFER, L"Write Mode: &Buffer and Flush");
    AppendMenu (m_diskMenu, MF_STRING, IDM_DISK_WRITEMODE_COW, L"Write Mode: &Copy on Write");
    AppendMenu (m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR> (m_diskMenu), L"&Disk");

    // View menu
    m_viewMenu = CreatePopupMenu ();
    CPRA (m_viewMenu);
    AppendMenu (m_viewMenu, MF_STRING | MF_CHECKED, IDM_VIEW_COLOR, L"&Color (NTSC)");
    AppendMenu (m_viewMenu, MF_STRING, IDM_VIEW_GREEN, L"&Green Monochrome");
    AppendMenu (m_viewMenu, MF_STRING, IDM_VIEW_AMBER, L"&Amber Monochrome");
    AppendMenu (m_viewMenu, MF_STRING, IDM_VIEW_WHITE, L"&White Monochrome");
    AppendMenu (m_viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu (m_viewMenu, MF_STRING, IDM_VIEW_FULLSCREEN, L"&Fullscreen\tAlt+Enter");
    AppendMenu (m_viewMenu, MF_STRING, IDM_VIEW_RESET_SIZE, L"Reset &Window Size\tCtrl+0");
    AppendMenu (m_viewMenu, MF_STRING | MF_GRAYED, IDM_VIEW_CRT_SHADER, L"C&RT Shader");
    AppendMenu (m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR> (m_viewMenu), L"&View");

    // Help menu
    m_helpMenu = CreatePopupMenu ();
    CPRA (m_helpMenu);
    AppendMenu (m_helpMenu, MF_STRING, IDM_HELP_KEYMAP, L"&Keyboard Map\tF1");
    AppendMenu (m_helpMenu, MF_STRING, IDM_HELP_DEBUG, L"&Debug Console\tCtrl+D");
    AppendMenu (m_helpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu (m_helpMenu, MF_STRING, IDM_HELP_ABOUT, L"&About Casso65...");
    AppendMenu (m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR> (m_helpMenu), L"&Help");

    {
        BOOL menuResult = SetMenu (hwnd, m_menuBar);
        CWRA (menuResult);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSpeedMode
//
////////////////////////////////////////////////////////////////////////////////

void MenuSystem::SetSpeedMode (SpeedMode mode)
{
    m_speedMode = mode;

    CheckMenuRadioItem (m_machineMenu,
        IDM_MACHINE_SPEED_1X, IDM_MACHINE_SPEED_MAX,
        mode == SpeedMode::Authentic ? IDM_MACHINE_SPEED_1X :
        mode == SpeedMode::Double    ? IDM_MACHINE_SPEED_2X :
                                       IDM_MACHINE_SPEED_MAX,
        MF_BYCOMMAND);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColorMode
//
////////////////////////////////////////////////////////////////////////////////

void MenuSystem::SetColorMode (ColorMode mode)
{
    m_colorMode = mode;

    CheckMenuRadioItem (m_viewMenu,
        IDM_VIEW_COLOR, IDM_VIEW_WHITE,
        mode == ColorMode::Color      ? IDM_VIEW_COLOR :
        mode == ColorMode::GreenMono  ? IDM_VIEW_GREEN :
        mode == ColorMode::AmberMono  ? IDM_VIEW_AMBER :
                                        IDM_VIEW_WHITE,
        MF_BYCOMMAND);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPaused
//
////////////////////////////////////////////////////////////////////////////////

void MenuSystem::SetPaused (bool paused)
{
    if (paused)
    {
        CheckMenuItem (m_machineMenu, IDM_MACHINE_PAUSE, MF_CHECKED);
    }
    else
    {
        CheckMenuItem (m_machineMenu, IDM_MACHINE_PAUSE, MF_UNCHECKED);
    }
}
