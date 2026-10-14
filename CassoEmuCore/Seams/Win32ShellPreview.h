#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellPreview
//
//  Windows' own preview of a host file, as File Explorer's Preview pane draws
//  it: the file type's preview handler in a window of its own over the pane,
//  or, for a picture or a video, the shell's large thumbnail.
//
//  EVERYTHING SLOW RUNS ON A THREAD OF ITS OWN. A handler is created out of
//  process where it is registered for that, as Explorer creates it, and
//  loading one can take a second or more; the window only posts requests
//  and reads results, so the selection never waits on a handler. Each load
//  has a number, and a result for any load but the latest is dropped.
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
    //  load posts `message` to `owner`.
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

    static constexpr UINT  s_kLoadMessage   = WM_APP + 1;
    static constexpr UINT  s_kRectMessage   = WM_APP + 2;
    static constexpr UINT  s_kUnloadMessage = WM_APP + 3;

    static constexpr const wchar_t *  s_kpszHostClass        = L"CassoExplorerPreviewHost";
    static constexpr const wchar_t *  s_kpszPreviewHandlerId = L"{8895b1c6-b41f-4c1c-a562-0d564250836f}";

    static LRESULT CALLBACK  HostProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void     Run          ();
    void     RunLoad      (const Job & job);
    void     RunUnload    ();
    HRESULT  ShowHandler  (const Job & job);
    HRESULT  MakePicture  (const Job & job, Result & outResult);
    void     Publish      (Result result);

    static HRESULT  GetHandlerId    (const std::wstring & path, CLSID & outId);
    static bool     IsPictureOrVideo (const std::wstring & path);

    HWND                                       m_owner    = nullptr;
    UINT                                       m_message  = 0;
    HWND                                       m_host     = nullptr;
    DWORD                                      m_threadId = 0;
    std::thread                                m_thread;
    std::atomic<uint64_t>                      m_request  { 0 };
    std::mutex                                 m_lock;
    std::optional<Result>                      m_result;

    //  The worker thread's own: the handler in the host.
    Microsoft::WRL::ComPtr<IPreviewHandler>    m_handler;
};
