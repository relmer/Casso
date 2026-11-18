#pragma once

#include "Debugger/Project/DebugProject.h"

class DebugViewModel;
class IDebuggerHost;
class IShellDebugContext;
struct Reply;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectMerge
//
//  The offer, when a more specific debug project opens, to add the last
//  project's breakpoints, watches, zero-page pointers and bookmarks to it.
//  The offer's lines are set aside on the CPU thread, the question is put on
//  the UI thread, and a yes runs PROJECT MERGE, whose lines are run on the
//  CPU thread on the next service tick.
//
////////////////////////////////////////////////////////////////////////////////

class DebugProjectMerge
{
public:
    DebugProjectMerge (IDebuggerHost & host, IShellDebugContext & context, DebugViewModel & viewModel) :
        m_host      (host),
        m_context   (context),
        m_viewModel (viewModel)
    {
    }

    //  Whether a project has a breakpoint, watch, bookmark or zero-page
    //  pointer to offer.
    static bool  HasItems  (const DebugProject & project);

    void         Offer     (const DebugProject & old, const std::string & intoLabel);
    bool         HasOffer  ();
    void         OnConfirm ();

    //  PROJECT MERGE: owed by the command, and run on the service tick.
    void         MarkOwed  ()       { m_isOwed = true; }
    bool         TakeOwed  ()       { return std::exchange (m_isOwed, false); }
    void         RunLines  ();

private:
    static std::optional<int>  GetMergedId (const Reply & reply);

    IDebuggerHost             & m_host;
    IShellDebugContext        & m_context;
    DebugViewModel            & m_viewModel;
    std::mutex                  m_mutex;
    std::vector<std::string>    m_mergeLines;
    std::string                 m_mergeFrom;
    std::string                 m_mergeInto;
    bool                        m_isOwed = false;
};
