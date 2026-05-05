#include "Pch.h"

#include "MenuSystem.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MenuSystem
//
////////////////////////////////////////////////////////////////////////////////

MenuSystem::MenuSystem ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Menu item table
//
////////////////////////////////////////////////////////////////////////////////

static constexpr UINT kSep = 0xFFFF;   // Sentinel: insert a separator

struct MenuItem
{
    UINT         flags;
    UINT         id;
    const wchar_t * label;
};

static const MenuItem kFileMenuItems[] =
{
    { MF_STRING, IDM_FILE_OPEN,   L"&Switch Machine...\tCtrl+O" },
    { 0,                     kSep,            nullptr },
    { MF_STRING,             IDM_FILE_EXIT,   L"E&xit" },
};

static const MenuItem kEditMenuItems[] =
{
    { MF_STRING, IDM_EDIT_COPY_TEXT,       L"Copy &Text\tCtrl+Shift+C" },
    { MF_STRING, IDM_EDIT_COPY_SCREENSHOT, L"Copy &Screenshot\tCtrl+Alt+C" },
    { 0,         kSep,                     nullptr },
    { MF_STRING, IDM_EDIT_PASTE,           L"&Paste\tCtrl+V" },
};

static const MenuItem kMachineMenuItems[] =
{
    { MF_STRING,              IDM_MACHINE_RESET,     L"&Reset\tCtrl+R" },
    { MF_STRING,              IDM_MACHINE_POWERCYCLE, L"&Power Cycle\tCtrl+Shift+R" },
    { 0,                      kSep,                  nullptr },
    { MF_STRING,              IDM_MACHINE_PAUSE,     L"P&ause\tPause" },
    { MF_STRING,              IDM_MACHINE_STEP,      L"&Step\tF11" },
    { 0,                      kSep,                  nullptr },
    { MF_STRING | MF_CHECKED, IDM_MACHINE_SPEED_1X,  L"Speed: &1x (Authentic)" },
    { MF_STRING,              IDM_MACHINE_SPEED_2X,  L"Speed: &2x" },
    { MF_STRING,              IDM_MACHINE_SPEED_MAX, L"Speed: &Maximum" },
    { 0,                      kSep,                  nullptr },
    { MF_STRING,              IDM_MACHINE_INFO,      L"Machine &Info..." },
};

static const MenuItem kDiskMenuItems[] =
{
    { MF_STRING,              IDM_DISK_INSERT1,         L"Insert Drive &1...\tCtrl+1" },
    { MF_STRING,              IDM_DISK_EJECT1,          L"Eject Drive 1\tCtrl+Shift+1" },
    { MF_STRING,              IDM_DISK_WRITEPROTECT1,   L"Write Protect Drive 1" },
    { 0,                      kSep,                     nullptr },
    { MF_STRING,              IDM_DISK_INSERT2,         L"Insert Drive &2...\tCtrl+2" },
    { MF_STRING,              IDM_DISK_EJECT2,          L"Eject Drive 2\tCtrl+Shift+2" },
    { MF_STRING,              IDM_DISK_WRITEPROTECT2,   L"Write Protect Drive 2" },
    { 0,                      kSep,                     nullptr },
    { MF_STRING | MF_CHECKED, IDM_DISK_WRITEMODE_BUFFER, L"Write Mode: &Buffer and Flush" },
    { MF_STRING,              IDM_DISK_WRITEMODE_COW,   L"Write Mode: &Copy on Write" },
};

static const MenuItem kViewMenuItems[] =
{
    { MF_STRING | MF_CHECKED, IDM_VIEW_COLOR,      L"&Color (NTSC)" },
    { MF_STRING,              IDM_VIEW_GREEN,      L"&Green Monochrome" },
    { MF_STRING,              IDM_VIEW_AMBER,      L"&Amber Monochrome" },
    { MF_STRING,              IDM_VIEW_WHITE,      L"&White Monochrome" },
    { 0,                      kSep,                nullptr },
    { MF_STRING,              IDM_VIEW_FULLSCREEN, L"&Fullscreen\tAlt+Enter" },
    { MF_STRING,              IDM_VIEW_RESET_SIZE, L"Reset &Window Size\tCtrl+0" },
    { MF_STRING | MF_GRAYED,  IDM_VIEW_CRT_SHADER, L"C&RT Shader" },
};

