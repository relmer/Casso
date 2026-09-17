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
        kEditAddress,
        kLineAddresses,
        kFind,
        kFindNext,
        kNoData,
        kFormatHex,
        kFormatSigned,
        kFormatUnsigned,
        kColumns,
        kColumnsAuto,
        kColumns1,
        kColumns2,
        kColumns4,
        kColumns8,
        kColumns16,
        kZoomIn,
        kZoomOut,
        kZoomReset,

        //  F4: the address opens for editing and its history drops, as in
        //  Explorer. Ctrl+L and Alt+D only open the address.
        kAddressHistory,

        //  The Options dialog, holding the settings that are not views.
        kOptions,

        //  Explorer's command bar: New, the clipboard, Rename and Delete over
        //  the file list's selection, then Sort, View and the theme.
        kNew,
        kCutItems,
        kCopyItems,
        kPasteItems,
        kRenameItem,
        kDeleteItems,
        kSort,
        kView,
        kTheme,
        kNewFolder,
        kNewDisk,
        kSortAscending,
        kSortDescending,
        kViewDetails,

        //  kSortByColumn + the column's index, one per list column.
        kSortByColumn = 400,
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

    //  The toolbar's entries, in strip order: navigation, refresh, and the
    //  preview pane's toggle.
    std::vector<DxuiToolbar::Entry>  BuildToolbarEntries() const;

    //  The preview pane's toolbar: Go to and the byte grouping over a hex
    //  view, or the line address toggle over a BASIC listing.
    std::vector<DxuiToolbar::Entry>  BuildPreviewToolbarEntries (bool hex, IDxuiToolbarCustomEntry * search, IDxuiToolbarCustomEntry * goTo) const;
    static std::vector<int>          GetPreviewToolbarCommandIds (bool hex);

    //  The toolbar entries' commands in strip order, so a host moving focus
    //  along the strip can check whether each one is enabled.
    static size_t  GetToolbarEntryCount ();
    static int     GetToolbarCommandId  (size_t index);
    std::shared_ptr<const DxuiCommand>  Find (int id) const;

    //  The command a key reaches, or zero. Alt combinations are included, so
    //  a caller asks before offering Alt to the menu bar's mnemonics.
    static int  TranslateKey (WPARAM vk, bool ctrl, bool alt, bool shift);

    //  The standard command for a row, or None for a row the window handles
    //  itself.
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
        bool               trailing = false;
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
        { kGoToOffset,        Menu::Edit, L"&Go to",               L"Ctrl+G",  false },
        { kFind,              Menu::Edit, L"&Find",                L"Ctrl+F",  false },
        { kFindNext,          Menu::Edit, L"Find &next",           L"F3",      false },

        //  Rows under Menu::Count are in no menu bar menu; they are the hex
        //  context menu's Columns submenu and its choices.
        { kColumns,           Menu::Count, L"Columns",             nullptr,    false },
        { kColumnsAuto,       Menu::Count, L"&Auto",               nullptr,    true  },
        { kColumns1,          Menu::Count, L"&1",                  nullptr,    true  },
        { kColumns2,          Menu::Count, L"&2",                  nullptr,    true  },
        { kColumns4,          Menu::Count, L"&4",                  nullptr,    true  },
        { kColumns8,          Menu::Count, L"&8",                  nullptr,    true  },
        { kColumns16,         Menu::Count, L"1&6",                 nullptr,    true  },
        { kRefresh,           Menu::View, L"&Refresh",            L"F5",       false },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kTogglePreview,     Menu::View, L"&Preview pane",       L"Alt+P",    true  },
        { kToggleDisassembly, Menu::View, L"&Disassemble binary", nullptr,     true  },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kZoomIn,            Menu::View, L"Zoom &in",            L"Ctrl++",   false },
        { kZoomOut,           Menu::View, L"Zoom &out",           L"Ctrl+-",   false },
        { kZoomReset,         Menu::View, L"Reset &zoom",         L"Ctrl+0",   false },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kThemeLight,        Menu::View, L"&Light theme",        nullptr,     true  },
        { kThemeDark,         Menu::View, L"Dar&k theme",         nullptr,     true  },
        { kThemeSystem,       Menu::View, L"Follow &system",      nullptr,     true  },
        { kThemeSkeuomorphic, Menu::View, L"Casso &Skeuomorphic (colors only)", nullptr, true },
        { kThemeDarkModern,   Menu::View, L"Casso Dark &Modern",  nullptr,     true  },
        { kThemeRetroTerminal, Menu::View, L"Casso &Retro Terminal", nullptr,  true  },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kLineAddresses,     Menu::View, L"Show &address",           nullptr,  true },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kOptions,           Menu::View, L"&Options...",         nullptr,     false },

        //  The command bar's buttons and the rows of its drop-downs.
        { kNew,               Menu::Count, L"New",                 nullptr,     false },
        { kCutItems,          Menu::Count, L"Cut",                 L"Ctrl+X",   false },
        { kCopyItems,         Menu::Count, L"Copy",                L"Ctrl+C",   false },
        { kPasteItems,        Menu::Count, L"Paste",               L"Ctrl+V",   false },
        { kRenameItem,        Menu::Count, L"Rename",              L"F2",       false },
        { kDeleteItems,       Menu::Count, L"Delete",              L"Del",      false },
        { kSort,              Menu::Count, L"Sort",                nullptr,     false },
        { kView,              Menu::Count, L"View",                nullptr,     false },
        { kTheme,             Menu::Count, L"Theme",               nullptr,     false },
        { kNewFolder,         Menu::Count, L"&Folder",             nullptr,     false },
        { kNewDisk,           Menu::Count, L"&Disk image...",      nullptr,     false },
        { kSortAscending,     Menu::Count, L"&Ascending",          nullptr,     true  },
        { kSortDescending,    Menu::Count, L"&Descending",         nullptr,     true  },
        { kViewDetails,       Menu::Count, L"&Details",            nullptr,     true  },
        { kSortByColumn + 0,  Menu::Count, L"&Name",               nullptr,     true  },
        { kSortByColumn + 1,  Menu::Count, L"&Type",               nullptr,     true  },
        { kSortByColumn + 2,  Menu::Count, L"&Size",               nullptr,     true  },
        { kSortByColumn + 3,  Menu::Count, L"A&ddress",            nullptr,     true  },
        { kSortByColumn + 4,  Menu::Count, L"&Locked",             nullptr,     true  },
        { kSortByColumn + 5,  Menu::Count, L"&Modified",           nullptr,     true  },

        //  The hex view's own choices are on its context menu, and nowhere
        //  else at the top of the window.
        { kNoData,            Menu::Count, L"Show &text only",         nullptr,  true },
        { kGroup1,            Menu::Count, L"&1-byte integer",         nullptr,  true },
        { kGroup2,            Menu::Count, L"&2-byte integer",         nullptr,  true },
        { kGroup4,            Menu::Count, L"&4-byte integer",         nullptr,  true },
        { kFormatHex,         Menu::Count, L"&Hexadecimal",            nullptr,  true },
        { kFormatSigned,      Menu::Count, L"&Signed",                 nullptr,  true },
        { kFormatUnsigned,    Menu::Count, L"&Unsigned",               nullptr,  true },
        { kBack,              Menu::Go,   L"&Back",               L"Alt+Left", false },
        { kForward,           Menu::Go,   L"&Forward",            L"Alt+Right", false },
        { kUp,                Menu::Go,   L"&Up one level",       L"Alt+Up",   false },
        { kAbout,             Menu::Help, L"&About Cassque...",   L"F1",       false },
    };

    static constexpr ToolbarRow  kToolbarRows[] =
    {
        //  Back, Forward, Up and Refresh are one group of bare icons, as in
        //  Explorer; the preview toggle keeps its label and sits at the far end,
        //  where Explorer puts Details. A new tab opens from the tab strip.
        { kBack,          DxuiToolbar::Kind::Command, 0, s_kpszMdl2Back,    L"Back",    L"Back (Alt+Left)",        true  },
        { kForward,       DxuiToolbar::Kind::Command, 0, s_kpszMdl2Forward, L"Forward", L"Forward (Alt+Right)",    true  },
        { kUp,            DxuiToolbar::Kind::Command, 0, s_kpszMdl2Up,      L"Up",      L"Up one level (Alt+Up)",  true  },
        { kRefresh,       DxuiToolbar::Kind::Command, 0, s_kpszMdl2Refresh, L"Refresh", L"Refresh (F5)",           true  },
        { kNew,           DxuiToolbar::Kind::DropDown, 1, s_kpszMdl2Add,    L"New",     L"New",                    false },
        { kCutItems,      DxuiToolbar::Kind::Command, 2, s_kpszMdl2Cut,     L"Cut",     L"Cut (Ctrl+X)",           true  },
        { kCopyItems,     DxuiToolbar::Kind::Command, 2, s_kpszMdl2Copy,    L"Copy",    L"Copy (Ctrl+C)",          true  },
        { kPasteItems,    DxuiToolbar::Kind::Command, 2, s_kpszMdl2Paste,   L"Paste",   L"Paste (Ctrl+V)",         true  },
        { kRenameItem,    DxuiToolbar::Kind::Command, 2, s_kpszMdl2Rename,  L"Rename",  L"Rename (F2)",            true  },
        { kDeleteItems,   DxuiToolbar::Kind::Command, 2, s_kpszMdl2Delete,  L"Delete",  L"Delete (Del)",           true  },
        { kSort,          DxuiToolbar::Kind::DropDown, 3, s_kpszMdl2Sort,   L"Sort",    L"Sort",                   false },
        { kView,          DxuiToolbar::Kind::DropDown, 3, s_kpszMdl2List,   L"View",    L"View",                   false },
        { kTogglePreview, DxuiToolbar::Kind::Toggle,  4, s_kpszMdl2Preview, L"Preview", L"Preview pane (Alt+P)",   false, true },
        { kTheme,         DxuiToolbar::Kind::DropDown, 4, s_kpszMdl2Palette, L"Theme",  L"Theme",                  false, true },
    };

    static constexpr ToolbarRow  kPreviewToolbarRows[] =
    {
        { kLineAddresses, DxuiToolbar::Kind::Toggle,  0, nullptr, L"Show address",   L"Show where each line starts in memory" },
        //  The Go to and search boxes, which the host supplies as custom entries.
        { kGoToOffset,    DxuiToolbar::Kind::Command,  0, nullptr, L"Go to",         L""                            },
        { kFind,          DxuiToolbar::Kind::Command,  1, nullptr, L"Search",        L"",                           false, true },
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
        { 'F',      true,  false, false, kFind          },
        { VK_OEM_PLUS,  true, false, false, kZoomIn     },
        { VK_OEM_PLUS,  true, false, true,  kZoomIn     },
        { VK_ADD,       true, false, false, kZoomIn     },
        { VK_OEM_MINUS, true, false, false, kZoomOut    },
        { VK_SUBTRACT,  true, false, false, kZoomOut    },
        { '0',          true, false, false, kZoomReset  },
        { VK_NUMPAD0,   true, false, false, kZoomReset  },
        { VK_F3,    false, false, false, kFindNext      },
        { 'L',      true,  false, false, kEditAddress   },
        { 'D',      false, true,  false, kEditAddress   },
        { VK_F4,    false, false, false, kAddressHistory },
    };

    //  The standard commands are NOT in the key table: DxuiCommandRouter
    //  defines their keystrokes, and the focused control handles them. The
    //  rows still include the accelerator text for the menu.

    void  ApplyToolbarRows (std::span<const ToolbarRow> rows);

    Handlers                                   m_handlers;
    std::vector<std::shared_ptr<DxuiCommand>>  m_commands;
};
