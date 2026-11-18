#include "Pch.h"

#include "Shell/DebugWindowMailbox.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox::TakeUpdate
//
////////////////////////////////////////////////////////////////////////////////

bool DebugWindowMailbox::TakeUpdate (
    std::shared_ptr<const DebuggerViewSnapshot>  & snapshot,
    std::vector<std::string>                     & consoleLines)
{
    std::lock_guard<std::mutex>  held (m_mutex);
    bool                         any  = m_isSnapshotFresh || !m_consolePending.empty();



    if (m_isSnapshotFresh)
    {
        snapshot          = m_snapshot;
        m_isSnapshotFresh = false;
    }

    consoleLines.swap (m_consolePending);
    m_consolePending.clear();

    return any;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox::PublishSnapshot
//
////////////////////////////////////////////////////////////////////////////////

void DebugWindowMailbox::PublishSnapshot (std::shared_ptr<const DebuggerViewSnapshot> snapshot)
{
    std::lock_guard<std::mutex>  held (m_mutex);



    m_snapshot        = std::move (snapshot);
    m_isSnapshotFresh = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox::IsSnapshotTaken
//
//  The window took the last snapshot, so building another is of use.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugWindowMailbox::IsSnapshotTaken()
{
    std::lock_guard<std::mutex>  held (m_mutex);



    return !m_isSnapshotFresh;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox::AddConsoleLine
//
////////////////////////////////////////////////////////////////////////////////

void DebugWindowMailbox::AddConsoleLine (const std::string & line)
{
    std::lock_guard<std::mutex>  held (m_mutex);



    m_consolePending.push_back (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox::AddConsoleLines
//
////////////////////////////////////////////////////////////////////////////////

void DebugWindowMailbox::AddConsoleLines (const std::vector<std::string> & lines)
{
    std::lock_guard<std::mutex>  held (m_mutex);



    m_consolePending.insert (m_consolePending.end(), lines.begin(), lines.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox::QueueAction
//
//  The action waits here, since a command crosses to the CPU thread as text
//  and an action must not.
//
////////////////////////////////////////////////////////////////////////////////

void DebugWindowMailbox::QueueAction (const DebuggerAction & action, bool isEchoed)
{
    std::lock_guard<std::mutex>  held (m_mutex);



    m_actionsPending.push_back (action);
    m_actionsPending.back().isEchoed = isEchoed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox::TakeActions
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DebuggerAction> DebugWindowMailbox::TakeActions()
{
    std::lock_guard<std::mutex>  held (m_mutex);
    std::vector<DebuggerAction>  actions;



    actions.swap (m_actionsPending);
    return actions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox::AddEventLogs
//
//  Each log's deltas are moved to the end of the ones waiting.
//
////////////////////////////////////////////////////////////////////////////////

void DebugWindowMailbox::AddEventLogs (DebugLogDeltas & deltas)
{
    std::lock_guard<std::mutex>  held (m_mutex);



    std::ranges::move (deltas.disk,    std::back_inserter (m_eventLogsPending.disk));
    std::ranges::move (deltas.input,   std::back_inserter (m_eventLogsPending.input));
    std::ranges::move (deltas.video,   std::back_inserter (m_eventLogsPending.video));
    std::ranges::move (deltas.sound,   std::back_inserter (m_eventLogsPending.sound));
    std::ranges::move (deltas.serial,  std::back_inserter (m_eventLogsPending.serial));
    std::ranges::move (deltas.network, std::back_inserter (m_eventLogsPending.network));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox::TakeEventLogs
//
////////////////////////////////////////////////////////////////////////////////

void DebugWindowMailbox::TakeEventLogs (DebugLogDeltas & deltas)
{
    std::lock_guard<std::mutex>  held (m_mutex);



    deltas.disk.clear();
    deltas.input.clear();
    deltas.video.clear();
    deltas.sound.clear();
    deltas.serial.clear();
    deltas.network.clear();

    deltas.disk.swap    (m_eventLogsPending.disk);
    deltas.input.swap   (m_eventLogsPending.input);
    deltas.video.swap   (m_eventLogsPending.video);
    deltas.sound.swap   (m_eventLogsPending.sound);
    deltas.serial.swap  (m_eventLogsPending.serial);
    deltas.network.swap (m_eventLogsPending.network);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox::ClearEventLogs
//
////////////////////////////////////////////////////////////////////////////////

void DebugWindowMailbox::ClearEventLogs()
{
    std::lock_guard<std::mutex>  held (m_mutex);



    m_eventLogsPending = DebugLogDeltas();
}
