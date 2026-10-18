#include "Pch.h"

#include "Ui/Debugger/Panes/ConsolePanes.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/Source/SourcePathList.h"
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
    std::vector<DxuiTextView::Row>  rows;
    bool                            atEnd   = m_view->IsAtEnd();
    int                             top     = m_view->GetTopLine();
    int                             dropped = 0;
    auto                            first   = lines.begin();



    while (m_lines.empty() && first != lines.end() && (first->empty() || DebuggerViewState::IsCommandGap (*first)))
    {
        ++first;
    }

    if (first == lines.end())
    {
        return;
    }

    m_lines.insert (m_lines.end(), first, lines.end());

    if ((int) m_lines.size() > kLineLimit)
    {
        dropped = (int) m_lines.size() - kLineLimit;
        m_lines.erase (m_lines.begin(), m_lines.begin() + dropped);
    }

    for (const std::string & line : m_lines)
    {
        DxuiTextView::Row  row;

        if (DebuggerViewState::IsCommandGap (line))
        {
            row.cells  = { std::wstring() };
            row.height = DebuggerViewState::kCommandGapHeight;
        }
        else
        {
            row.cells = { TextEncoding::NarrowToWide (line) };
        }

        rows.push_back (std::move (row));
    }

    m_view->SetRows (std::move (rows));
    m_view->SetTopLine (atEnd ? m_view->GetLineCount() : top - dropped);

    if (!m_view->IsVisible())
    {
        m_host.MarkPaneTab (DebuggerLayout::kConsole);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleOutput::Clear
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleOutput::Clear()
{
    m_lines.clear();
    m_view->SetRows ({});
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
//  does in a terminal.
//
////////////////////////////////////////////////////////////////////////////////

bool ConsoleOutput::RouteMouse (const DxuiMouseEvent & ev, IDxuiControl * keys)
{
    if (!m_view->IsVisible() || !m_host.IsRoutable (m_view))
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
//  The earlier lines, newest first, in a menu over the command line; picking
//  one puts it in the box to edit or run.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::ShowHistoryList()
{
    static constexpr size_t           s_kMaxRows = 30;
    const std::vector<std::wstring> & lines      = m_history.GetLines();
    PaneMenuItems                     items;
    RECT                              box        = m_box->GetBounds();



    if (lines.empty())
    {
        return;
    }

    for (auto it = lines.rbegin(); it != lines.rend() && items.size() < s_kMaxRows; ++it)
    {
        std::wstring  line = *it;

        items.push_back ({ line, [this, line]
        {
            m_box->SetText (line);
            m_box->SetSelection (line.size(), line.size());
            m_host.FocusControl (m_box);
            RefreshGhost();
            m_host.Repaint();
        } });
    }

    m_host.ShowPopupMenu (items, POINT { box.left, box.top });
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
    label.neverOverflow = true;

    m_dialect        = std::make_shared<DxuiCommand>();
    m_dialect->id    = kDialectEntry;
    m_dialect->label = GetModeLabel (L"AppleWin");
    m_dialect->tip   = L"Choose the command mode for the console and the memory Address box";

    dialect.command       = m_dialect;
    dialect.kind          = DxuiToolbar::Kind::DropDown;
    dialect.neverOverflow = true;

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

std::wstring ConsolePane::GetId() const
{
    return DebuggerLayout::kConsole;
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
