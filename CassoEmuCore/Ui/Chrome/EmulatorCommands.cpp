#include "Pch.h"

#include "EmulatorCommands.h"

#include "resource.h"
#include "Core/UnicodeSymbols.h"

#include "PrinterStatusLed.h"
#include "VolumeFlyout.h"




// Label syntax: `&X` marks `X` as the menu mnemonic (Win32
// convention). The `&` is stripped before rendering; the marked
// glyph is underlined when mnemonic cues should be shown.
//
// A 30-line table, so it stays a file-scope `static constexpr` under the
// documented 3+ line exception rather than moving onto the class.
static constexpr EmulatorMenuEntry  s_kMenuEntries[] =
{
    { IDM_PRINTER_PREVIEW,          MainMenuId::File,    L"Show &printer preview",  nullptr          },
    { IDM_PRINTER_COPY,             MainMenuId::File,    L"&Copy printout to clipboard",    nullptr   },
    { IDM_PRINTER_DISCARD,          MainMenuId::File,    L"&Discard printout (tear off)",   nullptr   },
    { 0,                            MainMenuId::File,    nullptr,                   nullptr          },
    { IDM_FILE_EXIT,                MainMenuId::File,    L"E&xit",                  nullptr          },
    { IDM_EDIT_COPY_TEXT,           MainMenuId::Edit,    L"&Copy text",             L"Ctrl+Shift+C"  },
    { IDM_EDIT_COPY_SCREENSHOT,     MainMenuId::Edit,    L"Copy &screenshot",       L"Ctrl+Alt+C"    },
    { IDM_EDIT_PASTE,               MainMenuId::Edit,    L"&Paste",                 L"Ctrl+V"        },
    { IDM_MACHINE_RESET,            MainMenuId::Machine, L"&Reset",                 L"Ctrl+Shift+R"  },
    { IDM_MACHINE_POWERCYCLE,       MainMenuId::Machine, L"Po&wer cycle",           L"Ctrl+Shift+P"  },
    { IDM_MACHINE_ARROWS_JOYSTICK,  MainMenuId::Machine, L"Map arrows to &joystick", L"Ctrl+Shift+J",  true   },
    { IDM_MACHINE_ARROWS_PADDLE,    MainMenuId::Machine, L"Map mouse to &paddle",   nullptr,          true   },
    { 0,                            MainMenuId::Machine, nullptr,                   nullptr          },
    { IDM_VIEW_CONTROLLER_SETTINGS, MainMenuId::Machine, L"&Controller settings...", nullptr         },
    { IDM_DISK_INSERT1,             MainMenuId::Disk,    L"&Insert drive 1...",     L"Ctrl+1"        },
    { IDM_DISK_EJECT1,              MainMenuId::Disk,    L"&Eject drive 1",         L"Ctrl+Shift+1"  },
    { IDM_DISK_WP1,                 MainMenuId::Disk,    L"&Write-protect disk 1",  nullptr          },
    { IDM_DISK_SALVAGE1,            MainMenuId::Disk,    L"Sa&lvage readable sectors...", nullptr    },
    { 0,                            MainMenuId::Disk,    nullptr,                   nullptr          },
    { IDM_DISK_INSERT2,             MainMenuId::Disk,    L"Insert drive &2...",     L"Ctrl+2"        },
    { IDM_DISK_EJECT2,              MainMenuId::Disk,    L"Eje&ct drive 2",         L"Ctrl+Shift+2"  },
    { IDM_DISK_WP2,                 MainMenuId::Disk,    L"Write-&protect disk 2",  nullptr          },
    { IDM_DISK_SALVAGE2,            MainMenuId::Disk,    L"Salvage readable sec&tors...", nullptr    },
    { 0,                            MainMenuId::Disk,    nullptr,                   nullptr          },
    { IDM_DISK_OPEN_CASSQUE,        MainMenuId::Disk,    L"Browse disks in Cassq&ue...", nullptr     },
    { IDM_VIEW_FULLSCREEN,          MainMenuId::View,    L"&Full screen",           L"Alt+Enter"     },
    { IDM_VIEW_DRIVE_STRIP,         MainMenuId::View,    L"Drive &strip (full screen)", L"Ctrl+D"      },
    { IDM_VIEW_RESET_SIZE,          MainMenuId::View,    L"&Reset view",            L"Ctrl+0"        },
    { IDM_VIEW_FRAME_RATE,          MainMenuId::View,    L"Frame &rate",            nullptr,          true   },
    { IDM_VIEW_SCENE_VIEW,          MainMenuId::View,    L"Scene &pose",            nullptr,          true   },
    { 0,                            MainMenuId::View,    nullptr,                   nullptr          },
    { IDM_VIEW_SETTINGS,            MainMenuId::View,    L"Se&ttings...",           L"Ctrl+,"        },
    { IDM_HELP_KEYMAP,              MainMenuId::Help,    L"&Keyboard map",          L"F1"            },
    { IDM_HELP_ABOUT,               MainMenuId::Help,    L"&About Casso...",        nullptr          },
    { IDM_MACHINE_PAUSE,            MainMenuId::Debug,   L"&Pause",                 L"Pause"         },
    { IDM_MACHINE_STEP,             MainMenuId::Debug,   L"&Step",                  L"F11"           },
    { IDM_VIEW_DISK2_DEBUG,         MainMenuId::Debug,   L"Disk ][ Debug...",       L"Ctrl+Shift+D"  },
    { IDM_VIEW_INPUT_DEBUG,         MainMenuId::Debug,   L"Input debug...",         L"Ctrl+Shift+I"  },
    { IDM_VIEW_DEBUGGER,            MainMenuId::Debug,   L"Debugger...",            L""              },
    { IDM_DEBUG_RESTART,            MainMenuId::Debug,   L"&Restart under debugger", L""              },
};

