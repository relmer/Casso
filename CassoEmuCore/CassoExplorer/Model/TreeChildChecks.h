#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TreeChildChecks
//
//  Whether each host folder in the tree has anything the tree lists, checked
//  on a worker thread so a folder's rows are drawn before its subfolders are
//  opened. Each result is stored by path and the folder's modified time, so a
//  folder whose modified time matches isn't opened again.
//
//  Without a window to post to, a check runs at once on the caller's thread.
//  With SetManualRun, checks are queued until RunQueued, for tests.
//
////////////////////////////////////////////////////////////////////////////////

class TreeChildChecks
{
public:
    using Checker = std::function<bool (const std::wstring & path)>;

    struct Result
    {
        std::wstring  path;
        bool          hasChildren = false;
    };

    TreeChildChecks () : m_state (std::make_shared<State>()) {}
    ~TreeChildChecks ();

    TreeChildChecks (const TreeChildChecks &)             = delete;
    TreeChildChecks & operator= (const TreeChildChecks &) = delete;

    void  SetChecker   (Checker checker)          { m_state->checker = std::move (checker); }
    void  SetTarget    (HWND hwnd, UINT message);
    void  SetManualRun (bool manual)              { m_manual = manual; }

    //  True with the result when it is known for this modified time. False
    //  while the check is queued or running; a check not yet queued is queued.
    bool  TryGet       (const std::wstring & path, int64_t modifiedUnix, bool & outHasChildren);

    //  The checks finished since the last call, stored for later TryGet calls.
    std::vector<Result>  TakeResults ();

    //  Runs every queued check on the calling thread.
    void  RunQueued    ();

    //  Drops every stored result and queued check, as when the tree's filter
    //  changes. A check running now is discarded when it finishes.
    void  Clear        ();

    size_t  GetCheckCount () const { return m_state->checkCount; }

private:
    struct Request
    {
        std::wstring  path;
        int64_t       modifiedUnix = 0;
        uint64_t      generation   = 0;
    };

    struct Finished
    {
        Request  request;
        bool     hasChildren = false;
    };

    struct Stored
    {
        int64_t  modifiedUnix = 0;
        bool     hasChildren  = false;
    };

    struct State
    {
        std::mutex                         lock;
        std::condition_variable            queued;
        std::deque<Request>                queue;
        std::vector<Finished>              finished;
        std::map<std::wstring, int64_t>    pending;
        uint64_t                           generation = 0;
        bool                               stopping   = false;
        Checker                            checker;
        HWND                               hwnd       = nullptr;
        UINT                               message    = 0;
        std::atomic<size_t>                checkCount = 0;
    };

    static void  RunWorker (std::shared_ptr<State> state);
    static void  RunOne    (State & state, const Request & request);

    std::shared_ptr<State>          m_state;
    std::map<std::wstring, Stored>  m_stored;
    std::thread                     m_worker;
    bool                            m_manual = false;
};