static const MenuItem kHelpMenuItems[] =
{
    { MF_STRING, IDM_HELP_KEYMAP, L"&Keyboard Map\tF1" },
    { MF_STRING | MF_GRAYED, IDM_HELP_DEBUG,  L"&Debug Console\tCtrl+D" },
    { 0,         kSep,            nullptr },
    { MF_STRING, IDM_HELP_ABOUT,  L"&About Casso..." },
};

struct MenuDef
{
    HMENU *         pMenu;
    const wchar_t * label;
    const MenuItem * items;
    size_t          count;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BuildPopupMenu
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT BuildPopupMenu (HMENU parent, const MenuDef & def)
{
    HRESULT hr       = S_OK;
    HMENU   menu     = nullptr;
    BOOL    fSuccess = FALSE;



    menu = CreatePopupMenu ();
    CPRA (menu);

    for (size_t i = 0; i < def.count; i++)
    {
        if (def.items[i].id == kSep)
        {
            fSuccess = AppendMenu (menu, MF_SEPARATOR, 0, nullptr);
            CWRA (fSuccess);
        }
        else
        {
            fSuccess = AppendMenu (menu, def.items[i].flags, def.items[i].id, def.items[i].label);
            CWRA (fSuccess);
        }
    }

    fSuccess = AppendMenu (parent, MF_POPUP, reinterpret_cast<UINT_PTR> (menu), def.label);
    CWRA (fSuccess);

    *def.pMenu = menu;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateMenuBar
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MenuSystem::CreateMenuBar (HWND hwnd)
{
    HRESULT hr       = S_OK;
    BOOL    fSuccess = FALSE;

    const MenuDef menus[] =
    {
        { &m_fileMenu,    L"&File",    kFileMenuItems,    _countof (kFileMenuItems) },
        { &m_editMenu,    L"&Edit",    kEditMenuItems,    _countof (kEditMenuItems) },
        { &m_machineMenu, L"&Machine", kMachineMenuItems, _countof (kMachineMenuItems) },
        { &m_diskMenu,    L"&Disk",    kDiskMenuItems,    _countof (kDiskMenuItems) },
        { &m_viewMenu,    L"&View",    kViewMenuItems,    _countof (kViewMenuItems) },
        { &m_helpMenu,    L"&Help",    kHelpMenuItems,    _countof (kHelpMenuItems) },
    };



    m_hwnd = hwnd;

    m_menuBar = CreateMenu ();
    CPRA (m_menuBar);

    for (size_t i = 0; i < _countof (menus); i++)
    {
        hr = BuildPopupMenu (m_menuBar, menus[i]);
        CHR (hr);
    }

    fSuccess = SetMenu (hwnd, m_menuBar);
    CWRA (fSuccess);

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
    HRESULT hr       = S_OK;
    BOOL    fSuccess = FALSE;



    m_speedMode = mode;

    fSuccess = CheckMenuRadioItem (m_machineMenu,
                                   IDM_MACHINE_SPEED_1X, IDM_MACHINE_SPEED_MAX,
                                   mode == SpeedMode::Authentic ? IDM_MACHINE_SPEED_1X :
                                   mode == SpeedMode::Double    ? IDM_MACHINE_SPEED_2X :
                                                                   IDM_MACHINE_SPEED_MAX,
                                   MF_BYCOMMAND);
    CWRA (fSuccess);

Error:
    ;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColorMode
//
////////////////////////////////////////////////////////////////////////////////

void MenuSystem::SetColorMode (ColorMode mode)
{
    HRESULT hr       = S_OK;
    BOOL    fSuccess = FALSE;



    m_colorMode = mode;

    fSuccess = CheckMenuRadioItem (m_viewMenu,
                                   IDM_VIEW_COLOR, IDM_VIEW_WHITE,
                                   mode == ColorMode::Color      ? IDM_VIEW_COLOR :
                                   mode == ColorMode::GreenMono  ? IDM_VIEW_GREEN :
                                   mode == ColorMode::AmberMono  ? IDM_VIEW_AMBER :
                                                                    IDM_VIEW_WHITE,
                                   MF_BYCOMMAND);
    CWRA (fSuccess);

Error:
    ;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPaused
//
////////////////////////////////////////////////////////////////////////////////

void MenuSystem::SetPaused (bool paused)
{
    CheckMenuItem (m_machineMenu, IDM_MACHINE_PAUSE, paused ? MF_CHECKED : MF_UNCHECKED);
}