// The toolbar's entries, in strip order. The order is also the COLLAPSE
// order read backwards: the last entry gives up its label first. A row
// whose id is a menu command's shares that command; the others are the
// toolbar's own. The short label is what the strip draws; the pickers
// label themselves with their PURPOSE, not with the value they hold, since
// a label that changes with the value moves every button to its right.
//
// The paddle picker is the one exception: it wears the source that is
// driving, because that answer is worth a permanent place on the strip and
// having it there is what lets the input cluster stop carrying it. Its width
// moves as a result, so the strip is laid out again whenever it changes, and
// the source labels are capped (InputModeRules::kShortLabelLimit) so the
// movement stays small.
struct ToolbarRow
{
    int                id;
    DxuiToolbar::Kind  kind;
    int                group;
    const wchar_t *    glyph;
    const wchar_t *    shortLabel;
    const wchar_t *    label;        // the toolbar-only entries' full label
};

// Segoe MDL2 Assets codepoints.
static constexpr const wchar_t * s_kGlyphSettings   = L"\uE713";   // gear
static constexpr const wchar_t * s_kGlyphTheme      = L"\uE746";   // half-filled square: light / dark
static constexpr const wchar_t * s_kGlyphScreenshot = L"\uE722";   // camera
static constexpr const wchar_t * s_kGlyphReset      = L"\uE72C";   // refresh arrow
static constexpr const wchar_t * s_kGlyphPower      = L"\uE7E8";   // power symbol
static constexpr const wchar_t * s_kGlyphVolume     = L"\uE767";   // speaker
static constexpr const wchar_t * s_kGlyphMuted      = L"\uE74F";   // muted speaker
static constexpr const wchar_t * s_kGlyphPrint      = L"\uE749";   // printer (monoline, matches the set)
static constexpr const wchar_t * s_kGlyphColor      = L"\uE790";   // artist's palette
static constexpr const wchar_t * s_kGlyphFullscreen = L"\uE740";   // diagonal arrows, outward
static constexpr const wchar_t * s_kGlyphRestore    = L"\uE73F";   // diagonal arrows, inward
static constexpr const wchar_t * s_kGlyphMouse      = L"\uE962";   // mouse: the one input device MDL2 draws better than we can

