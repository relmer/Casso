#pragma once

#include "Debugger/Source/SourceService.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/SourceSyntax.h"
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
    using RunFn  = std::function<void (const DebuggerActionBuilder & build)>;
    using GoToFn = std::function<void (Word address)>;

    //  How the PC's line and breakpoints are drawn: the disassembly pane's
    //  own marker color, row fill and breakpoint icons, so the two panes
    //  mark a line alike. No icons draws a bullet in the marker column; no
    //  syntax colors leaves the text in the view's own color.
    struct Style
    {
        uint32_t                               pcMarkerArgb = 0;
        uint32_t                               pcRowArgb    = 0;
        std::shared_ptr<const DxuiIconImage>   enabledIcon;
        std::shared_ptr<const DxuiIconImage>   disabledIcon;
        SourceSyntax::Colors                   syntax;
        uint32_t                               bytesArgb    = 0;

        bool operator== (const Style & other) const = default;
    };

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

    //  Rows are rebuilt with the next Apply when the style changes.
    void  SetStyle      (const Style & style);
    void  Apply         (const DebuggerViewSnapshot & snapshot);

    //  The file this document shows, or -1 for none; a new file is found
    //  through the host the next time the document is applied.
    void  SetFile       (int fileId);
    int   GetFile       () const { return m_docFileId; }

    //  Whether the PC's place is the macro body rather than the invocation.
    void  SetShowBody   (bool showBody) { m_macroLevel = showBody ? -1 : 0; }

    //  Which of the lines at PC is its place: 0 the invocation outside every
    //  macro, each level one macro further in, -1 the body line itself.
    void  SetMacroLevel (int level) { m_macroLevel = level; }

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

    //  The PC's branch, jump or call arrow, as the disassembly pane draws it,
    //  from the marked line to the line its target starts, in pixels; false
    //  when the PC is not on a marked line here or holds no branch.
    bool  GetBranchArrow (BranchArrow::Input & input, Word & goesTo, bool & isTaken) const;

    //  The line the PC's branch goes to in a file, if a line there starts at
    //  it, and whether it lies below the marked line; false for no branch.
    static bool  GetArrowTarget (const DebuggerViewSnapshot::SourceState & state, int fileId, int markedLine,
                                 std::optional<int> & outTargetLine, bool & outIsBelow);

    //  The pieces, apart from any view.
    static std::vector<std::wstring>       SplitLines (const std::string & text);
    static std::vector<DxuiTextView::Row>  BuildRows  (const std::vector<std::wstring> & lines, int markedLine,
                                                       const std::set<int> & breakpointLines,
                                                       const std::set<int> & disabledLines = {},
                                                       const Style         & style         = {},
                                                       const std::map<int, std::wstring> & lineBytes = {},
                                                       const std::map<int, std::pair<std::wstring, std::wstring>> & lineOperands = {});

    //  Whether a line's opcode is a 65C02 mnemonic, so that it is one instruction.
    static bool  IsInstructionLine (const std::wstring & line);

    //  The command a double click on a line sends: clearing the breakpoint
    //  already on it, or setting one.
    static std::string  GetToggleLine (const DebuggerViewSnapshot::SourceState & state, int fileId, int line);
    static std::optional<DebuggerAction>  GetToggleAction (const DebuggerViewSnapshot::SourceState & state, int fileId, int line, CommandMode mode);

    static std::wstring  GetBannerText (SourceMatch match, const std::string & fileName, bool hasText,
                                        int depth, bool showingBody, const std::string & bodyName, int bodyLine,
                                        const std::string & invokedByName = std::string(), int invokedByLine = 0);

    //  What to say when the file was found in a folder other than the one the
    //  debug file records -- a copy in another search folder -- or empty.
    static std::wstring  GetFoundElsewhereText (const std::string & fileName, const std::wstring & debugFilePath,
                                                const std::wstring & foundPath);

    //  Whether a macro's body can be offered: not when it is in the file that
    //  could not be found.
    static bool          CanShowBody   (bool hasText, int depth, int bodyFileId, int fileId);

    //  The level the banner's button goes to: from outside every macro to the
    //  body line, and from a body back out one invocation at a time.
    static int           GetNextMacroLevel (int level, int deepest);

    //  The level a request resolves to, -1 being the deepest.
    static int           GetMacroLevel (const DebuggerViewSnapshot::SourceState & state, int level);

    //  The file and line at a level, -1 being the body line.
    static std::pair<int, int>  GetPlace (const DebuggerViewSnapshot::SourceState & state, int level);

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

    std::optional<DebuggerViewSnapshot::SourceState>                   m_state;
    std::wstring                                                       m_loadedFor;
    int                                                                m_docFileId       = -1;
    int                                                                m_pendingTopLine  = 0;
    int                                                                m_fileId          = -1;
    SourceMatch                                                        m_match           = SourceMatch::NotFound;
    bool                                                               m_isDropped       = false;
    int                                                                m_droppedAt       = -1;
    std::wstring                                                       m_foundPath;
    std::vector<std::wstring>                                          m_lines;
    int                                                                m_macroLevel      = 0;
    int                                                                m_rowsLine        = -1;
    bool                                                               m_followPending   = false;
    std::set<int>                                                      m_rowsBreakpoints;
    std::set<int>                                                      m_rowsDisabled;
    std::shared_ptr<const std::map<std::pair<int, int>, std::string>>  m_rowsLineBytes;
    std::shared_ptr<const DebuggerViewSnapshot::LineOperands>          m_rowsLineOperands;
    std::set<int>                                                      m_disabledIds;
    Style                                                              m_style;
    bool                                                               m_isStyleStale    = false;
    int                                                                m_rowsFileId      = -2;
};
