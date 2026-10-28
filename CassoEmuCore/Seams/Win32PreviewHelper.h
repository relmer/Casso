#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper
//
//  The process a shell preview handler runs in: a copy of this program,
//  started with kSwitch, hosting one handler at a time in a window that the
//  owner reparents into its Preview pane.
//
//  FILE EXPLORER RUNS HANDLERS THE SAME WAY. Its preview host is a process at
//  low integrity, unless the handler's class sets
//  DisableLowILProcessIsolation, whose window Explorer reparents into its
//  own; a process may reparent a lower-integrity window into its own, and a
//  lower-integrity process may not create one inside a higher one's. Running
//  handlers at File Explorer's level lets them share state with its copies of
//  them, such as the WebView2 browser all of PowerToys' previewers use, which
//  only callers at the level that started it can use. The handler is loaded
//  into the helper, whose thread is per-monitor DPI aware; the shell's own
//  preview host, created from outside File Explorer, calls a handler from a
//  thread that is not, and a handler such as Edge's PDF preview then sizes
//  itself by the primary monitor's scale.
//
//  The owner sends loads and unloads as WM_COPYDATA; the helper posts
//  kReadyMessage once its window exists and kResultMessage for each load, and
//  exits when the owner's window is gone.
//
////////////////////////////////////////////////////////////////////////////////

class Win32PreviewHelper
{
public:
    //  Posted to the owner: the helper's window, with the slot in wParam and
    //  the window in lParam; and a load's outcome, with the load's number in
    //  wParam and its HRESULT in lParam.
    static constexpr UINT  kReadyMessage  = WM_APP + 0x3B;
    static constexpr UINT  kResultMessage = WM_APP + 0x3C;

    static constexpr const wchar_t *  kSwitch = L"--preview-host";

    //  A helper at low integrity, and one at the owner's own level.
    static constexpr int  kLowSlot    = 0;
    static constexpr int  kNormalSlot = 1;
    static constexpr int  kSlotCount  = 2;

    //  A load's fixed part; the file's path follows it.
    struct LoadHeader
    {
        uint64_t  request    = 0;
        COLORREF  background = 0;
        COLORREF  text       = 0;
    };

    //  The class registered as a file type's preview handler, as File
    //  Explorer finds it: under the type's shellex key, by the handler
    //  interface's id.
    static HRESULT  GetHandlerId (const std::wstring & path, CLSID & outId);

    //  The helper slot a handler runs in.
    static int  GetSlot (const CLSID & id);

    //  The owner's side.
    static HRESULT  Launch     (HWND owner, int slot, HANDLE * outProcess);
    static HRESULT  SendLoad   (HWND helper, HWND owner, const LoadHeader & header, const std::wstring & path);
    static void     SendUnload (HWND helper, HWND owner);

    //  The helper's side, from the program's entry point; returns the exit
    //  code once the owner's window is gone.
    static int  Run (HWND owner, int slot);

private:
    struct State
    {
        HWND                                       owner      = nullptr;
        int                                        slot       = 0;
        HWND                                       window     = nullptr;
        COLORREF                                   background = 0;
        LoadHeader                                 header;
        std::wstring                               path;
        bool                                       hasLoad    = false;
        Microsoft::WRL::ComPtr<IPreviewHandler>    handler;
    };

    static constexpr ULONG_PTR  s_kLoadCopyId      = 0x43455031;
    static constexpr ULONG_PTR  s_kUnloadCopyId    = 0x43455032;
    static constexpr UINT       s_kRunLoadMessage  = WM_APP + 1;
    static constexpr UINT       s_kUnloadMessage   = WM_APP + 2;
    static constexpr UINT       s_kOwnerCheckMs    = 2000;
    static constexpr UINT       s_kSendTimeoutMs   = 2000;

    static constexpr const wchar_t *  s_kpszWindowClass      = L"CassoExplorerPreviewHelper";
    static constexpr const wchar_t *  s_kpszPreviewHandlerId = L"{8895b1c6-b41f-4c1c-a562-0d564250836f}";

    static LRESULT CALLBACK  WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static bool     ReadCopy              (State & state, const COPYDATASTRUCT & copy);
    static HRESULT  ShowHandler           (State & state);
    static HRESULT  CreateHandler         (const CLSID & id, IPreviewHandler ** outHandler);
    static void     UnloadHandler         (State & state);
    static HRESULT  MakeLowIntegrityToken (HANDLE * outToken);
};