static constexpr ToolbarRow  s_kToolbarRows[] =
{
    { IDM_VIEW_SETTINGS,           DxuiToolbar::Kind::Command,  0, s_kGlyphSettings,   L"Settings",    nullptr          },
    { EmulatorCommands::kIdTheme,  DxuiToolbar::Kind::DropDown, 0, s_kGlyphTheme,      L"Theme",       L"Theme"         },
    { EmulatorCommands::kIdColor,  DxuiToolbar::Kind::DropDown, 0, s_kGlyphColor,      L"Color",       L"Color"         },
    { IDM_PRINTER_PREVIEW,         DxuiToolbar::Kind::Command,  0, s_kGlyphPrint,      L"Printer",     nullptr          },
    { EmulatorCommands::kIdVolume, DxuiToolbar::Kind::Flyout,   1, s_kGlyphVolume,     L"Volume",      L"Mute"          },
    { EmulatorCommands::kIdPaddle, DxuiToolbar::Kind::DropDown, 2, nullptr,            L"Controller",  L"Joystick and paddle source" },
    { EmulatorCommands::kIdMouse,  DxuiToolbar::Kind::Toggle,   2, s_kGlyphMouse,      L"Mouse",       L"Mouse" },
    { IDM_VIEW_FULLSCREEN,         DxuiToolbar::Kind::Command,  3, s_kGlyphFullscreen, L"Full screen", nullptr          },
    { IDM_EDIT_COPY_SCREENSHOT,    DxuiToolbar::Kind::Command,  3, s_kGlyphScreenshot, L"Screenshot",  nullptr          },
    { IDM_MACHINE_RESET,           DxuiToolbar::Kind::Command,  3, s_kGlyphReset,      L"Reset",       nullptr          },
    { IDM_MACHINE_POWERCYCLE,      DxuiToolbar::Kind::Command,  3, s_kGlyphPower,      L"Power",       nullptr          },
};

// The monitor-color rows. Settings spells the monochrome ones out in full;
// on a strip this narrow the phosphor name alone carries it.
static constexpr const wchar_t * s_kMonitorColorRows[] =
{
    L"Color",
    L"Green",
    L"Amber",
    L"White",
};





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::EmulatorCommands
//
//  Materializes the menu table into one DxuiCommand per entry, then gives
//  every toolbar row its glyph and short label, creating the four commands
//  the menu does not have. Each command's functors read the sinks live, so
//  a sink set later is honored without rebuilding.
//
//  A checkable entry is the ONLY kind that gets an `isChecked` functor. The
//  menu reserves its check gutter for a list with any checkable row, so
//  giving every command one would push every menu's labels right.
//
////////////////////////////////////////////////////////////////////////////////

