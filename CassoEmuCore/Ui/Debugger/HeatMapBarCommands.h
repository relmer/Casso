#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands
//
//  What the heat map pane's bar shows: Fading and Cumulative, the fade time
//  as a drop-down and Reset counts, then Zoom in, Zoom out and Reset zoom at
//  the far end. The bar is a DxuiToolbar, so what does not fit goes into its
//  "..." menu as on every other strip.
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
