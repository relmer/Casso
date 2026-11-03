#pragma once

#include "Ui/Debugger/CommandCompletion.h"
#include "Ui/Debugger/ConsoleHistory.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class DebuggerPaneFrame;
class ToolbarLabelEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput
//
//  The console's text: the machine's replies and the commands run, bounded
//  so a long session does not grow without limit. THE CONSOLE IS TEXT, NOT A
//  LIST: no columns or rows to pick, a selection that runs through the text
//  as an editor's does, and Ctrl+C to copy it.
//
////////////////////////////////////////////////////////////////////////////////

class ConsoleOutput
{
public:
    ConsoleOutput (IDebuggerPaneHost & host);

    ConsoleOutput (const ConsoleOutput &)             = delete;
    ConsoleOutput & operator= (const ConsoleOutput &) = delete;

    DxuiTextView       * GetView        () const { return m_view; }

    void                 Create         ();
    void                 Configure      ();

    //  Lines added at the end, the oldest going past the limit; the text
    //  emptied; and the text scrolled to its end. Each line is stamped with
    //  the local time it was added, or with `time`.
    void                 Append         (const std::vector<std::string> & lines);
    void                 Append         (const std::vector<std::string> & lines, const SYSTEMTIME & time);
    void                 Clear          ();
    void                 ScrollToEnd    ();

    //  Over a line's time stamp: the time stamp's rect, and the date and the
    //  time in full for the tip.
    bool                 TryGetTimeTip  (POINT point, RECT & anchor, std::wstring & text) const;

    //  The text as the project saves it, a line each, its time, a tab and
    //  its text; and the text put back from it, ahead of the lines written
    //  so far or in place of them. SaveIfChanged writes it to the project
    //  kSaveDelayMs after the last change, so a burst of output is written
    //  once.
    std::string          FormatSaved    () const;
    void                 RestoreSaved   (const std::string & text, bool keepCurrent);
    void                 SaveIfChanged  (int64_t nowMs);

    //  A time in the locale's short time format in brackets, as each line
    //  starts with; and the locale's long date and long time.
    static std::wstring  GetTimeStamp   (const SYSTEMTIME & time, const wchar_t * locale = LOCALE_NAME_USER_DEFAULT);
    static std::wstring  GetFullTime    (const SYSTEMTIME & time, const wchar_t * locale = LOCALE_NAME_USER_DEFAULT);

    //  The text laid out again once the view's width has changed, so a help
    //  listing's rows fit the width as it is now.
    void                 Reflow         ();

    //  The text takes a press or the wheel inside it, and everything while a
    //  selection drag or its scrollbar is under way; the keys go to `keys`,
    //  as they do in a terminal.
    bool                 RouteMouse     (const DxuiMouseEvent & ev, IDxuiControl * keys);

    //  Where a wrapped line continues: under the field after the line's last
    //  gap of two or more spaces within kWrapIndentLimit characters, as a
    //  help row's description and a listing's last column are; 0 for none.
    static int           GetWrapIndent  (const std::string & line);

private:
    static constexpr int      kLineLimit       = 2000;
    static constexpr float    kLineSpacing     = 1.2f;
    static constexpr size_t   kWrapIndentLimit = 48;
    static constexpr int64_t  kSaveDelayMs     = 2000;

    struct Line
    {
        std::string   text;
        SYSTEMTIME    time  = {};
        std::wstring  stamp;
    };

    //  The rows, and the line each row's time stamp is from in m_rowLines.
    std::vector<DxuiTextView::Row>  GetRows ();

    IDebuggerPaneHost  & m_host;
    DxuiTextView       * m_view      = nullptr;
    std::vector<Line>    m_lines;
    std::vector<int>     m_rowLines;   // -1 for a row with no time stamp
    int                  m_columns   = 0;   // the view's width the rows were laid out for
    int64_t              m_changedMs = 0;   // when the text last changed and was not yet saved; 0 for saved
};





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine
//
//  The console's command box: the prompt and the help command of the mode
//  in force, the lines run before (FR-128) and the completion (FR-131), the
//  gray text after the caret, and a line run on Enter.
//
////////////////////////////////////////////////////////////////////////////////

class CommandLine
{
public:
    CommandLine (IDebuggerPaneHost & host, ConsoleOutput & output);

    CommandLine (const CommandLine &)             = delete;
    CommandLine & operator= (const CommandLine &) = delete;

    DxuiTextInput      * GetBox         () const { return m_box; }
    ConsoleHistory     & GetHistory     () { return m_history; }

    void                 Create         ();
    void                 Configure      ();

    //  The prompt, the hint and the completion of the snapshot's mode, and a
    //  reply's suggestion, offered once against the box as it is.
    void                 Apply          (const DebuggerViewSnapshot & snapshot);

    //  The line run; a command missing its file name has the file picked
    //  first, and after backing out of the picker the line remains in the box,
    //  unrun.
    void                 Submit         ();

