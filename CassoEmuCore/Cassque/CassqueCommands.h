#pragma once

#include "Pch.h"

#include "Core/DxuiCommand.h"
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
    };

    enum class Menu { File, View, Go, Help, Count };

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
    const DxuiCommand *           Find (int id) const;

    //  The command a key reaches, or zero. Alt combinations are included, so
    //  a caller asks before offering Alt to the menu bar's mnemonics.
    static int  TranslateKey (WPARAM vk, bool ctrl, bool alt, bool shift);

    static const wchar_t *  GetMenuTitle (Menu menu);

private:
    struct Row
    {
        int              id;
        Menu             menu;
        const wchar_t  * label;
        const wchar_t  * accelerator;
        bool             checkable;
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
        { kExit,              Menu::File, L"E&xit",               nullptr,     false },
        { kRefresh,           Menu::View, L"&Refresh",            L"F5",       false },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kTogglePreview,     Menu::View, L"&Preview pane",       L"Alt+P",    true  },
        { kToggleDisassembly, Menu::View, L"&Disassemble binary", nullptr,     true  },
        { kSeparator,         Menu::View, nullptr,                nullptr,     false },
        { kThemeLight,        Menu::View, L"&Light theme",        nullptr,     true  },
        { kThemeDark,         Menu::View, L"Dar&k theme",         nullptr,     true  },
        { kThemeSystem,       Menu::View, L"Follow &system",      nullptr,     true  },
        { kBack,              Menu::Go,   L"&Back",               L"Alt+Left", false },
        { kForward,           Menu::Go,   L"&Forward",            L"Alt+Right", false },
        { kUp,                Menu::Go,   L"&Up one level",       L"Alt+Up",   false },
        { kAbout,             Menu::Help, L"&About Cassque...",   nullptr,     false },
    };

    static constexpr Key  kKeys[] =
    {
        { VK_F5,    false, false, false, kRefresh       },
        { 'P',      false, true,  false, kTogglePreview },
        { VK_LEFT,  false, true,  false, kBack          },
        { VK_RIGHT, false, true,  false, kForward       },
        { VK_UP,    false, true,  false, kUp            },
    };

    Handlers                                   m_handlers;
    std::vector<std::unique_ptr<DxuiCommand>>  m_commands;
};
