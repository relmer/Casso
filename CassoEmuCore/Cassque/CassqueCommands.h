#pragma once

#include "Pch.h"

#include "Core/DxuiCommand.h"
#include "Core/DxuiStandardCommand.h"
#include "Core/UnicodeSymbols.h"
#include "Widgets/DxuiToolbar.h"
#include "Widgets/DxuiMenuBar.h"
#include "Widgets/DxuiPopupMenu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueCommands
//
//  The browser's command table: every menu row, its accelerator text, and the
//  keys that reach it without the menu.
//
//  THE TABLE DECIDES NOTHING ABOUT STATE. Whether a row is enabled or checked,
//  and what it does, are three functions the window supplies, each handed the
//  command's id. The table owns the commands at fixed addresses, since the
//  menu surfaces hold them by pointer and re-read them at paint time.
//
////////////////////////////////////////////////////////////////////////////////

class CassqueCommands
{
public:
    enum Id : int
    {
        kExit = 100,
        kRefresh,
        kTogglePreview,
        kToggleDisassembly,
        kThemeLight,
        kThemeDark,
        kThemeSystem,
        kBack,
        kForward,
        kUp,
        kAbout,
        kNewTab,
        kCloseTab,
        kNextTab,
        kPreviousTab,
        kThemeSkeuomorphic,
        kThemeDarkModern,
        kThemeRetroTerminal,
        kNamingDescriptive,
        kNamingCiderPress,
        kCopy,
        kSelectAll,
        kGoToOffset,
        kGroup1,
        kGroup2,
        kGroup4,
        kGroup8,
    };

    enum class Menu { File, Edit, View, Go, Help, Count };

    struct Handlers
    {
        std::function<void (int id)>  dispatch;
        std::function<bool (int id)>  isEnabled;
        std::function<bool (int id)>  isChecked;
    };

    explicit CassqueCommands (Handlers handlers);

    CassqueCommands (const CassqueCommands &)             = delete;
    CassqueCommands & operator= (const CassqueCommands &) = delete;

    std::vector<DxuiMenuBarItem>  BuildMenuItems() const;

    //  The toolbar's entries, in strip order: navigation, refresh, a new tab,
    //  and the preview pane's toggle.
    std::vector<DxuiToolbar::Entry>  BuildToolbarEntries() const;

    //  The toolbar entries' commands in strip order, so a host stepping focus
    //  along the strip can ask whether each one can be used.
    static size_t  GetToolbarEntryCount ();
    static int     GetToolbarCommandId  (size_t index);
    const DxuiCommand *           Find (int id) const;

    //  The command a key reaches, or zero. Alt combinations are included, so
    //  a caller asks before offering Alt to the menu bar's mnemonics.
    static int  TranslateKey (WPARAM vk, bool ctrl, bool alt, bool shift);

    //  The standard command a row stands for, or None for a row the window
    //  answers itself.
    static DxuiStandardCommand  GetStandardCommand (int id);

    static const wchar_t *  GetMenuTitle (Menu menu);

private:
    struct Row
    {
        int                  id;
        Menu                 menu;
        const wchar_t      * label;
        const wchar_t      * accelerator;
        bool                 checkable;

        //  Set for a row whose meaning belongs to the focused control rather
        //  than to the window; the window routes it instead of switching on
        //  its id.
        DxuiStandardCommand  standard = DxuiStandardCommand::None;
    };

    struct ToolbarRow
    {
        int                id;
        DxuiToolbar::Kind  kind;
        int                group;
        const wchar_t    * glyph;
        const wchar_t    * shortLabel;
        const wchar_t    * tip;
        bool               iconOnly = false;
    };

    struct Key
    {
        WPARAM  vk;
        bool    ctrl;
        bool    alt;
        bool    shift;
        int     id;
    };

    static constexpr int  kSeparator = 0;

