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
    // taken. A step back replays at most one interval. The budget's upper
    // limit is the Options sheet's, from the memory available; a value above
    // kMaxReverseBudgetMb is invalid on any machine.
    static constexpr int  kMinReverseBudgetMb       = 1;
    static constexpr int  kMaxReverseBudgetMb       = 1024 * 1024;
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

    // The Disk II event log's and the input log's filters and columns, and
    // whether each pane was open, in the panes' options text; empty is the
    // defaults, closed.
    std::string  diskLogOptions;
    std::string  inputLogOptions;

    // Whether the console lists each action taken in the window as the
    // command that does the same, with its reply. Off, the console has only a
    // failed action's error and the result of an action that produces one.
    bool         echoActions              = false;

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

    // The debugger window's state with no setting of its own, by name: the
    // console history, the find history and the column widths, each in the
    // window's text for it. A debug project saves these with the rest.
    std::map<std::string, std::string>                windowState;
    std::map<std::string, std::vector<std::string>>   programSourceFolders;
};