    //  The completion keys: Tab takes the gray suggestion, or completes the
    //  command word and cycles through the matches, Shift+Tab backward;
    //  Right arrow at the end of the line takes the gray earlier line; F8
    //  steps back through earlier lines starting with the typed text; F7
    //  lists the earlier lines to pick from. Tab in an empty box with nothing
    //  to offer moves the focus. True when the key was used here.
    bool                 RouteCompletionKey (const DxuiKeyEvent & ev);

    //  Up and Down walk the lines run before; true when a line was recalled.
    bool                 Recall         (bool older);

    //  The gray text after the caret: the last reply's suggestion while the
    //  box is as it was when offered, otherwise the newest earlier line that
    //  starts with what is typed.
    void                 RefreshGhost   ();

    //  The history saved with the window's state, a line each.
    void                 SaveHistory    ();

private:
    void                 ShowHistoryList ();

    IDebuggerPaneHost    & m_host;
    ConsoleOutput        & m_output;
    DxuiTextInput        * m_box                = nullptr;
    ConsoleHistory         m_history;
    CommandCompletion      m_completion;
    uint32_t               m_offeredSuggestion  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeBar
//
//  The console's toolbar, in the breakpoints pane's style: a static "Mode:"
//  label, then the mode the console reads, which the memory Address box
//  follows too, as a drop-down reading the mode in force, and Find.
//
////////////////////////////////////////////////////////////////////////////////

class ConsoleModeBar
{
public:
    using ModeChoice = std::pair<CommandMode, const wchar_t *>;

    static constexpr int  kDialectEntry = 1;

    explicit ConsoleModeBar (IDebuggerPaneHost & host);
    ~ConsoleModeBar();

    ConsoleModeBar (const ConsoleModeBar &)             = delete;
    ConsoleModeBar & operator= (const ConsoleModeBar &) = delete;

    PaneBarSlot        & GetSlot        () { return m_bar; }
    const PaneBarSlot  & GetSlot        () const { return m_bar; }
    std::wstring         GetModeText    () const { return m_dialect->label; }

    void                 Create         () { m_bar.Create(); }
    void                 Configure      ();

    //  The modes, the one in force checked. Built again before the drop-down
    //  opens and when the mode changes, since the rows have the check they
    //  were built with.
    void                 SetMenus       ();

    //  The modes in the drop-down's order, alphabetical by label; a mode's
    //  label; the command box's prompt in a mode; and a mode's help
    //  command.
    static const std::vector<ModeChoice> &  GetModeChoices ();
    static std::wstring  GetModeLabel   (const wchar_t * mode);
    static std::wstring  GetPromptText  (CommandMode mode);
    static std::wstring  GetHelpCommand (CommandMode mode);

private:
    IDebuggerPaneHost                     & m_host;
    PaneBarSlot                             m_bar;
    std::unique_ptr<ToolbarLabelEntry>      m_label;
    std::shared_ptr<DxuiCommand>            m_dialect;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane
//
//  The console: its mode bar over its text over its command line.
//
////////////////////////////////////////////////////////////////////////////////

class ConsolePane : public IDebuggerPane
{
public:
    explicit ConsolePane (IDebuggerPaneHost & host);
    ~ConsolePane() override;

    ConsoleOutput                  & GetOutput      () { return m_output; }
    CommandLine                    & GetCommandLine () { return m_commandLine; }
    ConsoleModeBar                 & GetModeBar     () { return m_modeBar; }
    DebuggerPaneFrame              * GetFrame       () const { return m_frame.get(); }

    std::wstring                     GetId          () const override;
    std::wstring                     GetTitle       () const override { return L"Console"; }
    void                             CreateControls () override;
    void                             Configure      () override;
    void                             AddToDock      (DxuiDockSite & dock) override;
    IDxuiControl                   * GetContent     () const override;
    std::vector<IDxuiControl *>      GetControls    () const override;
    IDxuiControl                   * GetMainControl () const override { return m_commandLine.GetBox(); }
    bool                             IsCommandInput () const override { return true; }
    DxuiTextView                   * GetFindView    () const override { return m_output.GetView(); }
    DebuggerPaneFrame              * GetFindFrame   () const override { return m_frame.get(); }
    std::vector<DxuiToolbar *>       GetBars        () const override { return { m_modeBar.GetSlot().GetBar() }; }
    void                             Apply          (const DebuggerViewSnapshot & snapshot) override;
    void                             SetTextZoom    (float zoom) override;
    bool                             Tick           (int64_t nowMs) override;
    bool                             RouteEditKey   (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    bool                             SubmitTextBox  (DxuiTextInput * box) override;
    void                             PlaceBar       () override { m_modeBar.GetSlot().Place(); }
    bool                             RouteBarMouse  (const DxuiMouseEvent & ev) override { return m_modeBar.GetSlot().RouteMouse (ev); }
    const wchar_t                  * GetBarTip      (POINT clientPx, RECT & anchor) const override { return m_modeBar.GetSlot().GetTip (clientPx, anchor); }
    bool                             TryGetTip      (POINT clientPx, PaneTip & tip) override;

private:
    ConsoleOutput                         m_output;
    CommandLine                           m_commandLine;
    ConsoleModeBar                        m_modeBar;
    std::unique_ptr<DebuggerPaneFrame>    m_frame;
};
