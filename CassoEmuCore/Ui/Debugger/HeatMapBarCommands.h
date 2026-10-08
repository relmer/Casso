#pragma once

#include "Pch.h"

#include "Debugger/HeatMapOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands
//
//  What the heat map pane's bar shows: one drop-down for how the map counts,
//  fading for each of the fade times or cumulative, with Reset counts beside
//  it while cumulative; whether writes that change nothing are left out, and
//  the set whose reads before written are, as a drop-down. The map's row of
//  views holds a strip of its own: which accesses it shows, as a drop-down,
//  and Blend; then on the right the set of ranges shown and the bank shown,
//  as drop-downs, the set's ending with Edit ranges. The zoom is a widget in the map's own corner. Each strip is a
//  DxuiToolbar, so what does not fit goes into its "..." menu as on every
//  other strip.
//
//  The window owns the behavior: it hands over one dispatch, one enabled
//  test, one checked test and one label, each taking an id.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapBarCommands
{
public:
    //  How the map counts, and its fade time while fading.
    static constexpr int  kMode        = 3;

    //  Which accesses the map shows: all, code, data or changed values.
    static constexpr int  kView        = 10;
    static constexpr int  kResetCounts = 4;

    //  Apart from the others' numbers, so an entry added beside them keeps
    //  its own: which set of ranges the map shows, and which bank it shows.
    static constexpr int  kRangeSet    = 20;
    static constexpr int  kBank        = 30;

    //  Whether an address touched more than one way mixes their colors.
    static constexpr int  kBlend       = 40;

    //  Whether writes that stored the value already there are left out,
    //  and the set of ranges whose reads before written are.
    static constexpr int  kIgnoreSame  = 50;
    static constexpr int  kIgnoreSet   = 51;

    struct Handlers
    {
        std::function<void (int id)>          dispatch;
        std::function<bool (int id)>          isEnabled;
        std::function<bool (int id)>          isChecked;
        std::function<std::wstring (int id)>  getLabel;
    };

    explicit HeatMapBarCommands (Handlers handlers);

    //  The bar's entries, Reset counts among them only while cumulative,
    //  and the view row's.
    std::vector<DxuiToolbar::Entry>  BuildEntries        (bool isCumulative) const;
    std::vector<DxuiToolbar::Entry>  BuildViewRowEntries () const;
    std::shared_ptr<DxuiCommand>     Find         (int id) const;

    //  A fade time as the drop-down lists it, "10 s", and as the entry shows
    //  the one in force while fading, "Fade (10 s)".
    static std::wstring  GetFadeChoiceLabel (int seconds);
    static std::wstring  GetFadeLabel       (int seconds);

    //  The bank in force as the entry shows it, "Main RAM": the name alone,
    //  as the bar has little room.
    static std::wstring  GetBankEntryLabel  (HeatMapOptions::Bank bank);

    //  The set left out as the entry shows it, "Leave out: Boot probes", or
    //  "Leave out: None".
    static std::wstring  GetIgnoreSetLabel  (const std::wstring & set);

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
        bool                 trailing   = false;
        bool                 inViewRow  = false;
    };

    static const std::vector<Row> &  GetRows ();

    std::vector<DxuiToolbar::Entry>  BuildEntriesOf (bool viewRow, bool isCumulative) const;

    Handlers                                   m_handlers;
    std::vector<std::shared_ptr<DxuiCommand>>  m_commands;
};
