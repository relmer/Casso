#include "Pch.h"

#include "MainMenu.h"

#include "resource.h"
#include "Core/UnicodeSymbols.h"




namespace
{
    // Label syntax: `&X` marks `X` as the menu mnemonic (Win32
    // convention). The `&` is stripped before rendering; the marked
    // glyph is underlined when mnemonic cues should be shown.
    constexpr MainMenuCommandEntry  s_kEntries[] =
    {
        { IDM_PRINTER_EJECT,            MainMenuId::File,    L"&Finish Printing (Eject Paper)", nullptr   },
        { IDM_PRINTER_COPY,             MainMenuId::File,    L"&Copy Printout to Clipboard",    nullptr   },
        { IDM_PRINTER_DISCARD,          MainMenuId::File,    L"&Discard Printout (Tear Off)",   nullptr   },
        { 0,                            MainMenuId::File,    nullptr,                   nullptr          },
        { IDM_FILE_EXIT,                MainMenuId::File,    L"E&xit",                  nullptr          },
        { IDM_EDIT_COPY_TEXT,           MainMenuId::Edit,    L"&Copy text",             L"Ctrl+Shift+C"  },
        { IDM_EDIT_COPY_SCREENSHOT,     MainMenuId::Edit,    L"Copy &screenshot",       L"Ctrl+Alt+C"    },
        { IDM_EDIT_PASTE,               MainMenuId::Edit,    L"&Paste",                 L"Ctrl+V"        },
        { IDM_MACHINE_RESET,            MainMenuId::Machine, L"&Reset",                 L"Ctrl+Shift+R"  },
        { IDM_MACHINE_POWERCYCLE,       MainMenuId::Machine, L"Po&wer cycle",           L"Ctrl+Shift+P"  },
        { IDM_MACHINE_ARROWS_JOYSTICK,  MainMenuId::Machine, L"Map Arrows to &Joystick", L"Ctrl+Shift+J",  true   },
        { IDM_MACHINE_ARROWS_PADDLE,    MainMenuId::Machine, L"Map Mouse to &Paddle",   nullptr,          true   },
        { IDM_DISK_INSERT1,             MainMenuId::Disk,    L"&Insert drive 1...",     L"Ctrl+1"        },
        { IDM_DISK_EJECT1,              MainMenuId::Disk,    L"&Eject drive 1",         L"Ctrl+Shift+1"  },
        { 0,                            MainMenuId::Disk,    nullptr,                   nullptr          },
        { IDM_DISK_INSERT2,             MainMenuId::Disk,    L"Insert drive &2...",     L"Ctrl+2"        },
        { IDM_DISK_EJECT2,              MainMenuId::Disk,    L"Eje&ct drive 2",         L"Ctrl+Shift+2"  },
        { IDM_VIEW_FULLSCREEN,          MainMenuId::View,    L"&Fullscreen",            L"Alt+Enter"     },
        { IDM_VIEW_RESET_SIZE,          MainMenuId::View,    L"&Reset window size",     L"Ctrl+0"        },
        { 0,                            MainMenuId::View,    nullptr,                   nullptr          },
        { IDM_VIEW_SETTINGS,            MainMenuId::View,    L"Se&ttings...",           L"Ctrl+,"        },
        { IDM_HELP_KEYMAP,              MainMenuId::Help,    L"&Keyboard map",          L"F1"            },
        { IDM_HELP_ABOUT,               MainMenuId::Help,    L"&About Casso...",        nullptr          },
        { IDM_MACHINE_PAUSE,            MainMenuId::Debug,   L"&Pause",                 L"Pause"         },
        { IDM_MACHINE_STEP,             MainMenuId::Debug,   L"&Step",                  L"F11"           },
        { IDM_VIEW_DISK2_DEBUG,         MainMenuId::Debug,   L"Disk ][ Debug...",       L"Ctrl+Shift+D"  },
        { IDM_VIEW_INPUT_DEBUG,         MainMenuId::Debug,   L"Input Debug...",         L"Ctrl+Shift+I"  },
    };
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::MainMenu
//
////////////////////////////////////////////////////////////////////////////////

MainMenu::MainMenu ()
{
    Rebuild();
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::~MainMenu
//
////////////////////////////////////////////////////////////////////////////////

MainMenu::~MainMenu ()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::SetDispatch
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::SetDispatch (DispatchFn dispatch)
{
    m_dispatch = std::move (dispatch);
    Rebuild();
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::SetCheckQuery
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::SetCheckQuery (CheckFn query)
{
    m_isChecked = std::move (query);
    Rebuild();
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::GetCommandEntries
//
////////////////////////////////////////////////////////////////////////////////

std::span<const MainMenuCommandEntry> MainMenu::GetCommandEntries ()
{
    return std::span<const MainMenuCommandEntry> (s_kEntries, std::size (s_kEntries));
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::GetMenuName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * MainMenu::GetMenuName (MainMenuId menu)
{
    switch (menu)
    {
    case MainMenuId::File:    return L"&File";
    case MainMenuId::Edit:    return L"&Edit";
    case MainMenuId::Machine: return L"&Machine";
    case MainMenuId::Disk:    return L"&Disk";
    case MainMenuId::View:    return L"&View";
    case MainMenuId::Help:    return L"&Help";
    case MainMenuId::Debug:   return L"&Debug";
    }

    return L"?";
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::EmitParityMarkdown
//
////////////////////////////////////////////////////////////////////////////////

std::string MainMenu::EmitParityMarkdown ()
{
    std::ostringstream  os;



    os << "# Menu Command Parity\n\n";
    os << "Auto-generated by `MainMenu::EmitParityMarkdown` from the `s_kEntries` table. Do not edit by hand.\n\n";
    os << "| ID | Menu | Label | Accelerator |\n";
    os << "|----|------|-------|-------------|\n";

    for (const MainMenuCommandEntry & e : s_kEntries)
    {
        char            buf[256]      = {};
        const wchar_t * menuName      = GetMenuName (e.menu);
        std::wstring    menuStripped;
        std::wstring    labelStripped;
        int             mnIdx         = -1;
        wchar_t         mnCh          = 0;
        char            menuBuf[32]   = {};
        char            labelBuf[128] = {};
        char            accelBuf[64]  = {};

        if (IsSeparator (e))
        {
            continue;
        }

        DxuiMenuBar::ParseMnemonic (menuName,  menuStripped,  mnIdx, mnCh);
        DxuiMenuBar::ParseMnemonic (e.label,   labelStripped, mnIdx, mnCh);

        WideCharToMultiByte (CP_UTF8, 0, menuStripped.c_str(),  -1, menuBuf,  sizeof (menuBuf),  nullptr, nullptr);
        WideCharToMultiByte (CP_UTF8, 0, labelStripped.c_str(), -1, labelBuf, sizeof (labelBuf), nullptr, nullptr);

        if (e.accelerator != nullptr)
        {
            WideCharToMultiByte (CP_UTF8, 0, e.accelerator, -1, accelBuf, sizeof (accelBuf), nullptr, nullptr);
        }

        snprintf (buf, sizeof (buf), "| %u | %s | %s | %s |\n", (unsigned) e.commandId, menuBuf, labelBuf, accelBuf);
        os << buf;
    }

    return os.str();
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::IsSeparator
//
////////////////////////////////////////////////////////////////////////////////

bool MainMenu::IsSeparator (const MainMenuCommandEntry & entry)
{
    return entry.commandId == 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::Show
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::Show ()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::Dispatch
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::Dispatch (WORD commandId) const
{
    if (m_dispatch)
    {
        m_dispatch (commandId);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::Open
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::Open (MainMenuId menu, bool openedByKeyboard)
{
    DxuiMenuBar::Open ((int) menu, openedByKeyboard);
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::PaintStrip
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::PaintStrip (
    DxuiPainter             & painter,
    DxuiTextRenderer        & text,
    const ChromeVisualState & visual,
    const CassoTheme       & theme)
{
    ApplyChromeColors (theme);
    DxuiMenuBar::PaintStrip (painter, text, theme, visual.dpi);
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::PaintDropdown
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::PaintDropdown (
    DxuiPainter             & painter,
    DxuiTextRenderer        & text,
    const ChromeVisualState & visual,
    const CassoTheme       & theme)
{
    ApplyChromeColors (theme);
    DxuiMenuBar::PaintDropdown (painter, text, theme, visual.dpi);
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::Rebuild
//
//  Materializes the Casso `s_kEntries` table into the generic
//  `DxuiMenuBar` item shape. Each subitem's dispatch lambda captures
//  the entry's `commandId` and the MainMenu instance so it can fan
//  into the stored Casso-level dispatch callback.
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::Rebuild ()
{
    std::vector<DxuiMenuBarItem>  items;



    items.reserve (kMenuCount);

    for (int m = 0; m < kMenuCount; m++)
    {
        DxuiMenuBarItem  topItem;

        topItem.label = GetMenuName ((MainMenuId) m);

        for (const MainMenuCommandEntry & e : s_kEntries)
        {
            DxuiMenuBarSubitem  sub;

            if (e.menu != (MainMenuId) m)
            {
                continue;
            }

            if (IsSeparator (e))
            {
                sub.isSeparator = true;
                topItem.submenu.push_back (std::move (sub));
                continue;
            }

            sub.label       = e.label;
            sub.hotkey      = (e.accelerator != nullptr) ? std::wstring (e.accelerator) : std::wstring();
            sub.enabled     = true;
            sub.checkable   = e.checkable;
            sub.isSeparator = false;

            WORD  commandId = e.commandId;

            sub.dispatch = [this, commandId] ()
            {
                Dispatch (commandId);
            };

            if (e.checkable)
            {
                sub.isChecked = [this, commandId] () -> bool
                {
                    return m_isChecked ? m_isChecked (commandId) : false;
                };
            }

            topItem.submenu.push_back (std::move (sub));
        }

        items.push_back (std::move (topItem));
    }

    SetItems (std::move (items));
}




////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::ApplyChromeColors
//
//  Pushes the Casso-specific nav / dropdown palette onto the menu bar
//  so visual parity with the legacy chrome is preserved across paint.
//  The generic `IDxuiTheme` mapping is close but not identical, so we
//  drive the overrides every frame; cheap and tolerant of theme swaps.
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::ApplyChromeColors (const CassoTheme & theme)
{
    SetStripColors    (theme.navStrip,
                       theme.navHover,
                       theme.navItemText);
    SetDropdownColors (theme.dropdownBg,
                       theme.dropdownHover,
                       theme.dropdownItemText,
                       theme.dropdownAccel,
                       theme.navHover,
                       theme.navHover);
}
