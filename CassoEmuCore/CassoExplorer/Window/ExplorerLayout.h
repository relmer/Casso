#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerCommands.h"

class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout
//
//  The window's widgets and their arrangement: each widget set up and
//  wired to its handlers, the panes and splitters laid out, the command
//  bar refilled, the status bar's fields, and the theme chosen and applied.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerLayout
{
public:
    explicit ExplorerLayout (CassoExplorerWindow & window);

    //  Explorer's tabs are 33 dip tall. Its strip is 9 dip taller because it is
    //  also the window's caption; this window has a caption bar of its own, so
    //  the strip keeps only a small gap above the tabs.
    static constexpr int       kTabHeightDip       = 37;

    //  Explorer's two strips, measured at 100, 125, 150 and 200%: the command
    //  bar is 47 dip rounded down, its line included; the address bar's strip
    //  is 48 dip rounded down with its line added. The address box is 32 dip
    //  tall, and the command bar starts 4 dip in.
    static constexpr float  kCommandBarDip        = 47.0f;
    static constexpr float  kNavStripFillDip      = 48.0f;
    static constexpr UINT   kListRowHalfDip       = 14;
    static constexpr int    kMinTreeWidthDip      = 140;
    static constexpr int    kMinListWidthDip      = 220;
    static constexpr int    kMinPreviewWidthDip   = 280;   // wide enough for the hex view's toolbar

    //  The widest a pane's message runs before it wraps.
    static constexpr int     kMessageWidthDip = 400;


    //  The strip a pointer event goes to: the command bar over its band, the
    //  navigation toolbar anywhere else.
    //  The panes' widths in a body this wide, from the widths the user left
    //  them at. The list gives up width first, down to its minimum; then the
    //  preview, down to its own; then the tree. Past all three minimums the
    //  three shrink together. A preview width of zero is a hidden preview.
    struct PaneWidths
    {
        int  tree    = 0;
        int  list    = 0;
        int  preview = 0;
    };

    static PaneWidths  FitPanes (int bodyDip, int treeDip, int previewDip);

    //  File Explorer's list row, measured at nine scales from 100% to 350%:
    //  twice 14 dip rounded up, and a pixel more at any scale that is not a
    //  whole multiple -- 28, 37, 43, 51, 56, 65, 71, 84 and 99 pixels.
    static int  GetListRowHeightPx (UINT dpi);

    void  ConfigureWidgets();
    void  ApplyTheme();
    void  AdoptSystemColors();
    void  PreviewTheme (int index);
    void  SelectTheme  (const char * name);

    static bool  IsCassoThemeName (const std::string & name);
    void  RecomputeLayout();
    void  UpdateCommandBar();

    //  The part of a pane a message wraps within.
    RECT  GetMessageRect (const RECT & pane) const;

private:
    friend class CassoExplorerWindow;
    friend class ExplorerDispatch;
    friend class ExplorerInput;
    friend class ExplorerLinks;
    friend class ExplorerTips;

    CassoExplorerWindow  & m_window;

    DxuiHitTester                             m_dropHits;

    //  The location's buttons on the command bar, if any.
    CassoExplorerCommands::CommandBarExtra  m_commandBarExtra = CassoExplorerCommands::CommandBarExtra::None;
};
