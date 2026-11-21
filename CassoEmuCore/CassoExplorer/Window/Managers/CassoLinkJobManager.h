#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoLinkJobDialog.h"


struct ExplorerContext;
struct LinkOperation;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager
//
//  Each connection's latest disk job, and the job window that has a tab for
//  each. A job that ends opens the window on its tab, without activating it.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkJobManager
{
public:
    explicit CassoLinkJobManager (ExplorerContext & context);
    ~CassoLinkJobManager();

    //  The pending jobs looked at again: a new one takes its connection's
    //  tab, and one that ended opens the window there.
    void  Update     ();

    //  The window, on the newest job's tab, activated.
    void  ShowWindow ();
    bool  HasJobs    () const { return !m_jobs.empty(); }

private:
    struct Job
    {
        std::shared_ptr<LinkOperation>  operation;
        ULONGLONG                       startedMs = 0;   // once it runs
        ULONGLONG                       endedMs   = 0;   // once it ends
        ULONGLONG                       seenMs    = 0;   // when it was first seen
        bool                            isUnseen  = false;
    };

    void                           Show       (const std::string & link, bool activate);
    std::vector<CassoLinkJobView>  BuildViews ();
    void                           SelectFile (const std::wstring & imagePath, bool open);
    std::string                    GetNewest  () const;

    ExplorerContext                      & m_context;
    std::map<std::string, Job>             m_jobs;
    std::string                            m_selected;
    std::unique_ptr<CassoLinkJobDialog>    m_window;
};
