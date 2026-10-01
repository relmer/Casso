#pragma once

#include "Pch.h"
#include "Core/DxuiCommand.h"
#include "Widgets/DxuiToolbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBarCommands
//
//  What a memory window's bar shows, after Visual Studio's memory window: the
//  Address box with its history, Refresh, the Columns and grouping drop-downs,
//  and New memory window. The bar is a DxuiToolbar, so what does not fit goes
//  into its "..." menu as on every other strip.
//
//  The window owns the behavior: it hands over one dispatch, one enabled test
//  and one label, each taking an id.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryBarCommands
{
public:
    static constexpr int  kAddress   = 1;
    static constexpr int  kRefresh   = 2;
    static constexpr int  kColumns   = 3;
    static constexpr int  kGrouping  = 4;
    static constexpr int  kNewWindow = 5;

    struct Handlers
    {
        std::function<void (int id)>          dispatch;
        std::function<bool (int id)>          isEnabled;
        std::function<std::wstring (int id)>  getLabel;
    };

    explicit MemoryBarCommands (Handlers handlers);

    //  The address entry draws itself; the bar places it first.
    std::vector<DxuiToolbar::Entry>  BuildEntries (IDxuiToolbarCustomEntry * address) const;

    std::shared_ptr<DxuiCommand>     Find         (int id) const;

    //  The Columns drop-down's choices, values a row, with 0 for Auto.
    static const std::vector<int> &  GetColumnChoices  ();
    static std::wstring              GetColumnsLabel   (int columns);

    //  The grouping drop-down's choices, bytes a value.
    static const std::vector<int> &  GetGroupingChoices ();
    static std::wstring              GetGroupingName    (int grouping);
    static std::wstring              GetGroupingLabel   (int grouping);

private:
    struct Row
    {
        int                  id         = 0;
        const wchar_t      * label      = nullptr;
        const wchar_t      * glyph      = nullptr;
        const wchar_t      * tip        = nullptr;
        DxuiToolbar::Kind    kind       = DxuiToolbar::Kind::Command;
        int                  group      = 0;
        bool                 iconOnly   = false;
    };

    static const std::vector<Row> &  GetRows ();

    Handlers                                   m_handlers;
    std::vector<std::shared_ptr<DxuiCommand>>  m_commands;
};
