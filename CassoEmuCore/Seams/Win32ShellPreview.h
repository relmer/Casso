#pragma once

#include "Pch.h"

#include "Seams/Win32PreviewHelper.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview
//
//  Windows' own preview of a host file, as File Explorer's Preview pane draws
//  it: the file type's preview handler in a window over the pane, or, for a
//  picture or a video, the shell's large thumbnail.
//
//  A HANDLER RUNS IN A HELPER PROCESS, Win32PreviewHelper, whose window is
//  reparented into the host window over the pane; a thumbnail is made on a
//  thread of this process. Nothing slow runs on the owner's thread: the
//  window posts requests and reads results, so the selection never waits on
//  a handler. Each load has a number, and a result for any load but the
//  latest is dropped.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellPreview
{
public:
    enum class Outcome { Shown, Picture, Failed };

    struct Result
    {
        uint64_t               request = 0;
        Outcome                outcome = Outcome::Failed;
        std::vector<uint32_t>  bgra;
        int                    width   = 0;
        int                    height  = 0;
    };

    Win32ShellPreview() = default;
    ~Win32ShellPreview();

    //  The host window, a child of `owner`, and the thread. Each finished
    //  load posts `message` to `owner`; the owner calls OnHelperMessage
    //  with the helpers' messages.
    HRESULT  Start (HWND owner, UINT message);
    void     Stop  ();

    //  Whether a file has a preview handler, or is a picture or a video the
    //  shell has a thumbnail for.
    static bool  HasPreview (const std::wstring & path);

    //  Loads a preview at `rectPx` in the owner's client area, unloading the
    //  one before; returns the load's number.
    uint64_t  Load    (const std::wstring & path, const RECT & rectPx, COLORREF background, COLORREF text);
    void      SetRect (const RECT & rectPx);

    //  Makes the host visible over the pane, once a handler draws in it.
    void      ShowHost (bool show);
    void      Unload  ();

    //  A helper's message to the owner; returns whether it was one.
    bool  OnHelperMessage (UINT msg, WPARAM wParam, LPARAM lParam);

    //  The latest load's result, once; false when none has arrived.
    bool  TakeResult (Result & outResult);

    uint64_t  GetRequest () const { return m_request; }

private:
    struct Job
    {
        uint64_t      request    = 0;
        std::wstring  path;
        RECT          rect       = {};
        COLORREF      background = 0;
        COLORREF      text       = 0;
    };

    struct Helper
    {
        HANDLE  process = nullptr;
        HWND    window  = nullptr;
    };

    //  A load or an unload for a helper, sent on the sender thread.
    struct Send
    {
        HWND                            helper = nullptr;
        bool                            load   = false;
        Win32PreviewHelper::LoadHeader  header;
        std::wstring                    path;
    };

    static constexpr UINT   s_kLoadMessage  = WM_APP + 1;
    static constexpr DWORD  s_kStopWaitMs   = 1000;

    static constexpr const wchar_t *  s_kpszHostClass = L"CassoExplorerPreviewHost";

    static LRESULT CALLBACK  HostProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HRESULT  EnsureHelper   (int slot);
    HRESULT  SendLoad       ();
    void     SendUnload     (HWND helper);
    void     QueueSend      (Send send);
    void     RunSender      ();
    void     AdoptWindow    (int slot);
    void     HideHelpers    ();
    void     PostPictureJob (const Job & job);

    void     Run          ();
    void     RunLoad      (const Job & job);
    HRESULT  MakePicture  (const Job & job, Result & outResult);
    void     Publish      (Result result);

    static bool  IsPictureOrVideo (const std::wstring & path);

    HWND                                          m_owner        = nullptr;
    UINT                                          m_message      = 0;
    HWND                                          m_host         = nullptr;
    DWORD                                         m_threadId     = 0;
    std::thread                                   m_thread;
    std::atomic<uint64_t>                         m_request      { 0 };
    std::mutex                                    m_lock;
    std::optional<Result>                         m_result;

    //  The sender thread. A helper busy in a handler takes a sent message
    //  only once the handler returns, up to the send's timeout, so the
    //  owner's thread queues the message here instead of sending it.
    std::thread                                   m_sender;
    std::mutex                                    m_sendLock;
    std::condition_variable                       m_sendWake;
    std::deque<Send>                              m_sends;
    bool                                          m_sendStopping = false;

    //  The owner's thread's own: the helpers, the latest load, its helper,
    //  and whether that load waits for the helper's window.
    std::array<Helper, Win32PreviewHelper::kSlotCount>  m_helpers;
    Job                                                 m_current;
    int                                                 m_activeSlot = -1;
    bool                                                m_awaiting   = false;
};
