#pragma once

#include "Pch.h"

#include "Debugger/HeatMapOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands
//
//  What the heat map pane's bar shows: Fading and Cumulative, the fade time
//  as a drop-down and Reset counts, Blend, the set of ranges shown as a
//  drop-down, Edit ranges and the bank shown as a drop-down, whether writes
//  that change nothing are left out and the set whose reads before written
//  are, as a drop-down, then Zoom in, Zoom out and Reset zoom at the far end.
//  The bar is a DxuiToolbar, so what does not fit goes into its "..." menu as
//  on every other strip.
//
//  The window owns the behavior: it hands over one dispatch, one enabled
//  test, one checked test and one label, each taking an id.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapBarCommands
{
public:
    static constexpr int  kFading      = 1;
    static constexpr int  kCumulative  = 2;
    static constexpr int  kFade        = 3;
    static constexpr int  kResetCounts = 4;
    static constexpr int  kZoomIn      = 5;
    static constexpr int  kZoomOut     = 6;
    static constexpr int  kResetZoom   = 7;

    //  Apart from the others' numbers, so an entry added beside them keeps
    //  its own: which set of ranges the map shows, the pane that edits them,
    //  and which bank it shows.
    static constexpr int  kRangeSet    = 20;
    static constexpr int  kEditRanges  = 21;
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

    std::vector<DxuiToolbar::Entry>  BuildEntries () const;
    std::shared_ptr<DxuiCommand>     Find         (int id) const;

    //  A fade time as the drop-down lists it, "10 s", and as the entry shows
    //  the one in force, "Fade: 10 s".
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
    };

    static const std::vector<Row> &  GetRows ();

    Handlers                                   m_handlers;
    std::vector<std::shared_ptr<DxuiCommand>>  m_commands;
};
