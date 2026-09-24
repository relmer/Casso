#pragma once

#include "Debugger/Source/SourceService.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Widgets/DxuiActionBanner.h"
#include "Widgets/DxuiTextView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane
//
//  One source document (FR-054 to FR-059, FR-113): the file the window gives
//  it, the PC's line marked when the PC is in that file, breakpoints marked on
//  their lines, and a banner for what this document needs said.
//
//  THE PC IS AT THE OUTERMOST LINE. Inside a macro expansion that is the
//  invocation, and the invocation's document says the machine is inside a
//  macro and offers the body line. Showing the body is the user's choice,
//  kept by the window for every document, and lasts until the PC leaves the
//  expansion; the body's document then marks the body line.
//
//  A file is found once, through the host, when the document is given it. A
//  dropped file replaces the text until the document is given another file.
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

    //  Whether a debug file is loaded and the document has a file, which is
    //  when it is shown.
    bool  IsActive      () const { return m_state.has_value() && m_docFileId >= 0; }
    bool  HasBanner     () const { return !m_banner->GetText().empty(); }

    void  Configure     (HWND hwnd);
    void  Apply         (const DebuggerViewSnapshot & snapshot);

    //  The file this document shows, or -1 for none; a new file is found
    //  through the host the next time the document is applied.
    void  SetFile       (int fileId);
    int   GetFile       () const { return m_docFileId; }

    //  Whether the PC's place is the macro body rather than the invocation.
    void  SetShowBody   (bool showBody) { m_showBody = showBody; }

    //  The banner's Show body button asks the window, which keeps the choice
    //  for every document.
    void  SetOnToggleBody (std::function<void ()> fn) { m_onToggleBody = std::move (fn); }

    //  The 1-based line at the top of the view, and a line to put there, for
    //  a document saved and reopened.
    int   GetTopSourceLine () const;
    void  SetTopSourceLine (int line);

    //  A code row was selected: scroll to its line when it is in this file.
    void  ShowLine      (int fileId, int line);
    void  FollowMarkedLine ();

    //  A click on a line moves the code pane to the line's address; a double
    //  click toggles a breakpoint on it.
    void  OnClick       (POINT atDip);
    void  OnDoubleClick (POINT atDip);

    //  A file the user dropped, as the host matched it.
    void  ShowDropped   (const SourceLookup & lookup, int recordIndex);

    //  Between the invocation and the body line, inside a macro: the window's
    //  to switch, through SetOnToggleBody.
    void  ToggleBody    ();

    //  The file and line the PC is at, the body's when it is shown.
    int   GetShownFileId () const;
    int   GetShownLine   () const;

    //  The pieces, apart from any view.
    static std::vector<std::wstring>       SplitLines (const std::string & text);
    static std::vector<DxuiTextView::Row>  BuildRows  (const std::vector<std::wstring> & lines, int markedLine,
                                                       const std::set<int> & breakpointLines);

    //  The command a double click on a line sends: clearing the breakpoint
    //  already on it, or setting one.
    static std::string  GetToggleLine (const DebuggerViewSnapshot::SourceState & state, int fileId, int line);

    static std::wstring  GetBannerText (SourceMatch match, const std::string & fileName, bool hasText,
                                        int depth, bool showingBody, const std::string & bodyName, int bodyLine);

    //  Whether a macro's body can be offered: not when it is in the file that
    //  could not be found.
    static bool          CanShowBody   (bool hasText, int depth, int bodyFileId, int fileId);

private:
    static constexpr int  kTabWidth = 8;

    void  LoadFile   (int fileId);
    void  Rebuild    ();
    void  ScrollTo   (int line);
    std::optional<int>  GetLineAt (POINT atDip) const;
    std::string  GetFileName (int fileId) const;

    DxuiTextView                                     * m_view    = nullptr;
    DxuiActionBanner                                 * m_banner  = nullptr;
    FindFn                                             m_find;
    RunFn                                              m_run;
    GoToFn                                             m_goTo;

    std::function<void ()>                            m_onToggleBody;

    std::optional<DebuggerViewSnapshot::SourceState>  m_state;
    std::wstring                                      m_loadedFor;
    int                                               m_docFileId       = -1;
    int                                               m_pendingTopLine  = 0;
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
