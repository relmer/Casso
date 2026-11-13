#pragma once

#include "Debugger/Source/SourceService.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/Debugger/DebuggerActions.h"





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

    //  How the PC's line and breakpoints are drawn: the disassembly pane's
    //  own marker color, row fill, breakpoint icons and PC arrow, so the two
    //  panes mark a line alike, in the view's glyph margin, with the gray
    //  breakpoint on a line with code under the pointer there. No breakpoint
    //  icons draws a bullet in the marker column, and no PC arrow a triangle
    //  there; no syntax colors leaves the text in the view's own color.
    struct Style
    {
        uint32_t                               pcMarkerArgb = 0;
        uint32_t                               pcRowArgb    = 0;
        std::shared_ptr<const DxuiIconImage>   enabledIcon;
        std::shared_ptr<const DxuiIconImage>   disabledIcon;
        SourceSyntax::Colors                   syntax;
        uint32_t                               resultArgb   = 0;
        uint32_t                               operandArgb  = 0;
        std::shared_ptr<const DxuiIconImage>   pcIcon;
        std::shared_ptr<const DxuiIconImage>   hoverIcon;

        bool operator== (const Style & other) const = default;
    };

    //  A line's breakpoint: its id and whether it is enabled.
    struct LineBreakpoint
    {
        int   id        = 0;
        bool  isEnabled = true;
    };

    //  What an operand's result is shown after, here and in the disassembly.
    static constexpr const wchar_t * kpszResultPrefix = L"Result: ";

    //  A row's cells: the marker, the line number, the source text, and the
    //  annotation. The text's column is no wider than kAnnotationColumn, so
    //  the annotations start near the code, and only a longer line pushes
    //  that line's annotation along. An annotation is cut off at the view's
    //  right edge, with its tip over it, rather than scrolled to.
    static constexpr int    kTextCell         = 2;
    static constexpr int    kAnnotationCell   = 3;
    static constexpr int    kAnnotationColumn = 40;

    //  The line numbers are followed by room for the branch arrow, which
    //  stops short of the text.
    static constexpr int    kNumberCell       = 1;
    static constexpr int    kArrowGapCells    = 5;
    static constexpr float  kArrowMarginDip   = 6.0f;

    //  The symbol a source line has at a caret position, by its name, as
    //  "count_text = $60F2", and after a < or > before it, the byte that
    //  operator takes. False off a word, in a comment, or for a word that
    //  isn't a symbol. The symbol found is in `found`, when one is passed.
    static bool  TryGetSymbolText (const std::wstring & line, int ch, const std::vector<SymbolInfo> & symbols, std::wstring & text,
                                   const SymbolInfo ** found = nullptr);

    //  The lines of a symbol's tip after its address: the bytes there, as
    //  "$60C1: 05", for a symbol that is not a constant, then its description
    //  and the comment on its line in the source.
    static void  AppendSymbolDetails (std::wstring & text, const std::string & name, Word address, bool isConstant,
                                      const DebuggerViewSnapshot & snapshot, const std::string & comment);
    //  The symbol whose name is the word at a character of a line, before a
    //  comment, and where the word starts; null for none.
    static const SymbolInfo *  FindSymbolAt (const std::wstring & line, int ch, const std::vector<SymbolInfo> & symbols, int & start);

    //  The word at a character of a line, before a comment, as FindSymbolAt
    //  takes it; empty for none.
    static std::wstring  GetWordAt (const std::wstring & line, int ch);

    //  Whether a line defines a macro of a name: "name macro" as AS65 and
    //  Merlin's MAC write it, or ".macro name" as ca65 writes it.
    static bool  IsMacroDefinition (const std::wstring & line, const std::wstring & name);

    //  Where a line's code starts: past its label and the spaces after it, or
    //  at its start for a line with no code past a label.
    static int   FindCodeStart (const std::wstring & line);

    //  The first symbol the operand of the instruction starting at `codeAt`
    //  is written with, before a comment, as "count_text = $60F2", and
    //  where its word starts. False, and -1, for an operand with none.
    //  Whether the word at a character is that symbol.
    static bool  TryGetOperandSymbolText (const std::wstring & line, int codeAt, const std::vector<SymbolInfo> & symbols, std::wstring & text);
    static int   FindOperandSymbol       (const std::wstring & line, int codeAt, const std::vector<SymbolInfo> & symbols);
    static bool  IsOperandSymbolAt       (const std::wstring & line, int codeAt, int ch, const std::vector<SymbolInfo> & symbols);

    SourcePane (DxuiTextView * view, DxuiInfoBanner * banner, FindFn find, RunFn run);

    SourcePane (const SourcePane &)             = delete;
    SourcePane & operator= (const SourcePane &) = delete;

    DxuiTextView    * GetView   () const { return m_view; }
    DxuiInfoBanner  * GetBanner () const { return m_banner; }

    //  Whether a debug file is loaded and the document has a file, which is
    //  when it is shown.
    bool  IsActive      () const { return (m_state.has_value() || m_isLoose) && m_docFileId >= 0; }
    bool  HasBanner     () const { return !m_banner->GetText().empty(); }

    //  The path of the document's file, empty while none was found.
    const std::wstring &  GetFoundPath () const { return m_foundPath; }

    void  Configure     (HWND hwnd);

    //  Rows are rebuilt with the next Apply when the style changes.
    void  SetStyle      (const Style & style);

    //  The heat map's colors while the heat option is on, none while it is
    //  off: each line has the heat strip of the first range of bytes the
    //  line produced in the gutter, after the glyph margin.
    void  SetHeat       (const std::optional<HeatMapView::Palette> & palette);

    //  The outline drawn in the glyph margin of a line a bookmark is on,
    //  around the line's breakpoint when it has one.
    void  SetBookmarkMark (std::shared_ptr<const DxuiIconImage> mark) { m_bookmarkMark = std::move (mark); }

    //  The heat strips of the rows, one a line from line 1, as the gutter
    //  band's fills.
    static void  AddHeatStrips (std::vector<DxuiTextView::Row> & rows, int fileId,
                                const std::map<std::pair<int, int>, std::pair<Word, Word>> & lineRanges,
                                const DebuggerViewSnapshot::HeatColumnLevels & levels, const HeatMapView::Palette & palette,
                                const std::set<std::pair<int, int>> * unmatched = nullptr);

    //  Whose grammar colors the text: Any goes by what the file shows. Rows
    //  are rebuilt with the next Apply when it changes.

    void                     SetAssembler       (SourceSyntax::Assembler assembler);
    SourceSyntax::Assembler  GetAssemblerChoice () const { return m_assemblerChoice; }
    SourceSyntax::Assembler  GetAssembler       () const;
    void                     Apply              (const DebuggerViewSnapshot & snapshot);

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

    //  Called by the banner's Browse... for a file not found.
    void  SetOnBrowse     (std::function<void ()> fn) { m_onBrowse = std::move (fn); }

    //  The 1-based line at the top of the view, and a line to put there, for
    //  a document saved and reopened.
    int   GetTopSourceLine () const;
    void  SetTopSourceLine (int line);

    //  A code row was selected: its line is scrolled to and selected when it
    //  is in this file.
    void  ShowLine         (int fileId, int line);
    void  FollowMarkedLine ();

    //  Scrolls to the line the PC's branch goes to, and returns false when
    //  that address is on no line of this file. ShowMarkedLine scrolls back
    //  to the PC's line.
    bool  ShowArrowTarget ();
    void  ShowMarkedLine  ();

    //  The address of the line under a point, for Go to disassembly; a
    //  double click toggles a breakpoint on the line.
    std::optional<Word>  GetAddressAt    (POINT atDip) const;

    //  The address of the line the keyboard is on, when it has code.
    std::optional<Word>  GetFocusAddress () const;
    void                 OnDoubleClick   (POINT atDip);

    //  The glyph margin, as the disassembly's gutter: the pointer over it
    //  puts the gray breakpoint on a line with code, true when that line
    //  changed; and a press on it sets or clears the line's breakpoint,
    //  true when the press was on the margin.
    bool  SetGutterHover (POINT atDip);
    bool  ClickGutter    (POINT atDip);

    //  The line under a point when it has code, and its breakpoint, if it has
    //  one; false off every line with code. The breakpoint is set, switched
    //  on or off, or cleared by the actions that follow.
    bool                           TryGetLineBreakpoint   (POINT atDip, int & line, std::optional<LineBreakpoint> & breakpoint) const;
    std::optional<DebuggerAction>  GetSetBreakpointAction (int line, CommandMode mode) const;

    //  The tip of the heat strip under a point, and where to put it: by the
    //  start of the strip on its row. False off a strip, or while heat is off.
    bool  TryGetHeatTip (POINT atDip, HeatStrip::Tip & tip, RECT & anchor) const;

    //  A file the user dropped, as the host matched it.
    void  ShowDropped   (const SourceLookup & lookup, int recordIndex);

    //  A file with no debug file record, shown as text with no line mapping,
    //  with or without a debug file loaded. A source says it has no symbols
    //  and offers Load symbols, which goes to the window.
    void  ShowLoose        (const std::wstring & path, const std::string & text, bool isSource);
    bool  IsLoose          () const { return m_isLoose; }
    void  SetOnLoadSymbols (std::function<void ()> fn) { m_onLoadSymbols = std::move (fn); }

    //  What a source with no debug file record says, and its button.
    static constexpr const wchar_t * kpszNoSymbolsText  = L"No symbols are loaded for this file.";
    static constexpr const wchar_t * kpszLoadSymbols    = L"Load symbols";

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

    //  Whether a point is on a line, in the lanes left of the source text
    //  where the branch arrow runs.
    bool  IsOverArrowLanes (POINT atDip) const;

    //  Over a word of the source text, the symbol it is, and whether it is
    //  the symbol its line's operand is written with.
    bool                TryGetSymbolTip   (POINT atDip, const std::vector<SymbolInfo> & symbols, std::wstring & text, RECT & anchor,
                                           const SymbolInfo ** found = nullptr) const;
    const SymbolInfo  * FindSymbolAt      (POINT atDip, const std::vector<SymbolInfo> & symbols) const;

    //  The word of the source text under a point; and the number of the line
    //  that defines a macro of that name in the pane's file, none if no line
    //  does.
    std::wstring        GetWordAt         (POINT atDip) const;
    std::optional<int>  FindMacroLine     (const std::wstring & name) const;
    bool                IsOperandSymbolAt (POINT atDip, const std::vector<SymbolInfo> & symbols) const;

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
                                                       const std::map<int, std::pair<std::wstring, std::wstring>> & lineOperands = {},
                                                       SourceSyntax::Assembler assembler = SourceSyntax::Assembler::Any,
                                                       SourceSyntax::Listing   listing   = SourceSyntax::Listing::None,
                                                       bool                    isC       = false,
                                                       int                     hoverLine = -1);

    //  The source line each row shows.
    static std::vector<int>  GetRowLines (const std::vector<DxuiTextView::Row> & rows);

    //  Whether a line's opcode is a 65C02 mnemonic, so that it is one instruction.
    static bool  IsInstructionLine (const std::wstring & line, SourceSyntax::Assembler assembler = SourceSyntax::Assembler::Any);

    //  Whether a line is a label alone, with at most a comment after it.
    static bool  IsLabelLine       (const std::wstring & line, SourceSyntax::Assembler assembler = SourceSyntax::Assembler::Any);

    //  The command a double click on a line sends: clearing the breakpoint
    //  already on it, or setting one.
    static std::string                    GetToggleLine   (const DebuggerViewSnapshot::SourceState & state, int fileId, int line);
    static std::optional<DebuggerAction>  GetToggleAction (const DebuggerViewSnapshot::SourceState & state, int fileId, int line, CommandMode mode);

    static std::wstring  GetBannerText (SourceMatch match, const std::string & fileName, bool hasText,
                                        int depth, bool showingBody, const std::string & bodyName, int bodyLine,
                                        const std::string & invokedByName = std::string(), int invokedByLine = 0);

    //  The banner's first line while the program in memory isn't the debug
    //  file's.
    static std::wstring  GetStaleText (const std::wstring & debugFilePath);

    //  What to say when the file was found in a folder other than the one the
    //  debug file has it in -- a copy in another search folder -- or empty.
    //  A copy whose hash is the debug file's is the file the program was
    //  built from, and the text has that.
    static std::wstring  GetFoundElsewhereText (const std::string & fileName, const std::wstring & debugFilePath,
                                                const std::wstring & foundPath, bool isVerified = false);

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
    static constexpr int  kTabWidth = 4;

    void                LoadFile        (int fileId);
    void                Rebuild         ();
    void                ShowLooseBanner ();
    void                ScrollTo        (int line);
    std::optional<int>  GetLineAt       (POINT atDip) const;
    int                 GetRowOfLine    (int line) const;
    bool                HasCode         (int line) const;
    std::string         GetFileName     (int fileId) const;

    DxuiTextView    * m_view   = nullptr;
    DxuiInfoBanner  * m_banner = nullptr;
    FindFn            m_find;
    RunFn             m_run;

    std::function<void ()>                            m_onToggleBody;
    std::function<void ()>                            m_onBrowse;
    bool                                              m_isBrowseShown = false;
    std::function<void ()>                            m_onLoadSymbols;

    std::optional<DebuggerViewSnapshot::SourceState>               m_state;
    std::wstring                                                   m_loadedFor;
    int                                                            m_docFileId        = -1;
    int                                                            m_pendingTopLine   = 0;
    int                                                            m_fileId           = -1;
    SourceMatch                                                    m_match            = SourceMatch::NotFound;
    bool                                                           m_isDropped        = false;
    int                                                            m_droppedAt        = -1;
    std::wstring                                                   m_foundPath;
    std::vector<std::wstring>                                      m_lines;
    int                                                            m_macroLevel       = 0;
    int                                                            m_rowsLine         = -1;
    bool                                                           m_followPending    = false;
    std::set<int>                                                  m_rowsBreakpoints;
    std::set<int>                                                  m_rowsBookmarks;
    std::set<Word>                                                 m_bookmarkAddresses;
    std::shared_ptr<const DxuiIconImage>                           m_bookmarkMark;
    std::set<int>                                                  m_rowsDisabled;
    std::shared_ptr<const DebuggerViewSnapshot::LineOperands>      m_rowsLineOperands;
    std::vector<int>                                               m_rowLines;
    std::set<int>                                                  m_disabledIds;
    Style                                                          m_style;
    std::optional<HeatMapView::Palette>                            m_heatPalette;
    std::shared_ptr<const DebuggerViewSnapshot::HeatColumnLevels>  m_heatLevels;
    std::shared_ptr<const DebuggerViewSnapshot::HeatColumnLevels>  m_rowsHeatLevels;
    std::set<std::pair<int, int>>                                  m_rowsUnmatched;   // the unmatched lines the rows were built with
    bool                                                           m_isStyleStale     = false;
    bool                                                           m_isLoose          = false;
    bool                                                           m_isLooseSource    = false;
    SourceSyntax::Assembler                                        m_assemblerChoice  = SourceSyntax::Assembler::Any;
    SourceSyntax::Assembler                                        m_detected         = SourceSyntax::Assembler::As65;
    SourceSyntax::Listing                                          m_listing          = SourceSyntax::Listing::None;
    bool                                                           m_isC              = false;
    int                                                            m_rowsFileId       = -2;
    int                                                            m_hoverLine        = -1;
    int                                                            m_rowsHover        = -1;
};
