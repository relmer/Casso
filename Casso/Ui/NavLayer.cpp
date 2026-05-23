#include "Pch.h"

#include "NavLayer.h"

#include "resource.h"






namespace
{
    constexpr NavCommandEntry s_kEntries[] =
    {
        // File
        { IDM_FILE_OPEN,                NavMenu::File,    L"Switch Machine...",      L"Ctrl+O"        },
        { IDM_FILE_EXIT,                NavMenu::File,    L"Exit",                   nullptr          },

        // Edit
        { IDM_EDIT_COPY_TEXT,           NavMenu::Edit,    L"Copy Text",              L"Ctrl+Shift+C"  },
        { IDM_EDIT_COPY_SCREENSHOT,     NavMenu::Edit,    L"Copy Screenshot",        L"Ctrl+Alt+C"    },
        { IDM_EDIT_PASTE,               NavMenu::Edit,    L"Paste",                  L"Ctrl+V"        },

        // Machine
        { IDM_MACHINE_RESET,            NavMenu::Machine, L"Reset",                  L"Ctrl+R"        },
        { IDM_MACHINE_POWERCYCLE,       NavMenu::Machine, L"Power Cycle",            L"Ctrl+Shift+R"  },
        { IDM_MACHINE_PAUSE,            NavMenu::Machine, L"Pause",                  L"Pause"         },
        { IDM_MACHINE_STEP,             NavMenu::Machine, L"Step",                   L"F11"           },
        { IDM_MACHINE_SPEED_1X,         NavMenu::Machine, L"Speed: 1x (Authentic)",  nullptr          },
        { IDM_MACHINE_SPEED_2X,         NavMenu::Machine, L"Speed: 2x",              nullptr          },
        { IDM_MACHINE_SPEED_MAX,        NavMenu::Machine, L"Speed: Maximum",         nullptr          },
        { IDM_MACHINE_INFO,             NavMenu::Machine, L"Machine Info...",        nullptr          },

        // Disk
        { IDM_DISK_INSERT1,             NavMenu::Disk,    L"Insert Drive 1...",      L"Ctrl+1"        },
        { IDM_DISK_EJECT1,              NavMenu::Disk,    L"Eject Drive 1",          L"Ctrl+Shift+1"  },
        { IDM_DISK_WRITEPROTECT1,       NavMenu::Disk,    L"Write Protect Drive 1",  nullptr          },
        { IDM_DISK_INSERT2,             NavMenu::Disk,    L"Insert Drive 2...",      L"Ctrl+2"        },
        { IDM_DISK_EJECT2,              NavMenu::Disk,    L"Eject Drive 2",          L"Ctrl+Shift+2"  },
        { IDM_DISK_WRITEPROTECT2,       NavMenu::Disk,    L"Write Protect Drive 2",  nullptr          },
        { IDM_DISK_WRITEMODE_BUFFER,    NavMenu::Disk,    L"Write Mode: Buffer and Flush", nullptr    },
        { IDM_DISK_WRITEMODE_COW,       NavMenu::Disk,    L"Write Mode: Copy on Write",    nullptr    },

        // View
        { IDM_VIEW_COLOR,               NavMenu::View,    L"Color (NTSC)",           nullptr          },
        { IDM_VIEW_GREEN,               NavMenu::View,    L"Green Monochrome",       nullptr          },
        { IDM_VIEW_AMBER,               NavMenu::View,    L"Amber Monochrome",       nullptr          },
        { IDM_VIEW_WHITE,               NavMenu::View,    L"White Monochrome",       nullptr          },
        { IDM_VIEW_FULLSCREEN,          NavMenu::View,    L"Fullscreen",             L"Alt+Enter"     },
        { IDM_VIEW_RESET_SIZE,          NavMenu::View,    L"Reset Window Size",      L"Ctrl+0"        },
        { IDM_VIEW_CRT_SHADER,          NavMenu::View,    L"CRT Shader",             nullptr          },
        { IDM_VIEW_SETTINGS,            NavMenu::View,    L"Settings...",            L"Ctrl+,"        },
        { IDM_VIEW_DISKII_DEBUG,        NavMenu::View,    L"Disk II Debug...",       L"Ctrl+Shift+D"  },

        // Help
        { IDM_HELP_KEYMAP,              NavMenu::Help,    L"Keyboard Map",           L"F1"            },
        { IDM_HELP_DEBUG,               NavMenu::Help,    L"Debug Console",          L"Ctrl+D"        },
        { IDM_HELP_ABOUT,               NavMenu::Help,    L"About Casso...",         nullptr          },
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  NavLayer
//
////////////////////////////////////////////////////////////////////////////////

NavLayer::NavLayer ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~NavLayer
//
////////////////////////////////////////////////////////////////////////////////

NavLayer::~NavLayer ()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCommandEntries
//
//  Returns the parity table for traceability + runtime use.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const NavCommandEntry> NavLayer::GetCommandEntries ()
{
    return std::span<const NavCommandEntry> (s_kEntries, std::size (s_kEntries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMenuName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * NavLayer::GetMenuName (NavMenu menu)
{
    switch (menu)
    {
    case NavMenu::File:    return L"File";
    case NavMenu::Edit:    return L"Edit";
    case NavMenu::Machine: return L"Machine";
    case NavMenu::Disk:    return L"Disk";
    case NavMenu::View:    return L"View";
    case NavMenu::Help:    return L"Help";
    }

    return L"?";
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmitParityMarkdown
//
//  Renders the parity table as a markdown document for the spec /
//  traceability tooling.
//
////////////////////////////////////////////////////////////////////////////////

std::string NavLayer::EmitParityMarkdown ()
{
    std::ostringstream  os;



    os << "# Menu Command Parity\n\n";
    os << "Auto-generated by `NavLayer::EmitParityMarkdown` from the "
          "`s_kEntries` table in `Casso/Ui/NavLayer.cpp`. Do not edit by "
          "hand — regenerate via the spec tooling.\n\n";
    os << "| ID | Menu | Label | Accelerator |\n";
    os << "|----|------|-------|-------------|\n";

    for (const NavCommandEntry & e : s_kEntries)
    {
        char            buf[256]   = {};
        const wchar_t * menuName   = GetMenuName (e.menu);
        char            menuBuf[32] = {};
        char            labelBuf[128] = {};
        char            accelBuf[64]  = {};

        WideCharToMultiByte (CP_UTF8, 0, menuName,   -1, menuBuf,  sizeof (menuBuf),  nullptr, nullptr);
        WideCharToMultiByte (CP_UTF8, 0, e.label,    -1, labelBuf, sizeof (labelBuf), nullptr, nullptr);

        if (e.accelerator != nullptr)
        {
            WideCharToMultiByte (CP_UTF8, 0, e.accelerator, -1, accelBuf, sizeof (accelBuf), nullptr, nullptr);
        }

        snprintf (buf, sizeof (buf), "| %u | %s | %s | %s |\n",
                  (unsigned) e.commandId, menuBuf, labelBuf, accelBuf);
        os << buf;
    }

    return os.str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  No-op until the native painter pass takes over chrome rendering.
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Show ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
//  No-op counterpart to Show().
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Hide ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dispatch
//
//  Route a command through the registered callback. No-op when no
//  callback has been installed.
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Dispatch (WORD commandId) const
{
    if (m_dispatch)
    {
        m_dispatch (commandId);
    }
}
