#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeBarCommands
//
//  What the heat map ranges pane's bar shows, as Visual Studio lays out a
//  tool window's strip: the set being edited as a drop-down with New set,
//  Rename set and Delete set beside it, then New, Edit and Remove for its
//  ranges and Move up and Move down for their order.
//
//  The window owns the behavior: it hands over one dispatch, one enabled
//  test and one label, each taking an id.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapRangeBarCommands
{
public:
    static constexpr int  kSet       = 1;
    static constexpr int  kNewSet    = 2;
    static constexpr int  kRenameSet = 3;
    static constexpr int  kDeleteSet = 4;
    static constexpr int  kNew       = 5;
    static constexpr int  kEdit      = 6;
    static constexpr int  kRemove    = 7;
    static constexpr int  kUp        = 8;
    static constexpr int  kDown      = 9;

    struct Handlers
    {
        std::function<void (int id)>          dispatch;
        std::function<bool (int id)>          isEnabled;
        std::function<std::wstring (int id)>  getLabel;
    };

    explicit HeatMapRangeBarCommands (Handlers handlers);

    std::vector<DxuiToolbar::Entry>  BuildEntries () const;
    std::shared_ptr<DxuiCommand>     Find         (int id) const;

    //  The set drop-down's label: "Set: Game", or "No sets" with none made.
    static std::wstring  GetSetLabel (const std::wstring & name);

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