    static constexpr Row  kRows[] =
    {
        { kNewTab,            Menu::File, L"&New tab",            L"Ctrl+T",   false },
        { kCloseTab,          Menu::File, L"&Close tab",          L"Ctrl+W",   false },
        { kSeparator,         Menu::File, nullptr,                nullptr,     false },
        { kExit,              Menu::File, L"E&xit",               nullptr,     false },
        { kCopy,              Menu::Edit, L"&Copy",                L"Ctrl+C",  false, DxuiStandardCommand::Copy },
        { kSelectAll,         Menu::Edit, L"Select &all",          L"Ctrl+A",  false, DxuiStandardCommand::SelectAll },
        { kSeparator,         Menu::Edit, nullptr,                 nullptr,    false },
        { kGoToOffset,        Menu::Edit, L"&Go to offset...",     L"Ctrl+G",  false },
        { kRefresh,           Menu::View, L"&Refresh",            L"F5",       false },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kTogglePreview,     Menu::View, L"&Preview pane",       L"Alt+P",    true  },
        { kToggleDisassembly, Menu::View, L"&Disassemble binary", nullptr,     true  },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kThemeLight,        Menu::View, L"&Light theme",        nullptr,     true  },
        { kThemeDark,         Menu::View, L"Dar&k theme",         nullptr,     true  },
        { kThemeSystem,       Menu::View, L"Follow &system",      nullptr,     true  },
        { kThemeSkeuomorphic, Menu::View, L"Casso &Skeuomorphic (colors only)", nullptr, true },
        { kThemeDarkModern,   Menu::View, L"Casso Dark &Modern",  nullptr,     true  },
        { kThemeRetroTerminal, Menu::View, L"Casso &Retro Terminal", nullptr,  true  },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kGroup1,            Menu::View, L"Bytes grouped by &one",   nullptr,  true },
        { kGroup2,            Menu::View, L"Bytes grouped by &two",   nullptr,  true },
        { kGroup4,            Menu::View, L"Bytes grouped by &four",  nullptr,  true },
        { kGroup8,            Menu::View, L"Bytes grouped by &eight", nullptr,  true },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kNamingDescriptive, Menu::View, L"Descriptive host file &names", nullptr, true },
        { kNamingCiderPress,  Menu::View, L"&CiderPress host file names", nullptr, true },
        { kBack,              Menu::Go,   L"&Back",               L"Alt+Left", false },
        { kForward,           Menu::Go,   L"&Forward",            L"Alt+Right", false },
        { kUp,                Menu::Go,   L"&Up one level",       L"Alt+Up",   false },
        { kAbout,             Menu::Help, L"&About Cassque...",   L"F1",       false },
    };

    static constexpr ToolbarRow  kToolbarRows[] =
    {
        //  Back, Forward, Up and Refresh are one group of bare icons, as in
        //  Explorer; the new tab and the preview toggle keep their labels.
        { kBack,          DxuiToolbar::Kind::Command, 0, s_kpszMdl2Back,    L"Back",    L"Back (Alt+Left)",        true  },
        { kForward,       DxuiToolbar::Kind::Command, 0, s_kpszMdl2Forward, L"Forward", L"Forward (Alt+Right)",    true  },
        { kUp,            DxuiToolbar::Kind::Command, 0, s_kpszMdl2Up,      L"Up",      L"Up one level (Alt+Up)",  true  },
        { kRefresh,       DxuiToolbar::Kind::Command, 0, s_kpszMdl2Refresh, L"Refresh", L"Refresh (F5)",           true  },
        { kNewTab,        DxuiToolbar::Kind::Command, 2, s_kpszMdl2Add,     L"New tab", L"New tab (Ctrl+T)",       false },
        { kTogglePreview, DxuiToolbar::Kind::Toggle,  3, s_kpszMdl2Preview, L"Preview", L"Preview pane (Alt+P)",   false },
    };

    static constexpr Key  kKeys[] =
    {
        { VK_F1,    false, false, false, kAbout         },
        { VK_F5,    false, false, false, kRefresh       },
        { 'P',      false, true,  false, kTogglePreview },
        { VK_LEFT,  false, true,  false, kBack          },
        { VK_RIGHT, false, true,  false, kForward       },
        { VK_UP,    false, true,  false, kUp            },
        { 'T',      true,  false, false, kNewTab        },
        { 'W',      true,  false, false, kCloseTab      },
        { VK_TAB,   true,  false, false, kNextTab       },
        { VK_TAB,   true,  false, true,  kPreviousTab   },
        { 'G',      true,  false, false, kGoToOffset    },
    };

    //  The standard commands are NOT in the key table: DxuiCommandRouter owns
    //  their keystrokes, and the control with focus answers them. The rows
    //  carry the accelerator text so the menu still shows what to press.

    Handlers                                   m_handlers;
    std::vector<std::unique_ptr<DxuiCommand>>  m_commands;
};
