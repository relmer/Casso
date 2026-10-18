#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/Panes/ToolPanes.h"
#include "Ui/Debugger/StepFiltersDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CreatePanes
//
//  The panes on the pane interface, each created and called in this order.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CreatePanes()
{
    IDebuggerPaneHost            & host     = *this;
    std::unique_ptr<ProfilerPane>  profiler = std::make_unique<ProfilerPane> (host);



    m_profilerPane = profiler.get();

    m_panes.push_back (std::make_unique<SymbolsPane>    (host));
    m_panes.push_back (std::make_unique<BookmarksPane>  (host));
    m_panes.push_back (std::move (profiler));
    m_panes.push_back (std::make_unique<DataBlocksPane> (host));

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->CreateControls();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FindPane
//
//  The pane on the pane interface with a layout id, or null.
//
////////////////////////////////////////////////////////////////////////////////

IDebuggerPane * DebuggerWindow::FindPane (const std::wstring & pane) const
{
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->GetId() == pane)
        {
            return each.get();
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetProfilerCommands
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::shared_ptr<DxuiCommand>> & DebuggerWindow::GetProfilerCommands() const
{
    return m_profilerPane->GetCommands();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunStepFiltersDialog
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::vector<std::wstring>> DebuggerWindow::RunStepFiltersDialog (const std::vector<std::wstring> & filters,
                                                                               std::function<bool (const std::wstring &, std::wstring &)> check)
{
    return StepFiltersDialog::RunModal (GetHwnd(), m_theme, filters, std::move (check));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EditStepFilters
//
//  Debug > Step filters... (FR-184): the dialog's list against the one it
//  opened with, each filter taken out run as SKIP - and each new one as
//  SKIP. A name must be a symbol, and the filter is added by that name.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EditStepFilters()
{
    std::vector<std::wstring>                  before;
    std::optional<std::vector<std::wstring>>   after;
    std::vector<DebuggerAction>                actions;
    auto                                       findSymbol = [this] (const std::wstring & name) -> std::optional<Word>
    {
        std::string  narrow = TextEncoding::WideToNarrow (name);



        if (m_snapshot == nullptr || m_snapshot->symbols == nullptr)
        {
            return std::nullopt;
        }

        for (const SymbolInfo & symbol : m_snapshot->symbols->symbols)
        {
            if (_stricmp (symbol.name.c_str(), narrow.c_str()) == 0)
            {
                return symbol.address;
            }
        }

        return std::nullopt;
    };



    if (m_snapshot == nullptr)
    {
        return;
    }

    for (const StepFilterEntry & entry : m_snapshot->stepFilters)
    {
        before.push_back (TextEncoding::NarrowToWide (DebuggerActions::GetRemoveSkip (entry, GetMode()).command.text));
    }

    after = RunStepFiltersDialog (before, [&findSymbol] (const std::wstring & text, std::wstring & error)
    {
        bool  isSymbol = findSymbol (text).has_value();



        error = isSymbol ? std::wstring() : std::format (L"{} is not a symbol, an address or a range.", text);
        return isSymbol;
    });

    if (!after.has_value())
    {
        return;
    }

    for (size_t i = 0; i < m_snapshot->stepFilters.size(); i++)
    {
        if (std::ranges::find (*after, before[i]) == after->end())
        {
            actions.push_back (DebuggerActions::GetRemoveSkip (m_snapshot->stepFilters[i], GetMode()));
        }
    }

    for (const std::wstring & text : *after)
    {
        Word                 first   = 0;
        Word                 last    = 0;
        std::optional<Word>  address;



        if (std::ranges::find (before, text) != before.end())
        {
            continue;
        }

        if (StepFiltersDialog::TryParseRange (text, first, last))
        {
            actions.push_back (DebuggerActions::GetSkipRange (first, last, GetMode()));
        }
        else if (address = findSymbol (text); address.has_value())
        {
            actions.push_back (DebuggerActions::GetSkipRoutine (TextEncoding::WideToNarrow (text), *address, GetMode()));
        }
    }

    for (const DebuggerAction & action : actions)
    {
        RunAction (action);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCursorAddress
//
//  The selected disassembly line's address, or the PC's.
//
////////////////////////////////////////////////////////////////////////////////

Word DebuggerWindow::GetCursorAddress() const
{
    int  line = GetCodeLineOfRow (m_activeCode, m_codeLists[(size_t) m_activeCode]->GetSelectedRow());



    if (line >= 0)
    {
        return GetCodeLines (m_activeCode)[(size_t) line].address;
    }

    return (m_snapshot != nullptr) ? m_snapshot->pc : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ToggleBookmarkAtCursor
//
//  Ctrl+K, K (FR-189): a bookmark on the selected line is removed, and one
//  is added where there is none.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ToggleBookmarkAtCursor()
{
    Word  at       = GetCursorAddress();
    auto  bookmark = std::ranges::find_if (m_snapshot->bookmarks, [at] (const DebuggerViewSnapshot::WatchLine & each) { return each.address == at; });



    RunAction (bookmark != m_snapshot->bookmarks.end() ? DebuggerActions::GetClearBookmark (bookmark->id, GetMode())
                                                       : DebuggerActions::GetAddBookmark (at, GetMode()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GoToBookmark
//
//  Ctrl+K, N and Ctrl+K, P: the next bookmark after the selected line, or
//  the one before it, round from the last to the first.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::GoToBookmark (bool isBackward)
{
    std::optional<Word>  next = BookmarksPane::GetNext (m_snapshot->bookmarks, GetCursorAddress(), isBackward);



    if (next.has_value())
    {
        ShowCode (*next);
    }
}
