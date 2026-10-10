#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerSettings
//
//  What the debugger keeps between sessions: reverse execution's history,
//  and the debugger window's scheme, theme, arrangement and views. The
//  emulator's preferences hold one and load and save it with the rest, under
//  the same keys as before; the debugger reads and changes it through a
//  reference and asks its host to save.
//
////////////////////////////////////////////////////////////////////////////////

struct DebuggerSettings
{
    // REVERSE EXECUTION'S HISTORY, read when the emulator starts. Recording
    // is on unless turned off here; the budget is the memory the keyframes
    // may hold, and the interval is how many video frames apart they are
    // taken. A step back replays at most one interval.
    static constexpr int  kMinReverseBudgetMb       = 4;
    static constexpr int  kMaxReverseBudgetMb       = 4096;
    static constexpr int  kMaxReverseIntervalFrames = 600;

    bool         reverseRecording         = true;
    int          reverseBudgetMb          = 64;
    int          reverseIntervalFrames    = 10;

    // The debugger window's keyboard scheme, by name: "VisualStudio",
    // "AppleWin" or "GSSquared". Stored as the name so a reordering of the
    // schemes never repoints a saved choice; the window reads an unknown name
    // as the default.
    std::string  keyScheme                = "VisualStudio";

    // The debugger window's own theme, by name, from DebuggerThemes. Empty
    // follows the emulator's theme; the window reads an unknown name the same.
    std::string  theme;

    // The debugger window's pane arrangement, in DxuiPaneLayout's text form,
    // which carries its own version. Empty until the user moves a pane; text
    // the window cannot read gives the default arrangement.
    std::string  layout;

    // The debugger's fixed panes the user closed, as pane ids separated by
    // spaces, so they stay closed in the next session.
    std::string  closedPanes;

    // Where the debugger's command bar is docked: its edge and its place
    // along it, as "left 120".
    std::string  commandBarDock;
    // Where the debugger's history timeline is docked, in the same form.
    std::string  timelineDock;
    // The debugger pane that had the keys when its window closed, by its
    // layout id; empty gives the console.
    std::string  focusedPane;
    // The debugger's disassembly viewing options turned on, as keys separated
    // by spaces; empty gives the defaults.
    std::string  disassemblyOptions;
    // The debugger's heat map options, in HeatMapOptions' text; empty gives
    // the defaults, and marks a layout saved before the heat map opened by
    // default.
    std::string  heatMapOptions;

    // The heat map's sets of ranges and the set it shows, in
    // HeatMapRangeSets' text; empty until the ranges pane first opens.
    std::string  heatMapRanges;

    // Which of the debugger's optional views were open, so a restart brings
    // them back where they were: disassembly views 2 to 4 and the one
    // following the PC, memory windows 2 to 4, and device panels. The layout
    // above says where each view sits; this says which exist.
    std::string  openViews;

    // The debugger panes' text size, as a whole percentage, within the
    // smallest and largest sizes the debugger's zoom offers.
    static constexpr int  kMinTextZoomPercent = 50;
    static constexpr int  kMaxTextZoomPercent = 300;

    int          textZoomPercent          = 100;

    // Folders where the debugger found source files, most-recent-first: for
    // every program, and for each program by its debug file's SHA-1. A
    // program's own list is searched before the global one.
    std::vector<std::string>                          sourceFolders;
    std::map<std::string, std::vector<std::string>>   programSourceFolders;
};
