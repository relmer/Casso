#include "Pch.h"

#include "Shell/DebugProjectMerge.h"
#include "Core/TextEncoding.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/WatchHandlers.h"
#include "Shell/DebugViewModel.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/IDebuggerHost.h"
#include "Shell/IShellDebugContext.h"
#include "Ui/Debugger/DebuggerActions.h"





//  The lines of a project's script that clear the old state ahead of its own,
//  left out when its items are added to another project's.
static constexpr const char  * s_kClearLines[] = { "BPC *", "WC *", "ZPC *", "BMC *", "SKIP CLEAR" };





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectMerge::HasItems
//
//  Whether a project has a breakpoint, watch, bookmark or zero-page pointer
//  to offer; its settings alone are not offered.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugProjectMerge::HasItems (const DebugProject & project)
{
    return !project.breakpointIds.entries.empty() || !project.watchIds.entries.empty() || !project.bookmarkIds.entries.empty() ||
           std::ranges::any_of (project.engineScript, [] (const std::string & line) { return line.starts_with ("ZP "); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectMerge::Offer
//
//  The old project's breakpoint, watch, zero-page pointer and bookmark
//  lines, each section after the line that clears it, saved for PROJECT
//  MERGE; its settings, after SKIP CLEAR, are not offered. The prompt is
//  posted to the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProjectMerge::Offer (const DebugProject & old, const std::string & intoLabel)
{
    std::vector<std::string>  lines;
    bool                      hasItem = false;



    for (const std::string & line : old.engineScript)
    {
        if (line == "SKIP CLEAR")
        {
            break;
        }

        lines.push_back (line);
        hasItem = hasItem || std::ranges::find (s_kClearLines, line) == std::end (s_kClearLines);
    }

    if (!hasItem)
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_mutex);



        m_mergeLines = std::move (lines);
        m_mergeFrom  = old.scope.label;
        m_mergeInto  = intoLabel;
    }

    if (m_host.GetMainWindow() != nullptr)
    {
        PostMessageW (m_host.GetMainWindow(), (UINT) AppMessage::ShellConfirmProjectMerge, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectMerge::HasOffer
//
//  Whether the old project's items are set aside for the user's answer.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugProjectMerge::HasOffer()
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return !m_mergeLines.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectMerge::OnConfirm
//
//  UI thread: the prompt, and on Yes, PROJECT MERGE queued for the CPU thread.
//  The emulator's window is drawn on a timer while the prompt is up, since
//  the prompt's message loop replaces the window's.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProjectMerge::OnConfirm()
{
    std::wstring         question;
    HWND                 owner    = nullptr;
    const IDxuiTheme   * theme    = nullptr;
    int                  choice   = IDNO;



    {
        std::lock_guard<std::mutex>  lock (m_mutex);



        if (m_mergeLines.empty())
        {
            return;
        }

        question = TextEncoding::NarrowToWide (std::format ("The debug project for {} is open. Add the breakpoints, watches "
                                                            "and bookmarks set for {} to it?", m_mergeInto, m_mergeFrom));
    }

    //  Over the debugger's window the box is in the debugger's theme.
    m_context.GetQuestionOwner (false, owner, theme);

    m_host.BeginModalKeepAlive();
    choice = DxuiMessageBox (owner, theme, question.c_str(), L"Debug project", MB_YESNO | MB_ICONQUESTION);
    m_host.EndModalKeepAlive();

    if (choice == IDYES)
    {
        m_context.RunDebuggerAction (DebuggerActions::GetProject ("MERGE", std::string(), CommandMode::AppleWin));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectMerge::RunLines
//
//  CPU thread: the old project's items added to the open one's. An item the
//  open project has, by the same line, is not added again. A disable or
//  flag line refers to the item by its place in the old project's list, so it
//  is run for the id the item was given here, and not at all for an item
//  that was not added.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProjectMerge::RunLines()
{
    static constexpr std::string_view  kFlagVerbs[] = { "BPD ", "BPCHANGE ", "WD ", "ZPD " };
    std::vector<std::string>           merge;
    std::vector<std::string>           lines;
    std::set<std::string>              present;
    std::map<size_t, int>              mergedIds;
    std::string                        section;
    size_t                             place     = 0;



    {
        std::lock_guard<std::mutex>  lock (m_mutex);



        merge.swap (m_mergeLines);
    }

    {
        std::lock_guard<std::mutex>  viewLock (m_viewModel.GetLock());
        DebugSession               & session = m_context.GetDebugController()->GetSession();
        std::string                  script  = BreakpointHandlers::MakeScript (session) +
                                               WatchHandlers::MakeScript (session, WatchListKind::Watch) +
                                               WatchHandlers::MakeScript (session, WatchListKind::ZeroPage) +
                                               WatchHandlers::MakeScript (session, WatchListKind::Bookmark);
        std::istringstream           in (script);
        std::string                  each;



        while (std::getline (in, each))
        {
            present.insert (each);
        }

        for (const std::string & line : merge)
        {
            auto         verb = std::ranges::find_if (kFlagVerbs, [&line] (std::string_view v) { return line.starts_with (v); });
            std::string  run  = line;
            Reply        reply;



            if (std::ranges::find (s_kClearLines, line) != std::end (s_kClearLines))
            {
                section = line;
                place   = 0;
                mergedIds.clear();
                continue;
            }

            //  A zero-page pointer's slot is its id in both projects.
            if (verb != std::end (kFlagVerbs) && section != "ZPC *")
            {
                size_t  numberAt = verb->size();
                size_t  numberTo = line.find (' ', numberAt);
                size_t  index    = (size_t) std::strtoul (line.c_str() + numberAt, nullptr, 10);



                if (!mergedIds.contains (index))
                {
                    continue;
                }

                run = std::format ("{}{}{}", *verb, mergedIds[index], numberTo == std::string::npos ? std::string() : line.substr (numberTo));
            }
            else if (verb == std::end (kFlagVerbs) && line != "BRKUNINIT ON" && present.contains (line))
            {
                place++;
                continue;
            }

            reply = session.ExecuteLine (run, CommandMode::AppleWin);

            if (reply.status != CommandStatus::Ok)
            {
                lines.push_back ("Not added: " + line);
            }

            if (verb == std::end (kFlagVerbs) && line != "BRKUNINIT ON")
            {
                std::optional<int>  id = (reply.status == CommandStatus::Ok) ? GetMergedId (reply) : std::nullopt;



                if (id.has_value())
                {
                    mergedIds[place] = *id;
                }

                place++;
            }
        }
    }

    m_context.AddConsoleLines (lines);
    m_viewModel.MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectMerge::GetMergedId
//
//  The id a breakpoint, watch or bookmark line was given.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int> DebugProjectMerge::GetMergedId (const Reply & reply)
{
    if (const BreakpointSetData * set = std::get_if<BreakpointSetData> (&reply.data))
    {
        return set->breakpoint.id;
    }

    if (const WatchListData * list = std::get_if<WatchListData> (&reply.data); list != nullptr && list->entries.size() == 1)
    {
        return list->entries.front().id;
    }

    return std::nullopt;
}





