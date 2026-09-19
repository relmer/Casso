#pragma once

#include "Debugger/Source/SourceService.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Widgets/DxuiActionBanner.h"
#include "Widgets/DxuiTextView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane
//
//  The source file and line for the PC (FR-054 to FR-059): the line marked,
//  breakpoints marked on their lines, a banner for what the pane needs said.
//
//  IT SHOWS THE OUTERMOST LINE. Inside a macro expansion that is the
//  invocation, and the banner says the machine is inside a macro and offers
//  the body line. Showing the body is the user's choice and lasts until the
//  PC leaves the expansion.
//
//  A file is found once, through the host, when the pane first needs it; the
//  text is kept until another file is needed. A dropped file replaces it.
//
//  Rows are rebuilt only when the file, the marked line or the breakpoints
//  change, so the user's scroll and selection survive the snapshots between.
//
//  The window owns the view and the banner as child controls; the pane holds
//  pointers to them.
//
////////////////////////////////////////////////////////////////////////////////

class SourcePane
{
public:
    using FindFn = std::function<SourceLookup (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                               const std::string & programKey)>;
    using RunFn  = std::function<void (const std::string & line)>;
    using GoToFn = std::function<void (Word address)>;

    SourcePane (DxuiTextView * view, DxuiActionBanner * banner, FindFn find, RunFn run, GoToFn goTo);

    SourcePane (const SourcePane &)             = delete;
    SourcePane & operator= (const SourcePane &) = delete;

    DxuiTextView      * GetView   () const { return m_view; }
    DxuiActionBanner  * GetBanner () const { return m_banner; }

    //  Whether a debug file is loaded, which is when the pane is shown.
    bool  IsActive      () const { return m_state.has_value(); }
    bool  HasBanner     () const { return !m_banner->GetText().empty(); }

    void  Configure     (HWND hwnd);
    void  Apply         (const DebuggerViewSnapshot & snapshot);

    //  A code row was selected: scroll to its line when it is in this file.
    void  ShowLine      (int fileId, int line);
    void  FollowMarkedLine ();

    //  A click on a line moves the code pane to the line's address; a double
    //  click toggles a breakpoint on it.
    void  OnClick       (POINT atDip);
    void  OnDoubleClick (POINT atDip);

    //  A file the user dropped, as the host matched it.
    void  ShowDropped   (const SourceLookup & lookup, int recordIndex);

    //  Between the invocation and the body line, inside a macro.
    void  ToggleBody    ();

    //  The pieces, apart from any view.
    static std::vector<std::wstring>       SplitLines (const std::string & text);
    static std::vector<DxuiTextView::Row>  BuildRows  (const std::vector<std::wstring> & lines, int markedLine,
                                                       const std::set<int> & breakpointLines);

    //  The command a double click on a line sends: clearing the breakpoint
    //  already on it, or setting one.
    static std::string  GetToggleLine (const DebuggerViewSnapshot::SourceState & state, int fileId, int line);

    static std::wstring  GetBannerText (SourceMatch match, const std::string & fileName, bool hasText,
                                        int depth, bool showingBody, const std::string & bodyName, int bodyLine);

private:
    static constexpr int  kTabWidth = 8;

    void  LoadFile   (int fileId);
    void  Rebuild    ();
    void  ScrollTo   (int line);
    int   GetShownFileId () const;
    int   GetShownLine   () const;
    std::optional<int>  GetLineAt (POINT atDip) const;
    std::string  GetFileName (int fileId) const;

    DxuiTextView                                     * m_view    = nullptr;
    DxuiActionBanner                                 * m_banner  = nullptr;
    FindFn                                             m_find;
    RunFn                                              m_run;
    GoToFn                                             m_goTo;

    std::optional<DebuggerViewSnapshot::SourceState>  m_state;
    std::wstring                                      m_loadedFor;
    int                                               m_fileId          = -1;
    SourceMatch                                       m_match           = SourceMatch::NotFound;
    bool                                              m_isDropped       = false;
    int                                               m_droppedAt       = -1;
    std::vector<std::wstring>                         m_lines;
    bool                                              m_showBody        = false;
    int                                               m_rowsLine        = -1;
    bool                                              m_followPending   = false;
    std::set<int>                                     m_rowsBreakpoints;
    int                                               m_rowsFileId      = -2;
};
