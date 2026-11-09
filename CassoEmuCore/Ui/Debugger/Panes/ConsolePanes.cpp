#include "Pch.h"

#include "Ui/Debugger/Panes/ConsolePanes.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/Source/SourcePathList.h"
#include "Ui/Debugger/ConsoleLinks.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::ConsoleOutput
//
////////////////////////////////////////////////////////////////////////////////

ConsoleOutput::ConsoleOutput (IDebuggerPaneHost & host) :
    m_host (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::Create
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::Create()
{
    m_view = m_host.GetControlParent().CreateChild<DxuiTextView>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::Configure()
{
    m_view->SetOwnerWindow   (m_host.GetOwnerHwnd());
    m_view->SetFollowEnd     (true);
    m_view->SetLineSpacing   (kLineSpacing);
    m_view->SetPaneTextInset (true);
    m_view->SetOnLink        ([this] (int row, int span) { OpenLink (row, span); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::Append
//
//  A command's gap sets it apart from the one before, so the console does
//  not open on one. A console scrolled back to read remains on the same
//  text, even as the oldest lines go; one at its end follows the output.
//  Output to a console out of sight marks its tab (FR-041).
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::Append (const std::vector<std::string> & lines)
{
    SYSTEMTIME  now = {};



    GetLocalTime (&now);
    Append (lines, now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::Append
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::Append (const std::vector<std::string> & lines, const SYSTEMTIME & time)
{
    bool          atEnd   = m_view->IsAtEnd();
    int           top     = m_view->GetTopLine();
    int           dropped = 0;
    auto          first   = lines.begin();
    std::wstring  stamp   = GetTimeStamp (time);



    while (m_lines.empty() && first != lines.end() && (first->empty() || DebuggerViewState::IsCommandGap (*first)))
    {
        ++first;
    }

    if (first == lines.end())
    {
        return;
    }

    //  No full path is written out in the console: each is its file's name,
    //  linked, with the path in its tip.
    for (auto line = first; line != lines.end(); ++line)
    {
        m_lines.push_back (Line { ConsoleLinks::LinkPaths (*line), time, stamp });
    }

    if ((int) m_lines.size() > kLineLimit)
    {
        dropped = (int) m_lines.size() - kLineLimit;
        m_lines.erase (m_lines.begin(), m_lines.begin() + dropped);
    }

    m_columns = m_view->GetVisibleColumns();
    m_view->SetRowsDroppingFront (GetRows(), dropped);
    m_changedMs = (std::max) ((int64_t) 1, (int64_t) GetTickCount64());
    m_view->SetTopLine (atEnd ? m_view->GetLineCount() : top - dropped);

    if (!m_view->IsVisible())
    {
        m_host.MarkPaneTab (DebuggerLayout::kConsole);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::Reflow
//
//  A console at its end remains at its end; otherwise the top line is the same.
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::Reflow()
{
    int   columns = m_view->GetVisibleColumns();
    bool  atEnd   = m_view->IsAtEnd();
    int   top     = m_view->GetTopLine();



    if (columns <= 0 || columns == m_columns || m_lines.empty())
    {
        return;
    }

    m_columns = columns;
    m_view->UpdateRows (GetRows());
    m_view->SetTopLine (atEnd ? m_view->GetLineCount() : top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::GetRows
//
//  Each command's output is laid out apart from the rest, so a help
//  listing's syntax column fits its rows at the view's width less the time
//  stamps' column. The time stamp is the first cell of a line's first row;
//  a help row laid out as two has an empty first cell on the second. Rows
//  are laid out from the text without its marks. A line laid out as one row
//  gets its links; a line split into several, as a help listing's row is,
//  doesn't get them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTextView::Row> ConsoleOutput::GetRows()
{
    std::vector<DxuiTextView::Row>  rows;
    size_t                          start   = 0;
    size_t                          stampAt = 0;
    std::vector<std::string>        texts;
    std::vector<std::string>        laidOut;
    std::vector<size_t>             sources;



    for (const Line & line : m_lines)
    {
        stampAt = (std::max) (stampAt, line.stamp.size() + 1);
    }

    m_rowLines.clear();
    m_rowLinks.clear();

    while (start < m_lines.size())
    {
        size_t  end = start + 1;

        while (end < m_lines.size() && !DebuggerViewState::IsCommandGap (m_lines[end].text))
        {
            end++;
        }

        texts.clear();

        for (size_t i = start; i < end; i++)
        {
            texts.push_back (ConsoleLinks::GetShownText (m_lines[i].text));
        }

        laidOut = CommandModeHelp::LayOutTabbedRows (texts, (std::max) (0, m_columns - (int) stampAt), sources);

        for (size_t r = 0; r < laidOut.size(); r++)
        {
            const std::string                & text   = laidOut[r];
            size_t                             source = start + sources[r];
            bool                               isLead = r == 0 || sources[r - 1] != sources[r];
            bool                               isOne  = text == texts[sources[r]];
            DxuiTextView::Row                  row;
            std::vector<ConsoleLinks::Link>    links;
            std::vector<std::wstring>          paths;



            if (DebuggerViewState::IsCommandGap (text))
            {
                row.cells  = { std::wstring() };
                row.height = DebuggerViewState::kCommandGapHeight;
                m_rowLines.push_back (-1);
            }
            else
            {
                row.cells      = { isLead ? m_lines[source].stamp : std::wstring(), isOne ? ConsoleLinks::Parse (m_lines[source].text, links) : TextEncoding::NarrowToWide (text) };
                row.wrapIndent = GetWrapIndent (text);
                m_rowLines.push_back (isLead ? (int) source : -1);
            }

            for (const ConsoleLinks::Link & link : links)
            {
                row.spans.push_back (DxuiTextView::Span { 1, link.start, link.length, 0, 0, true });
                paths.push_back (link.path);
            }

            rows.push_back (std::move (row));
            m_rowLinks.push_back (std::move (paths));
        }

        start = end;
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::GetWrapIndent
//
////////////////////////////////////////////////////////////////////////////////

int ConsoleOutput::GetWrapIndent (const std::string & line)
{
    size_t  limit  = std::min (line.size(), kWrapIndentLimit);
    size_t  indent = 0;
    size_t  i      = 0;



    for (i = 0; i < limit; i++)
    {
        bool  isGap = line[i] == ' ' && (i == 0 || (i + 1 < line.size() && line[i + 1] == ' ') || line[i - 1] == ' ');



        if (isGap && i + 1 < line.size() && line[i + 1] != ' ')
        {
            indent = i + 1;
        }
    }

    return (int) indent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::Clear
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::Clear()
{
    m_lines.clear();
    m_rowLines.clear();
    m_rowLinks.clear();
    m_view->SetRows ({});
    m_changedMs = (std::max) ((int64_t) 1, (int64_t) GetTickCount64());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::FormatSaved
//
//  A line's time is its local date and time, as YYYY-MM-DD HH:MM:SS.
//
////////////////////////////////////////////////////////////////////////////////

std::string ConsoleOutput::FormatSaved() const
{
    std::string  text;



    for (const Line & line : m_lines)
    {
        text += std::format ("{:04}-{:02}-{:02} {:02}:{:02}:{:02}\t{}\n", line.time.wYear, line.time.wMonth, line.time.wDay,
                             line.time.wHour, line.time.wMinute, line.time.wSecond, line.text);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::RestoreSaved
//
//  A line whose time cannot be read is passed over. With `keepCurrent`, the
//  lines written since the window opened follow the saved ones.
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::RestoreSaved (const std::string & text, bool keepCurrent)
{
    std::istringstream  in (text);
    std::string         line;
    std::vector<Line>   current = keepCurrent ? std::move (m_lines) : std::vector<Line>();



    m_lines.clear();

    while (std::getline (in, line))
    {
        SYSTEMTIME  time   = {};
        size_t      tab    = line.find ('\t');
        int         year   = 0;
        int         month  = 0;
        int         day    = 0;
        int         hour   = 0;
        int         minute = 0;
        int         second = 0;

        if (tab == std::string::npos ||
            sscanf_s (line.substr (0, tab).c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
        {
            continue;
        }

        time.wYear   = (WORD) year;
        time.wMonth  = (WORD) month;
        time.wDay    = (WORD) day;
        time.wHour   = (WORD) hour;
        time.wMinute = (WORD) minute;
        time.wSecond = (WORD) second;

        m_lines.push_back (Line { ConsoleLinks::LinkPaths (line.substr (tab + 1)), time, GetTimeStamp (time) });
    }

    m_lines.insert (m_lines.end(), std::make_move_iterator (current.begin()), std::make_move_iterator (current.end()));

    if ((int) m_lines.size() > kLineLimit)
    {
        m_lines.erase (m_lines.begin(), m_lines.end() - kLineLimit);
    }

    m_columns = m_view->GetVisibleColumns();
    m_view->SetRows (GetRows());
    m_view->SetTopLine (m_view->GetLineCount());
    m_changedMs = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::SaveIfChanged
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::SaveIfChanged (int64_t nowMs)
{
    IDebuggerWindowHost  * host = m_host.GetWindowHost();



    if (m_changedMs == 0 || nowMs - m_changedMs < kSaveDelayMs || host == nullptr)
    {
        return;
    }

    host->SetDebuggerWindowState ("consoleText", FormatSaved());
    m_changedMs = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::TryGetTimeTip
//
//  The time stamp is the first cell of a row's first line, so a point on a
//  wrapped row's later lines is not over it.
//
////////////////////////////////////////////////////////////////////////////////

bool ConsoleOutput::TryGetTimeTip (POINT point, RECT & anchor, std::wstring & text) const
{
    DxuiTextView::Position  pos    = {};
    float                   left   = 0.0f;
    float                   right  = 0.0f;
    float                   middle = 0.0f;
    float                   half   = 0.0f;
    int                     line   = -1;



    if (m_view == nullptr || !m_view->IsVisible() || !m_host.IsRoutable (m_view) || !DxuiDockSite::Contains (m_view->GetBounds(), point))
    {
        return false;
    }

    pos = m_view->HitTest (point);

    if (pos.row < 0 || pos.row >= (int) m_rowLines.size() || m_rowLines[(size_t) pos.row] < 0)
    {
        return false;
    }

    line = m_rowLines[(size_t) pos.row];

    if (!m_view->GetCellAnchorPx (pos.row, 0, left, middle) || !m_view->GetCellAnchorPx (pos.row, 1, right, middle))
    {
        return false;
    }

    half = (float) m_view->GetCellHeightPx() * 0.5f;

    if ((float) point.x < left || (float) point.x >= right || (float) point.y < middle - half || (float) point.y >= middle + half)
    {
        return false;
    }

    anchor = RECT { (LONG) left, (LONG) (middle - half), (LONG) right, (LONG) (middle + half) };
    text   = GetFullTime (m_lines[(size_t) line].time);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::TryGetLinkTip
//
////////////////////////////////////////////////////////////////////////////////

bool ConsoleOutput::TryGetLinkTip (POINT point, RECT & anchor, std::wstring & text) const
{
    int   row  = -1;
    int   span = -1;
    RECT  part = {};



    if (m_view == nullptr || !m_view->IsVisible() || !m_host.IsRoutable (m_view) || !m_view->TryGetLinkAt (point, row, span, part))
    {
        return false;
    }

    text = GetLinkPath (row, span);

    if (text.empty())
    {
        return false;
    }

    anchor = part;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::GetLinkPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ConsoleOutput::GetLinkPath (int row, int span) const
{
    if (row < 0 || row >= (int) m_rowLinks.size() || span < 0 || span >= (int) m_rowLinks[(size_t) row].size())
    {
        return std::wstring();
    }

    return m_rowLinks[(size_t) row][(size_t) span];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::OpenLink
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::OpenLink (int row, int span)
{
    IDebuggerWindowHost  * host = m_host.GetWindowHost();
    std::wstring           path = GetLinkPath (row, span);



    if (host == nullptr || path.empty())
    {
        return;
    }

    host->OpenDebuggerFile (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::GetTimeStamp
//
//  The short time format is the one the user picks in the region settings,
//  which GetTimeFormatEx with no format would not use.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ConsoleOutput::GetTimeStamp (const SYSTEMTIME & time, const wchar_t * locale)
{
    wchar_t  format[80]  = {};
    wchar_t  stamp[80]   = {};



    if (GetLocaleInfoEx (locale, LOCALE_SSHORTTIME, format, (int) std::size (format)) == 0 ||
        GetTimeFormatEx (locale, 0, &time, format, stamp, (int) std::size (stamp)) == 0)
    {
        return std::format (L"[{:02}:{:02}]", time.wHour, time.wMinute);
    }

    return std::format (L"[{}]", stamp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::GetFullTime
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ConsoleOutput::GetFullTime (const SYSTEMTIME & time, const wchar_t * locale)
{
    wchar_t  date[128]  = {};
    wchar_t  clock[80]  = {};



    if (GetDateFormatEx (locale, DATE_LONGDATE, &time, nullptr, date, (int) std::size (date), nullptr) == 0 ||
        GetTimeFormatEx (locale, 0, &time, nullptr, clock, (int) std::size (clock)) == 0)
    {
        return std::format (L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    }

    return std::format (L"{} {}", date, clock);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::ScrollToEnd
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::ScrollToEnd()
{
    m_view->SetTopLine (m_view->GetLineCount());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::RouteMouse
//
//  A press selects in the text, but typing goes to the command line, as it
//  does in a terminal. A move over the text sets the link that is
//  underlined, then goes on to the rest of the window, where the tips are.
//
////////////////////////////////////////////////////////////////////////////////

bool ConsoleOutput::RouteMouse (const DxuiMouseEvent & ev, IDxuiControl * keys)
{
    bool  isRoutable = m_view->IsVisible() && m_host.IsRoutable (m_view);



    if (ev.kind == DxuiMouseEventKind::Move && m_view->SetHoverPoint (isRoutable ? ev.positionDip : POINT { -1, -1 }))
    {
        m_host.Repaint();
    }

    if (!isRoutable)
    {
        return false;
    }

    if (m_view->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
    {
        (void) m_view->OnMouse (ev);
        return true;
    }

    if (!DxuiDockSite::Contains (m_view->GetBounds(), ev.positionDip) ||
        (ev.kind != DxuiMouseEventKind::Down && ev.kind != DxuiMouseEventKind::Wheel) ||
        (ev.kind == DxuiMouseEventKind::Down && ev.button != DxuiMouseButton::Left))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_host.FocusControl (keys);
    }

    (void) m_view->OnMouse (ev);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::CommandLine
//
////////////////////////////////////////////////////////////////////////////////

CommandLine::CommandLine (IDebuggerPaneHost & host, ConsoleOutput & output) :
    m_host   (host),
    m_output (output)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::Create
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::Create()
{
    m_box = m_host.GetControlParent().CreateChild<DxuiTextInput>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::Configure
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::Configure()
{
    m_box->SetHwnd        (m_host.GetOwnerHwnd());
    m_box->SetMaxLength   (256);
    m_box->SetPrompt      (ConsoleModeBar::GetPromptText (CommandMode::AppleWin));
    m_box->SetPlaceholder (L"Command (Enter to run, HELP for help)");
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::Apply
//
//  The console reads as a command prompt in the mode in force, with that
//  mode's own help command in its hint.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::Apply (const DebuggerViewSnapshot & snapshot)
{
    m_box->SetPrompt      (ConsoleModeBar::GetPromptText (snapshot.mode));
    m_box->SetPlaceholder (std::format (L"Command (Enter to run, {} for help)", ConsoleModeBar::GetHelpCommand (snapshot.mode)));

    m_completion.SetMode (snapshot.mode);

    if (snapshot.suggestionSerial != m_offeredSuggestion)
    {
        m_offeredSuggestion = snapshot.suggestionSerial;
        m_completion.OfferSuggestion (TextEncoding::NarrowToWide (snapshot.suggestion), m_box->GetText());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::Submit
//
//  The mode comes from the last snapshot; one built before a mode change can
//  only miss a prompt, and the handler then reports the missing name.
//  Running a command scrolls the output to its end, however far back it was
//  scrolled.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::Submit()
{
    HRESULT                       hr       = S_OK;
    IDebuggerWindowHost         * host     = m_host.GetWindowHost();
    IHostDialogs                * dialogs  = m_host.GetHostDialogs();
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    std::wstring                  typed    = m_box->GetText();
    std::string                   line     = TextEncoding::WideToNarrow (typed);
    std::optional<DebugVerb>      fileVerb;
    FileDialogSpec                spec;
    std::filesystem::path         chosen;
    bool                          picked   = false;



    BAIL_OUT_IF (host == nullptr, S_OK);

    fileVerb = DebuggerViewState::GetMissingFileVerb (line,
                                                      snapshot != nullptr ? snapshot->mode : CommandMode::AppleWin,
                                                      snapshot != nullptr && snapshot->isAssembling);

    if (fileVerb.has_value())
    {
        spec.filters = { { L"All files", L"*.*" } };

        hr = (*fileVerb == DebugVerb::ReadFile) ? dialogs->PickFileToOpen (m_host.GetOwnerHwnd(), spec, chosen, picked)
                                                : dialogs->PickFileToSave (m_host.GetOwnerHwnd(), spec, chosen, picked);
        CHR (hr);

        BAIL_OUT_IF (!picked, S_OK);

        line = DebuggerViewState::GetLineWithFileName (line, TextEncoding::WideToNarrow (chosen.wstring()));
    }

    m_history.Add (typed);
    SaveHistory();

    m_output.ScrollToEnd();

    host->RunDebuggerCommand (line);
    m_box->SetText (L"");

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::RouteCompletionKey
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLine::RouteCompletionKey (const DxuiKeyEvent & ev)
{
    std::wstring  text      = m_box->GetText();
    std::wstring  result;
    bool          atEnd     = m_box->GetSelectionStart() == text.size() && m_box->GetSelectionEnd() == text.size();
    bool          isChanged = false;



    switch (ev.vk)
    {
    case VK_TAB:
        isChanged = (!ev.shift && m_completion.TryAcceptSuggestion (text, result)) || m_completion.TryComplete (text, !ev.shift, result);

        if (!isChanged && text.empty())
        {
            return false;
        }

        break;

    case VK_RIGHT:
        if (ev.shift || !atEnd || !m_completion.TryAcceptHistory (text, m_history.GetLines(), result))
        {
            return false;
        }

        isChanged = true;
        break;

    case VK_F8:
        isChanged = !ev.shift && m_completion.TrySearchHistory (text, m_history.GetLines(), result);
        break;

    case VK_F7:
        ShowHistoryList();
        return true;

    default:
        return false;
    }

    if (isChanged)
    {
        m_box->SetText (result);
        m_box->SetSelection (result.size(), result.size());
    }

    RefreshGhost();
    m_host.Repaint();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::Recall
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLine::Recall (bool older)
{
    std::optional<std::wstring>  recalled = older ? m_history.GetOlder (m_box->GetText()) : m_history.GetNewer (m_box->GetText());



    if (!recalled.has_value())
    {
        return false;
    }

    m_box->SetText (*recalled);
    m_box->SetSelection (recalled->size(), recalled->size());
    m_host.Repaint();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::RefreshGhost
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::RefreshGhost()
{
    const std::wstring &  text  = m_box->GetText();
    bool                  atEnd = m_box->GetSelectionStart() == text.size() && m_box->GetSelectionEnd() == text.size();



    m_box->SetGhostText (m_completion.GetGhost (text, atEnd, m_history.GetLines()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::ShowHistoryList
//
//  The earlier lines in a list rising from the command line, from where the
//  typed text starts to a little short of the console's right edge, in the
//  command line's face and size, with the newest nearest the box. A long
//  line is cut, with no tip of the whole line. Home and End go to the oldest
//  and newest, and Up and Down stop at them. Picking one puts it in the box
//  to edit or run.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::ShowHistoryList()
{
    static constexpr size_t           s_kMaxRows     = 200;
    static constexpr int              s_kShownRows   = 10;
    static constexpr int              s_kRightGapDip = 24;   // between the list's right edge and the console's
    const std::vector<std::wstring> & lines          = m_history.GetLines();
    const DxuiTheme                 * theme          = m_host.GetTheme();
    const DxuiDpiScaler             & scaler         = m_host.GetPaneScaler();
    DxuiTextView                    * view           = m_output.GetView();
    PaneMenuItems                     items;
    RECT                              box            = m_box->GetBounds();
    RECT                              pane           = view->GetBounds();
    DxuiContextMenu::ListLook         look;
    size_t                            first          = 0;



    if (lines.empty() || theme == nullptr)
    {
        return;
    }

    first = (lines.size() > s_kMaxRows) ? lines.size() - s_kMaxRows : 0;

    for (size_t i = first; i < lines.size(); i++)
    {
        std::wstring  line = lines[i];

        items.push_back ({ line, [this, line]
        {
            m_box->SetText (line);
            m_box->SetSelection (line.size(), line.size());
            m_host.FocusControl (m_box);
            RefreshGhost();
            m_host.Repaint();
        } });
    }

    look.face                 = m_box->GetFace();
    look.metrics              = DxuiMenuMetrics::FromSystem (scaler.GetDpi());
    look.metrics.fontPx       = scaler.ToPxf (m_box->GetFontDip());
    look.metrics.minWidthPx   = 0;
    look.maxHeightPx          = look.metrics.rowHeightPx * s_kShownRows;
    look.isCutTipShown        = false;
    look.isWrapping           = false;

    //  Each command's text starts where the typed text does after the
    //  prompt, so the list's edge is apart from the console's beneath it.
    look.labelLeftAtPx = box.left + m_box->GetTypedTextLeftPx();

    m_host.ShowListAbove (items, RECT { pane.left, box.top, pane.right - scaler.ToPx (s_kRightGapDip), box.bottom }, look);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::SaveHistory
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::SaveHistory()
{
    IDebuggerWindowHost  * host = m_host.GetWindowHost();
    std::wstring           text;



    for (const std::wstring & line : m_history.GetLines())
    {
        text += line + L"\n";
    }

    if (host != nullptr)
    {
        host->SetDebuggerWindowState ("console", SourcePathList::WideToUtf8 (text));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeBar::ConsoleModeBar
//
////////////////////////////////////////////////////////////////////////////////

ConsoleModeBar::ConsoleModeBar (IDebuggerPaneHost & host) :
    m_host (host),
    m_bar  (host, DebuggerLayout::kConsole, L"Console commands")
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeBar::~ConsoleModeBar
//
////////////////////////////////////////////////////////////////////////////////

ConsoleModeBar::~ConsoleModeBar() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeBar::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleModeBar::Configure()
{
    DxuiToolbar::Entry  label;
    DxuiToolbar::Entry  dialect;



    m_label = std::make_unique<ToolbarLabelEntry> (L"Mode:");

    label.command       = m_label->GetCommand();
    label.custom        = m_label.get();
    label.pinned        = true;

    m_dialect        = std::make_shared<DxuiCommand>();
    m_dialect->id    = kDialectEntry;
    m_dialect->label = GetModeLabel (L"AppleWin");
    m_dialect->tip   = L"Choose the command mode for the console and the memory Address box";

    dialect.command       = m_dialect;
    dialect.kind          = DxuiToolbar::Kind::DropDown;
    dialect.pinned        = true;

    m_bar.Configure();
    m_bar.GetBar()->SetEntries ({ label, dialect, m_host.GetFindEntry (DebuggerLayout::kConsole) });
    m_bar.SetOnPress ([this] { SetMenus(); });

    SetMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeBar::GetModeChoices
//
//  Only the order is the drop-down's; the mode a choice sets, and so the
//  name saved for it, is the same.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<ConsoleModeBar::ModeChoice> & ConsoleModeBar::GetModeChoices()
{
    static const std::vector<ModeChoice>  kChoices =
    {
        { CommandMode::AppleWin,  L"AppleWin"  },
        { CommandMode::Casso,     L"Casso"     },
        { CommandMode::GSSquared, L"GSSquared" },
        { CommandMode::Monitor,   L"Monitor"   },
        { CommandMode::WinDbg,    L"WinDbg"    },
    };



    return kChoices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeBar::SetMenus
//
//  A pick changes the mode and nothing more: the prompt has the new one, and
//  the console gains no MODE line the user never typed. The entry is as wide
//  as its text, so a new mode lays the strip out again.
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleModeBar::SetMenus()
{
    const DebuggerViewSnapshot      * snapshot = m_host.GetLatestSnapshot();
    std::vector<DxuiPopupMenuItem>    modes;
    std::wstring                      before   = m_dialect->label;



    for (const auto & [mode, label] : GetModeChoices())
    {
        CommandMode  target  = mode;
        bool         current = (snapshot != nullptr) && snapshot->mode == mode;

        if (current || (snapshot == nullptr && mode == CommandMode::AppleWin))
        {
            m_dialect->label = GetModeLabel (label);
        }

        modes.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (label, current, [this, target]
        {
            DebuggerAction  action = DebuggerActions::GetSetMode (target, m_host.GetMode());



            action.quiet = true;
            m_host.RunAction (action);
        })));
    }

    m_bar.GetBar()->SetDropDownItems (kDialectEntry, std::move (modes));

    if (m_dialect->label != before)
    {
        m_bar.Place();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeBar::GetModeLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ConsoleModeBar::GetModeLabel (const wchar_t * mode)
{
    return std::wstring (mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeBar::GetPromptText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ConsoleModeBar::GetPromptText (CommandMode mode)
{
    switch (mode)
    {
    case CommandMode::Monitor:   return L"Monitor>";
    case CommandMode::GSSquared: return L"GSSquared>";
    case CommandMode::WinDbg:    return L"WinDbg>";
    case CommandMode::Casso:     return L"Casso>";
    default:                     return L"AppleWin>";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeBar::GetHelpCommand
//
//  Monitor mode runs HELP through its slash, as it runs every AppleWin
//  command.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ConsoleModeBar::GetHelpCommand (CommandMode mode)
{
    switch (mode)
    {
    case CommandMode::Monitor:   return L"/HELP";
    case CommandMode::GSSquared: return L"help";
    case CommandMode::WinDbg:    return L".help";
    default:                     return L"HELP";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::ConsolePane
//
////////////////////////////////////////////////////////////////////////////////

ConsolePane::ConsolePane (IDebuggerPaneHost & host) :
    IDebuggerPane  (host),
    m_output       (host),
    m_commandLine  (host, m_output),
    m_modeBar      (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::~ConsolePane
//
////////////////////////////////////////////////////////////////////////////////

ConsolePane::~ConsolePane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::GetId
//
////////////////////////////////////////////////////////////////////////////////

const std::wstring & ConsolePane::GetId() const
{
    static const std::wstring  s_kId = DebuggerLayout::kConsole;



    return s_kId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::CreateControls
//
////////////////////////////////////////////////////////////////////////////////

void ConsolePane::CreateControls()
{
    m_output.Create();
    m_commandLine.Create();
    m_modeBar.Create();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ConsolePane::Configure()
{
    m_output.Configure();
    m_commandLine.Configure();
    m_modeBar.Configure();

    //  Over a line's time stamp, the date and time in full; over a link, the
    //  file's full path.
    m_output.GetView()->SetTipFn ([this] (POINT clientPx, DxuiTip & tip) { return TryGetTipAt (clientPx, tip); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::AddToDock
//
//  The mode bar, a place PlaceBar fills, over the text, over the command
//  line. The command line's prompt, a pad inside its border, starts where
//  the text and the pane's title do.
//
////////////////////////////////////////////////////////////////////////////////

void ConsolePane::AddToDock (DxuiDockSite & dock)
{
    constexpr int  kBoxDip = 30;
    constexpr int  kPadDip = 4;



    m_frame = std::make_unique<DebuggerPaneFrame> (L"Console");
    m_frame->AddPart (m_modeBar.GetSlot().GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_output.GetView());
    m_frame->AddPart (m_commandLine.GetBox(), [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (kBoxDip); });
    m_frame->SetBottomMarginDip (kPadDip + 2);
    m_frame->SetPartTextAligned (m_commandLine.GetBox(), DxuiTextInput::kPadLeftDip);

    dock.AddPane (DebuggerLayout::kConsole, L"Console", m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * ConsolePane::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> ConsolePane::GetControls() const
{
    return { m_modeBar.GetSlot().GetSlot(), m_modeBar.GetSlot().GetBar(), m_output.GetView(), m_commandLine.GetBox() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void ConsolePane::Apply (const DebuggerViewSnapshot & snapshot)
{
    m_commandLine.Apply (snapshot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::SetTextZoom
//
////////////////////////////////////////////////////////////////////////////////

void ConsolePane::SetTextZoom (float zoom)
{
    m_output.GetView()->SetZoom (zoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::Tick
//
////////////////////////////////////////////////////////////////////////////////

bool ConsolePane::Tick (int64_t nowMs)
{
    m_output.Reflow();
    m_output.SaveIfChanged (nowMs);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::RouteEditKey
//
//  The command line's own keys: Ctrl+C with nothing selected in it copies
//  what is selected in the text above, since a click in the text puts the
//  focus in the command line; the completion keys; and Up and Down, which
//  walk the lines run before.
//
////////////////////////////////////////////////////////////////////////////////

bool ConsolePane::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    DxuiTextInput  * box  = m_commandLine.GetBox();
    DxuiTextView   * view = m_output.GetView();



    if (ev.kind != DxuiKeyEventKind::Down || focused != box || ev.alt)
    {
        return false;
    }

    if (ev.ctrl && ev.vk == 'C' && box->GetSelectionStart() == box->GetSelectionEnd() && view->HasSelection())
    {
        view->CopySelection();
        return true;
    }

    if (ev.ctrl)
    {
        return false;
    }

    if (m_commandLine.RouteCompletionKey (ev))
    {
        return true;
    }

    if (ev.vk == VK_UP || ev.vk == VK_DOWN)
    {
        (void) m_commandLine.Recall (ev.vk == VK_UP);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::SubmitTextBox
//
////////////////////////////////////////////////////////////////////////////////

bool ConsolePane::SubmitTextBox (DxuiTextInput * box)
{
    if (box == nullptr || box != m_commandLine.GetBox())
    {
        return false;
    }

    m_commandLine.Submit();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsolePane::TryGetTipAt
//
//  Over a line's time stamp, the date and the time in full; over a link, the
//  file's full path.
//
////////////////////////////////////////////////////////////////////////////////

bool ConsolePane::TryGetTipAt (POINT clientPx, DxuiTip & tip)
{
    if (!m_output.TryGetLinkTip (clientPx, tip.anchor, tip.text) && !m_output.TryGetTimeTip (clientPx, tip.anchor, tip.text))
    {
        return false;
    }

    tip.placement = DxuiTip::Placement::AtPointer;
    return true;
}
