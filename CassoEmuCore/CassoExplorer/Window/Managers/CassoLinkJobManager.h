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

    //  A read that couldn't read every sector: its tab offers a raw capture
    //  of the disk or a read again with those sectors written as zeros.
    void  SetOffer   (const std::string & link, std::function<void()> captureRaw, std::function<void()> readAgain);

    //  The status bar's line for the running disk jobs, empty with none.
    std::wstring  DescribeJobs () const;

private:
    struct Job
    {
        std::shared_ptr<LinkOperation>  operation;
        ULONGLONG                       startedMs   = 0;   // once it runs
        ULONGLONG                       endedMs     = 0;   // once it ends
        ULONGLONG                       seenMs      = 0;   // when it was first seen
        bool                            isUnseen    = false;
        bool                            isCanceling = false;
    };

    struct Offer
    {
        std::function<void()>  captureRaw;
        std::function<void()>  readAgain;
    };

    void                           Show       (const std::string & link, bool activate);
    CassoLinkJobView               MakeView   (const std::string & link, const LinkOperation & operation) const;
    void                           TakeOffer  (const std::string & link, bool captureRaw);
    std::vector<CassoLinkJobView>  BuildViews ();
    void                           SelectFile (const std::wstring & imagePath, bool open);
    std::string                    GetNewest  () const;

    ExplorerContext                      & m_context;
    std::map<std::string, Job>             m_jobs;
    std::map<std::string, Offer>           m_offers;
    std::string                            m_selected;
    std::unique_ptr<CassoLinkJobDialog>    m_window;
    bool                                   m_isOpening = false;    // the window is being created, and its first refresh can't open another
};