EmulatorCommands::EmulatorCommands()
{
    for (const EmulatorMenuEntry & e : s_kMenuEntries)
    {
        std::shared_ptr<DxuiCommand>  cmd;
        WORD                          commandId   = e.commandId;
        std::wstring                  staticLabel;

        if (IsSeparator (e))
        {
            continue;
        }

        cmd              = std::make_shared<DxuiCommand>();
        cmd->id          = commandId;
        cmd->label       = e.label;
        cmd->accelerator = (e.accelerator != nullptr) ? std::wstring (e.accelerator) : std::wstring();
        staticLabel      = cmd->label;

        cmd->dispatch = [this, commandId] ()
        {
            Dispatch (commandId);
        };

        if (e.checkable)
        {
            cmd->isChecked = [this, commandId] () -> bool
            {
                return m_isChecked ? m_isChecked (commandId) : false;
            };
        }

        cmd->isEnabled = [this, commandId] () -> bool
        {
            return m_isEnabled ? m_isEnabled (commandId) : true;
        };

        cmd->labelText = [this, commandId, staticLabel] () -> std::wstring
        {
            std::wstring  dynamic = m_labelQuery ? m_labelQuery (commandId)
                                                 : std::wstring();

            return dynamic.empty() ? staticLabel : dynamic;
        };

        m_commands.push_back (std::move (cmd));
    }

    for (const ToolbarRow & row : s_kToolbarRows)
    {
        std::shared_ptr<DxuiCommand>  cmd = FindMutable (row.id);

        if (cmd == nullptr)
        {
            std::shared_ptr<DxuiCommand>  own = std::make_shared<DxuiCommand>();

            own->id    = row.id;
            own->label = row.label;
            cmd        = own;
            m_commands.push_back (std::move (own));
        }

        cmd->glyph      = row.glyph;
        cmd->shortLabel = row.shortLabel;
    }

    // The paddle picker wears the source that is driving, not the word for
    // what it is for. With the answer on its face there is nothing left for a
    // separate indicator to say. It keeps its static label as the fallback
    // for when nothing is driving the axes at all.
    {
        std::shared_ptr<DxuiCommand>  paddle = FindMutable (kIdPaddle);

        if (paddle != nullptr)
        {
            paddle->shortLabel.clear();
            paddle->labelText = [this] () { return GetCheckedPaddleSourceLabel(); };

            // A labeled slot shows only an explicit tip, so the word for what
            // the picker is for moves there once the face wears the answer.
            paddle->tip = paddle->label;
        }
    }

    // The paddle picker's Profiles submenu, and its New... row.
    m_profilesRow           = std::make_shared<DxuiCommand>();
    m_profilesRow->label    = L"Profiles";
    m_newProfileRow         = std::make_shared<DxuiCommand>();
    m_newProfileRow->label  = L"New...";
    m_newProfileRow->dispatch = [this] ()
    {
        if (m_onNewProfile)
        {
            m_onNewProfile();
        }
    };

    for (size_t i = 0; i < std::size (s_kMonitorColorRows); i++)
    {
        std::shared_ptr<DxuiCommand>  cmd = std::make_shared<DxuiCommand>();

        cmd->id        = (int) i;
        cmd->label     = s_kMonitorColorRows[i];
        cmd->isChecked = [this, i] () { return (int) i == m_colorIndex; };

        m_colorRows.push_back (std::move (cmd));
    }

    RebuildActionTips();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::GetMenuEntries / GetMenuName / IsSeparator
//
////////////////////////////////////////////////////////////////////////////////

std::span<const EmulatorMenuEntry> EmulatorCommands::GetMenuEntries()
{
    return std::span<const EmulatorMenuEntry> (s_kMenuEntries, std::size (s_kMenuEntries));
}


const wchar_t * EmulatorCommands::GetMenuName (MainMenuId menu)
{
    // The & marks the Alt accelerator; "?" is a visible placeholder for a
    // menu id this function has not been taught about.
    const wchar_t *  name = L"?";



    switch (menu)
    {
    case MainMenuId::File:    name = L"&File";    break;
    case MainMenuId::Edit:    name = L"&Edit";    break;
    case MainMenuId::Machine: name = L"&Machine"; break;
    case MainMenuId::Disk:    name = L"&Disk";    break;
    case MainMenuId::View:    name = L"&View";    break;
    case MainMenuId::Help:    name = L"&Help";    break;
    case MainMenuId::Debug:   name = L"&Debug";   break;
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::IsSeparator
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorCommands::IsSeparator (const EmulatorMenuEntry & entry)
{
    return entry.commandId == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::EmitParityMarkdown
//
//  Emits the menu-command parity table as Markdown, generated from the same
//  entry table the menu itself is built from.
//
//  Generated rather than hand-written precisely because a hand-maintained
//  table of commands drifts the moment anyone adds a menu item. Reading the
//  live table means the document cannot disagree with the product -- and the
//  header says so, so nobody edits the output.
//
//  Mnemonic ampersands are STRIPPED from both the menu and the label, since
//  they are display markup for the menu bar and would read as literal
//  ampersands in Markdown.
//
//  Separators are skipped: they are layout, not commands, and have no id,
//  label, or accelerator to report.
//
//  Text is converted to UTF-8 because the output is a Markdown file; the menu
//  itself is wide throughout.
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorCommands::EmitParityMarkdown()
{
    std::ostringstream  os;



    os << "# Menu Command Parity\n\n";
    os << "Auto-generated by `EmulatorCommands::EmitParityMarkdown` from the menu entry table. Do not edit by hand.\n\n";
    os << "| ID | Menu | Label | Accelerator |\n";
    os << "|----|------|-------|-------------|\n";

    for (const EmulatorMenuEntry & e : s_kMenuEntries)
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
//  EmulatorCommands::Dispatch
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCommands::Dispatch (WORD commandId) const
{
    if (m_dispatch)
    {
        m_dispatch (commandId);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::Find / FindMutable
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiCommand> EmulatorCommands::Find (int commandId) const
{
    for (const std::shared_ptr<DxuiCommand> & cmd : m_commands)
    {
        if (cmd->id == commandId)
        {
            return cmd;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::FindMutable
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> EmulatorCommands::FindMutable (int commandId)
{
    for (const std::shared_ptr<DxuiCommand> & cmd : m_commands)
    {
        if (cmd->id == commandId)
        {
            return cmd;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::RebuildActionTips
//
//  Reset and Power say which machine they act on, so the tips are composed
//  rather than fixed. Reset's also carries the reboot chord on a line of its
//  own, written as the two keycaps and nothing else: a chord is a picture of
//  what to hold. Which host key stands in for the apple is the keyboard map's
//  job, and repeating it in every tip that mentions the key made each one wrap.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCommands::RebuildActionTips()
{
    std::wstring                  machine = m_machineName.empty() ? std::wstring (L"machine") : m_machineName;
    std::wstring                  apple   = DxuiTextRenderer::HasSymbolFont()
                                                ? std::wstring (s_kpszOpenApple)
                                                : std::wstring (L"Open Apple");
    std::shared_ptr<DxuiCommand>  reset   = FindMutable (IDM_MACHINE_RESET);
    std::shared_ptr<DxuiCommand>  power   = FindMutable (IDM_MACHINE_POWERCYCLE);



    if (reset != nullptr)
    {
        reset->tip = L"Reset the " + machine + L".\n" + apple + L" + Reset to reboot.";
    }

    if (power != nullptr)
    {
        power->tip = L"Power-cycle the " + machine;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::SetMachineDisplayName / SetFullscreen / SetMuted
//
//  One fullscreen button covers both directions, so its glyph and short
//  label follow the presentation the click would LEAVE, not the one it is
//  in. The volume entry's full label is the ACTION its click takes, which is
//  what its collapsed form shows as a tip.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCommands::SetMachineDisplayName (const std::wstring & displayName)
{
    if (displayName != m_machineName)
    {
        m_machineName = displayName;
        RebuildActionTips();
    }
}


void EmulatorCommands::SetFullscreen (bool fullscreen)
{
    std::shared_ptr<DxuiCommand>  cmd = FindMutable (IDM_VIEW_FULLSCREEN);



    if (cmd != nullptr)
    {
        cmd->glyph      = fullscreen ? s_kGlyphRestore    : s_kGlyphFullscreen;
        cmd->shortLabel = fullscreen ? L"Exit full screen" : L"Full screen";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::SetMuted
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCommands::SetMuted (bool muted)
{
    std::shared_ptr<DxuiCommand>  cmd = FindMutable (kIdVolume);



    if (cmd != nullptr)
    {
        cmd->glyph = muted ? s_kGlyphMuted : s_kGlyphVolume;
        cmd->label = muted ? L"Unmute"     : L"Mute";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::SetThemeNames
//
//  One row per theme, checked while it is the active one. The rows are
//  rebuilt only here, so a toolbar holding the previous list must be handed
//  GetThemeItems() again by the caller.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCommands::SetThemeNames (const std::vector<std::wstring> & displayNames)
{
    m_themeRows.clear();

    for (size_t i = 0; i < displayNames.size(); i++)
    {
        std::shared_ptr<DxuiCommand>  cmd = std::make_shared<DxuiCommand>();

        cmd->id        = (int) i;
        cmd->label     = displayNames[i];
        cmd->isChecked = [this, i] () { return (int) i == m_themeIndex; };

        m_themeRows.push_back (std::move (cmd));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::BuildMenuItems
//
//  One item list per title, in the table's order under each.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiMenuBarItem> EmulatorCommands::BuildMenuItems() const
{
    std::vector<DxuiMenuBarItem>  items;



    items.reserve (kMenuCount);

    for (int m = 0; m < kMenuCount; m++)
    {
        DxuiMenuBarItem  topItem;

        topItem.label = GetMenuName ((MainMenuId) m);

        for (const EmulatorMenuEntry & e : s_kMenuEntries)
        {
            if (e.menu != (MainMenuId) m)
            {
                continue;
            }

            if (IsSeparator (e))
            {
                topItem.submenu.push_back (DxuiPopupMenuItem::ForSeparator());
            }
            else
            {
                topItem.submenu.push_back (DxuiPopupMenuItem::ForCommand (Find (e.commandId)));
            }
        }

        items.push_back (std::move (topItem));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::GetThemeItems / GetMonitorItems
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> EmulatorCommands::GetThemeItems() const
{
    std::vector<DxuiPopupMenuItem>  items;



    for (const std::shared_ptr<DxuiCommand> & cmd : m_themeRows)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (cmd));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::GetMonitorItems
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> EmulatorCommands::GetMonitorItems() const
{
    std::vector<DxuiPopupMenuItem>  items;



    for (const std::shared_ptr<DxuiCommand> & cmd : m_colorRows)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (cmd));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::SetPaddleSources
//
//  One row per entry of the paddle-source list, checked while it is the one
//  driving. The rows carry no ids of their own: a controller comes and goes,
//  so a row is identified by the entry it was built from, which the dispatch
//  captures.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCommands::SetPaddleSources (const std::vector<InputModeRules::PaddleSource> & sources)
{
    size_t  i = 0;



    m_paddleSources = sources;

    //  Dropping the previous rows frees only those nothing else holds. A menu
    //  on screen shares ownership of its rows, and a dispatching surface holds
    //  the row it is dispatching, so a rebuild during either leaves them alive.
    m_paddleSourceRows.clear();

    for (i = 0; i < m_paddleSources.size(); i++)
    {
        std::shared_ptr<DxuiCommand>  cmd    = std::make_shared<DxuiCommand>();
        InputModeRules::PaddleSource  source = m_paddleSources[i];

        cmd->id    = (int) i;
        cmd->label = source.label;

        //  EACH ROW CARRIES ITS OWN SOURCE BY VALUE, rather than an index to
        //  look up when it is clicked. The list is rebuilt whenever a
        //  controller comes or goes, and that changes its LENGTH, so an
        //  index captured when the row was built names a DIFFERENT source
        //  afterwards. A user picking "Use keys as joystick" off a list built
        //  a moment earlier landed on "Use mouse as paddle", which takes the
        //  pointer. A row now does what it says, whatever the list did since.
        cmd->isChecked = [source] () { return source.isChecked; };

        cmd->dispatch  = [this, source] ()
        {
            if (m_onPaddleSourcePicked)
            {
                m_onPaddleSourcePicked (source);
            }
        };

        m_paddleSourceRows.push_back (std::move (cmd));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::SetProfileSections
//
//  Default always leads a section, whether or not its list carries it or
//  where: the service plays Default for any name a model lacks, so it is
//  always a real choice. Sections are kept apart by a separator, and New...
//  closes the list below one more.
//
//  A menu on screen shares ownership of its rows, so dropping ours frees only
//  the rows nothing else still holds.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCommands::SetProfileSections (std::vector<ProfileSection> sections)
{
    const char *  pszDefault = ControllerProfile::kpszDefaultName;



    m_profileSections = std::move (sections);
    m_profileItems.clear();

    for (const ProfileSection & section : m_profileSections)
    {
        std::vector<std::string>  names;
        bool                      isActiveListed = false;

        if (!m_profileItems.empty())
        {
            m_profileItems.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        if (!section.header.empty())
        {
            m_profileItems.push_back (DxuiPopupMenuItem::ForHeader (section.header));
        }

        names.push_back (pszDefault);

        for (const std::string & name : section.names)
        {
            if (_stricmp (name.c_str(), pszDefault) != 0)
            {
                names.push_back (name);
            }
        }

        // An empty active name is Default, and so is a name the model does
        // not have, which is what the service plays in that case.
        for (const std::string & name : names)
        {
            isActiveListed = isActiveListed || _stricmp (name.c_str(), section.active.c_str()) == 0;
        }

        for (size_t i = 0; i < names.size(); i++)
        {
            std::shared_ptr<DxuiCommand>  cmd       = std::make_shared<DxuiCommand>();
            std::string                   name      = names[i];
            ControllerUnitKey             unit      = section.unit;
            bool                          isDefault = (i == 0);
            bool                          isChecked = isActiveListed ? _stricmp (name.c_str(), section.active.c_str()) == 0
                                                                     : isDefault;

            cmd->id        = (int) i;
            cmd->label     = TextEncoding::NarrowToWide (name);
            cmd->isChecked = [isChecked] () { return isChecked; };

            // The row carries its controller and name by value rather than an
            // index, so a row from a list rebuilt since still picks the
            // profile it shows, for the controller it was shown for.
            cmd->dispatch  = [this, unit, name, isDefault] ()
            {
                if (m_onProfilePicked)
                {
                    m_onProfilePicked (unit, isDefault ? std::string() : name);
                }
            };

            m_profileItems.push_back (DxuiPopupMenuItem::ForCommand (cmd));
        }
    }

    if (!m_profileItems.empty())
    {
        m_profileItems.push_back (DxuiPopupMenuItem::ForSeparator());
        m_profileItems.push_back (DxuiPopupMenuItem::ForCommand (m_newProfileRow));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::GetProfileItems
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> EmulatorCommands::GetProfileItems() const
{
    return m_profileItems;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::SetMouseModeFns
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCommands::SetMouseModeFns (std::function<bool()> isOn,
                                        std::function<bool()> isOffered,
                                        std::function<void()> toggle)
{
    std::shared_ptr<DxuiCommand>  mouse = FindMutable (kIdMouse);



    if (mouse == nullptr)
    {
        return;
    }

    mouse->isChecked = std::move (isOn);
    mouse->isEnabled = std::move (isOffered);
    mouse->dispatch  = std::move (toggle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::SetJoyportFns
//
//  The row's check is asked each time the menu draws, so it shows whether
//  the Joyport is attached however it came to be -- from this row or from
//  the Machine tab.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCommands::SetJoyportFns (std::function<bool()> isOn,
                                      std::function<bool()> isOffered,
                                      std::function<void()> toggle)
{
    m_joyportRow = std::make_shared<DxuiCommand>();

    m_joyportRow->label     = L"Sirius Joyport";
    m_joyportRow->isChecked = std::move (isOn);
    m_joyportRow->dispatch  = std::move (toggle);
    m_isJoyportOffered      = std::move (isOffered);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::GetCheckedPaddleSourceLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EmulatorCommands::GetCheckedPaddleSourceLabel() const
{
    return InputModeRules::GetPaddleSourceLabel (m_paddleSources);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::GetCheckedPaddleSourceGlyph
//
////////////////////////////////////////////////////////////////////////////////

InputMonoGlyphKind EmulatorCommands::GetCheckedPaddleSourceGlyph() const
{
    for (const InputModeRules::PaddleSource & source : m_paddleSources)
    {
        if (!source.isChecked)
        {
            continue;
        }

        if (source.isArrowKeys)
        {
            return InputMonoGlyphKind::Keys;
        }

        if (source.isMousePaddle)
        {
            return InputMonoGlyphKind::Paddle;
        }

        // A wheel draws as a joystick: an icon for a device almost nobody
        // will plug into an Apple II is not worth a drawing of its own.
        return (source.formFactor == ControllerFormFactor::Gamepad)
                   ? InputMonoGlyphKind::Gamepad
                   : InputMonoGlyphKind::Joystick;
    }

    return InputMonoGlyphKind::Gamepad;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::GetPaddleSourceItems
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> EmulatorCommands::GetPaddleSourceItems() const
{
    std::vector<DxuiPopupMenuItem>  items;



    for (const std::shared_ptr<DxuiCommand> & cmd : m_paddleSourceRows)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (cmd));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::GetPaddlePickerItems
//
//  What the paddle picker lists: every source, then Controller settings...
//  below a separator, so the settings for the controller in use are one
//  click from where it was chosen.
//
//  The two-player row sits below a separator of its own. The rows above it
//  are the one thing that drives the game port; it is the mode where two
//  things do, so grouping it with them would read as a third source.
//
//  The Sirius Joyport row has a group of its own below them, on a machine that
//  can take one. It is not something that drives the game port but a device
//  on it, attached whichever source drives, so beside the sources it would
//  read as one more of them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> EmulatorCommands::GetPaddlePickerItems() const
{
    std::vector<DxuiPopupMenuItem>      items;
    std::vector<DxuiPopupMenuItem>      rows      = GetPaddleSourceItems();
    std::shared_ptr<const DxuiCommand>  settings  = Find (IDM_VIEW_CONTROLLER_SETTINGS);
    size_t                              i         = 0;
    bool                                isJoyport = m_joyportRow != nullptr && m_isJoyportOffered && m_isJoyportOffered();



    for (i = 0; i < rows.size(); i++)
    {
        if (i < m_paddleSources.size() && m_paddleSources[i].isMultiplayer)
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        items.push_back (rows[i]);
    }

    if (isJoyport)
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        items.push_back (DxuiPopupMenuItem::ForCommand (m_joyportRow));
    }

    // The profiles of the controllers in play, one submenu away from the
    // controllers themselves, above the settings that edit them.
    if (!m_profileItems.empty() || settings != nullptr)
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    if (!m_profileItems.empty())
    {
        items.push_back (DxuiPopupMenuItem::ForSubmenu (m_profilesRow, m_profileItems));
    }

    if (settings != nullptr)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (settings));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommands::BuildToolbar
//
//  The printer entry follows card presence: no card, no printer button. The
//  volume entry's click toggles mute on the flyout, handled there rather
//  than dispatched, because the flyout owns state the slider reads back and
//  a round trip through the command path would put it behind its own
//  control. The input rows dispatch on their own, so that picker needs no
//  sink.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCommands::BuildToolbar (DxuiToolbar       & toolbar,
                                     PrinterStatusLed  & led,
                                     VolumeFlyout      & volume)
{
    std::vector<DxuiToolbar::Entry>  entries;
    std::shared_ptr<DxuiCommand>     printer = FindMutable (IDM_PRINTER_PREVIEW);
    std::shared_ptr<DxuiCommand>     mute    = FindMutable (kIdVolume);



    if (printer != nullptr)
    {
        printer->isEnabled = [&led] () { return led.IsPresent(); };
    }

    if (mute != nullptr)
    {
        mute->dispatch = [&volume] () { volume.ToggleMute(); };
    }

    for (const ToolbarRow & row : s_kToolbarRows)
    {
        DxuiToolbar::Entry  e;

        e.command = Find (row.id);
        e.kind    = row.kind;
        e.group   = row.group;

        if (row.id == IDM_PRINTER_PREVIEW) { e.decoration = led.MakeDecoration(); }

        // The picker draws its own icon, because the icon tracks the DEVICE:
        // a gamepad, a stick, the paddle or the arrow keys (FR-008b). A font
        // glyph cannot follow that, and MDL2 has no Apple paddle anyway. It
        // goes through the strip's icon path rather than a decoration, so
        // layout reserves the icon's room and the label starts after it, and
        // it takes the entry's disabled and hover ink.
        if (row.id == kIdPaddle)
        {
            e.icon = [this] (IDxuiPainter & painter, const DxuiToolbarIconBox & icon)
            {
                RECT  box = { (LONG) icon.x,
                              (LONG) (icon.top + (icon.rowH - icon.size) * 0.5f),
                              (LONG) (icon.x + icon.size),
                              (LONG) (icon.top + (icon.rowH + icon.size) * 0.5f) };

                InputMonoGlyphs::Paint (painter, GetCheckedPaddleSourceGlyph(), box, icon.ink);
            };
        }


        entries.push_back (std::move (e));
    }

    toolbar.SetEntries       (std::move (entries));
    toolbar.SetFlyoutControl (kIdVolume, &volume, VolumeFlyout::kPanelDp);
    toolbar.SetDropDownItems (kIdTheme, GetThemeItems());
    toolbar.SetDropDownItems (kIdColor, GetMonitorItems());
    toolbar.SetDropDownItems (kIdPaddle, GetPaddlePickerItems());
}
