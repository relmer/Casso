#include "Pch.h"

#include "NavLayer.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Parity table
//
//  Source of truth for both the runtime nav layer and the markdown
//  traceability doc. Every IDM_* that lives in Casso/resource.h and
//  is wired into MenuSystem.cpp must appear here. The unit test enforces
//  that invariant by iterating the known IDM_ id set and asserting
//  membership.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr NavCommandEntry kEntries[] =
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


    constexpr const char * kNavLayerRml = R"RML(
<rml>
    <head>
        <title>Casso Nav Layer</title>
        <style>
            body
            {
                width: 100%;
                height: 28dp;
                background-color: #2a2a2a;
                color: #e8e8e8;
                font-family: sans-serif;
                font-size: 12dp;
            }
            .nav-strip { display: block; height: 28dp; }
            .nav-item
            {
                display: inline-block;
                padding: 6dp 12dp;
                color: #e8e8e8;
            }
            .nav-item:hover { background-color: #3a3a3a; }
        </style>
    </head>
    <body>
        <div class="nav-strip">
            <span class="nav-item">File</span>
            <span class="nav-item">Edit</span>
            <span class="nav-item">Machine</span>
            <span class="nav-item">Disk</span>
            <span class="nav-item">View</span>
            <span class="nav-item">Help</span>
        </div>
    </body>
</rml>
)RML";
}





////////////////////////////////////////////////////////////////////////////////
//
//  NavLayer
//
////////////////////////////////////////////////////////////////////////////////

NavLayer::NavLayer()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~NavLayer
//
////////////////////////////////////////////////////////////////////////////////

NavLayer::~NavLayer()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCommandEntries
//
//  Pure-data parity table — visible to the test suite via
//  GetCommandEntries().
//
////////////////////////////////////////////////////////////////////////////////

std::span<const NavCommandEntry> NavLayer::GetCommandEntries()
{
    return std::span<const NavCommandEntry> (kEntries, std::size (kEntries));
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
//  Translates the parity table into a markdown table writeable to
//  specs/007-ui-overhaul/menu-command-parity.md. Returns the string;
//  caller decides what to do with it. Pure function — no I/O.
//
////////////////////////////////////////////////////////////////////////////////

std::string NavLayer::EmitParityMarkdown()
{
    std::ostringstream  os;


    os << "# Menu Command Parity Table\n\n";
    os << "Auto-generated by `NavLayer::EmitParityMarkdown` from the "
          "`kEntries` table in `Casso/Ui/NavLayer.cpp`. Do not edit by "
          "hand — re-run the generator after adding a new IDM_ command "
          "(see `NavLayerTraceabilityTests`).\n\n";
    os << "| Command ID | Hex | Menu | Label | Accelerator |\n";
    os << "|-----------:|----:|:-----|:------|:------------|\n";

    for (const NavCommandEntry & e : kEntries)
    {
        char  hexBuf[16] = {};
        snprintf (hexBuf, sizeof (hexBuf), "0x%04X", e.commandId);

        os << "| " << e.commandId
           << " | " << hexBuf
           << " | ";

        // Narrow conversion good enough for ASCII menu labels.
        const wchar_t * menuName = GetMenuName (e.menu);

        for (const wchar_t * p = menuName; *p; ++p)
        {
            os << (char) *p;
        }

        os << " | ";

        for (const wchar_t * p = e.label; *p; ++p)
        {
            os << (char) *p;
        }

        os << " | ";

        if (e.accelerator != nullptr)
        {
            for (const wchar_t * p = e.accelerator; *p; ++p)
            {
                os << (char) *p;
            }
        }
        else
        {
            os << "—";
        }

        os << " |\n";
    }

    return os.str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NavLayer::Show (Rml::Context * context, DispatchFn dispatch)
{
    HRESULT  hr = S_OK;


    CPR (context);

    if (m_doc != nullptr)
    {
        Hide();
    }

    m_context  = context;
    m_dispatch = std::move (dispatch);
    m_doc      = m_context->LoadDocumentFromMemory (kNavLayerRml,
                                                     "nav_layer.rml");
    CPR (m_doc);

    m_doc->Show();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Hide()
{
    if (m_doc != nullptr && m_context != nullptr)
    {
        m_context->UnloadDocument (m_doc);
    }

    m_doc      = nullptr;
    m_context  = nullptr;
    m_dispatch = {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dispatch
//
//  Dispatch a command through the registered callback. No-op if
//  dispatch was never installed. Public so tests / future RML
//  event listeners can exercise routing.
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Dispatch (WORD commandId) const
{
    if (m_dispatch)
    {
        m_dispatch (commandId);
    }
}
