#include "Pch.h"

#include "Seams/Win32PreviewHelper.h"
#include "Core/AppTimerIds.h"
#include "Core/AppMessages.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::GetHandlerId
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PreviewHelper::GetHandlerId (const std::wstring & path, CLSID & outId)
{
    HRESULT       hr        = S_OK;
    wchar_t       text[64]  = {};
    DWORD         length    = (DWORD) std::size (text);
    std::wstring  extension = fs::path (path).extension().wstring();
    bool          known     = !extension.empty();



    outId = CLSID_NULL;

    CBREx (known, E_INVALIDARG);

    hr = AssocQueryStringW (ASSOCF_NONE, ASSOCSTR_SHELLEXTENSION, extension.c_str(), s_kpszPreviewHandlerId, text, &length);
    CHR (hr);

    hr = CLSIDFromString (text, &outId);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::GetSlot
//
//  Low integrity unless the handler's class sets
//  DisableLowILProcessIsolation, as in File Explorer.
//
////////////////////////////////////////////////////////////////////////////////

int Win32PreviewHelper::GetSlot (const CLSID & id)
{
    wchar_t       text[64] = {};
    int           written  = StringFromGUID2 (id, text, (int) std::size (text));
    std::wstring  key      = std::wstring (L"CLSID\\") + text;
    DWORD         optOut   = 0;
    DWORD         bytes    = (DWORD) sizeof (optOut);
    LSTATUS       status   = (written != 0) ? RegGetValueW (HKEY_CLASSES_ROOT, key.c_str(), L"DisableLowILProcessIsolation", RRF_RT_REG_DWORD, nullptr, &optOut, &bytes)
                                            : ERROR_INVALID_DATA;



    return (status == ERROR_SUCCESS && optOut != 0) ? kNormalSlot : kLowSlot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::Launch
//
//  This program again, with the switch, the owner's window and the slot; at
//  low integrity for the low slot.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PreviewHelper::Launch (HWND owner, int slot, HANDLE * outProcess)
{
    HRESULT              hr               = S_OK;
    wchar_t              exe[MAX_PATH]    = {};
    DWORD                length           = GetModuleFileNameW (nullptr, exe, (DWORD) std::size (exe));
    bool                 fits             = length != 0 && length < (DWORD) std::size (exe);
    std::wstring         command;
    STARTUPINFOW         startup          = { sizeof (startup) };
    PROCESS_INFORMATION  process          = {};
    HANDLE               token            = nullptr;
    BOOL                 ok               = FALSE;



    *outProcess = nullptr;

    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));

    command = L"\"" + std::wstring (exe) + L"\" " + kSwitch + L" " + std::to_wstring ((uintptr_t) owner) + L" " + std::to_wstring (slot);

    if (slot == kLowSlot)
    {
        hr = MakeLowIntegrityToken (&token);
        CHR (hr);

        ok = CreateProcessAsUserW (token, exe, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process);
    }
    else
    {
        ok = CreateProcessW (exe, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process);
    }

    CWR (ok);

    CloseHandle (process.hThread);
    *outProcess = process.hProcess;

Error:
    if (token != nullptr)
    {
        CloseHandle (token);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::SendLoad
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PreviewHelper::SendLoad (HWND helper, HWND owner, const LoadHeader & header, const std::wstring & path)
{
    HRESULT               hr     = S_OK;
    std::vector<uint8_t>  data   (sizeof (header) + path.size() * sizeof (wchar_t));
    COPYDATASTRUCT        copy   = {};
    DWORD_PTR             result = 0;
    LRESULT               sent   = 0;



    memcpy (data.data(), &header, sizeof (header));
    memcpy (data.data() + sizeof (header), path.data(), path.size() * sizeof (wchar_t));

    copy.dwData = s_kLoadCopyId;
    copy.cbData = (DWORD) data.size();
    copy.lpData = data.data();

    sent = SendMessageTimeoutW (helper, WM_COPYDATA, (WPARAM) owner, (LPARAM) &copy, SMTO_ABORTIFHUNG, s_kSendTimeoutMs, &result);
    CWR (sent != 0);
    CBR (result != 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::SendUnload
//
////////////////////////////////////////////////////////////////////////////////

void Win32PreviewHelper::SendUnload (HWND helper, HWND owner)
{
    COPYDATASTRUCT  copy   = {};
    DWORD_PTR       result = 0;
    LRESULT         sent   = 0;



    copy.dwData = s_kUnloadCopyId;

    sent = SendMessageTimeoutW (helper, WM_COPYDATA, (WPARAM) owner, (LPARAM) &copy, SMTO_ABORTIFHUNG, s_kSendTimeoutMs, &result);
    UNREFERENCED_PARAMETER (sent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::Run
//
//  A single-threaded apartment with a message loop, for the handler's calls
//  back into it. The window is a hidden popup until the owner reparents it.
//
////////////////////////////////////////////////////////////////////////////////

int Win32PreviewHelper::Run (HWND owner, int slot)
{
    HRESULT      hr     = S_OK;
    HRESULT      hrCom  = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    HINSTANCE    module = GetModuleHandleW (nullptr);
    WNDCLASSEXW  wc     = { sizeof (wc) };
    ATOM         atom   = 0;
    State        state;
    MSG          msg    = {};
    BOOL         posted = FALSE;
    bool         alive  = IsWindow (owner) != FALSE;



    CBREx (alive, E_INVALIDARG);

    state.owner = owner;
    state.slot  = slot;

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = module;
    wc.lpszClassName = s_kpszWindowClass;
    wc.hCursor       = LoadCursorW (nullptr, IDC_ARROW);

    atom = RegisterClassExW (&wc);
    CWR (atom != 0);

    state.window = CreateWindowExW (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, s_kpszWindowClass, L"", WS_POPUP | WS_CLIPCHILDREN,
                                    0, 0, 0, 0, nullptr, nullptr, module, &state);
    CWR (state.window != nullptr);

    SetTimer (state.window, (UINT_PTR) AppTimerId::PreviewHelperOwner, s_kOwnerCheckMs, nullptr);

    posted = PostMessageW (owner, (UINT) AppMessage::PreviewHelperReady, (WPARAM) slot, (LPARAM) state.window);
    CWR (posted);

    while (GetMessageW (&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage (&msg);
        DispatchMessageW (&msg);
    }

    UnloadHandler (state);

Error:
    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }

    return FAILED (hr) ? 1 : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::WindowProc
//
//  A load sent by the owner is run after the send returns, so the owner never
//  waits on a handler; the outcome is posted back. The helper ends when the
//  owner's window is gone.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Win32PreviewHelper::WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    State   * state  = nullptr;
    HRESULT   hr     = S_OK;
    RECT      client = {};
    HBRUSH    brush  = nullptr;



    if (msg == WM_NCCREATE)
    {
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, (LONG_PTR) ((CREATESTRUCTW *) lParam)->lpCreateParams);
    }

    state = (State *) GetWindowLongPtrW (hwnd, GWLP_USERDATA);

    if (state == nullptr)
    {
        return DefWindowProcW (hwnd, msg, wParam, lParam);
    }

    switch (msg)
    {
        case WM_COPYDATA:
            return ((HWND) wParam == state->owner && ReadCopy (*state, *(const COPYDATASTRUCT *) lParam)) ? TRUE : FALSE;

        case (UINT) AppMessage::PreviewHelperRunLoad:
            if (state->hasLoad)
            {
                state->hasLoad = false;
                UnloadHandler (*state);

                hr = ShowHandler (*state);
                PostMessageW (state->owner, (UINT) AppMessage::PreviewHelperResult, (WPARAM) state->header.request, (LPARAM) hr);
            }

            return 0;

        case (UINT) AppMessage::PreviewHelperUnload:
            UnloadHandler (*state);
            InvalidateRect (hwnd, nullptr, TRUE);
            return 0;

        case WM_SIZE:
            if (state->handler)
            {
                GetClientRect (hwnd, &client);
                hr = state->handler->SetRect (&client);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }

            return 0;

        case WM_ERASEBKGND:
            brush = CreateSolidBrush (state->background);
            GetClientRect (hwnd, &client);
            FillRect ((HDC) wParam, &client, brush);
            DeleteObject (brush);
            return 1;

        case WM_TIMER:
            if (wParam == (UINT_PTR) AppTimerId::PreviewHelperOwner && !IsWindow (state->owner))
            {
                DestroyWindow (hwnd);
            }

            return 0;

        case WM_DESTROY:
            PostQuitMessage (0);
            return 0;

        default:
            break;
    }

    return DefWindowProcW (hwnd, msg, wParam, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::ReadCopy
//
//  A load replaces any load not yet run; an unload drops it too.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PreviewHelper::ReadCopy (State & state, const COPYDATASTRUCT & copy)
{
    const uint8_t  * bytes = (const uint8_t *) copy.lpData;
    size_t           chars = 0;



    if (copy.dwData == s_kUnloadCopyId)
    {
        state.hasLoad = false;
        PostMessageW (state.window, (UINT) AppMessage::PreviewHelperUnload, 0, 0);
        return true;
    }

    if (copy.dwData != s_kLoadCopyId || bytes == nullptr || copy.cbData < sizeof (LoadHeader))
    {
        return false;
    }

    chars = (copy.cbData - sizeof (LoadHeader)) / sizeof (wchar_t);

    memcpy (&state.header, bytes, sizeof (LoadHeader));
    state.path.assign ((const wchar_t *) (bytes + sizeof (LoadHeader)), chars);
    state.background = state.header.background;
    state.hasLoad    = true;

    PostMessageW (state.window, (UINT) AppMessage::PreviewHelperRunLoad, 0, 0);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::ShowHandler
//
//  The handler is given the file by stream when it takes one, else as an
//  item, else by path, in File Explorer's order, and draws in this window.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PreviewHelper::ShowHandler (State & state)
{
    HRESULT                          hr       = S_OK;
    HRESULT                          hrColor  = S_OK;
    HRESULT                          hrStream = S_OK;
    HRESULT                          hrItem   = S_OK;
    HRESULT                          hrVisual = S_OK;
    CLSID                            id       = {};
    ComPtr<IPreviewHandler>          handler;
    ComPtr<IInitializeWithStream>    byStream;
    ComPtr<IInitializeWithItem>      byItem;
    ComPtr<IInitializeWithFile>      byFile;
    ComPtr<IPreviewHandlerVisuals>   visuals;
    ComPtr<IStream>                  stream;
    ComPtr<IShellItem>               item;
    RECT                             client   = {};



    hr = GetHandlerId (state.path, id);
    CHR (hr);

    hr = CreateHandler (id, &handler);
    CHR (hr);

    hrStream = handler.As (&byStream);
    hrItem   = handler.As (&byItem);

    if (SUCCEEDED (hrStream))
    {
        hr = SHCreateStreamOnFileEx (state.path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
        CHR (hr);

        hr = byStream->Initialize (stream.Get(), STGM_READ);
    }
    else if (SUCCEEDED (hrItem))
    {
        hr = SHCreateItemFromParsingName (state.path.c_str(), nullptr, IID_PPV_ARGS (&item));
        CHR (hr);

        hr = byItem->Initialize (item.Get(), STGM_READ);
    }
    else
    {
        hr = handler.As (&byFile);
        CHR (hr);

        hr = byFile->Initialize (state.path.c_str(), STGM_READ);
    }

    CHR (hr);

    //  A handler that takes colors draws on the pane's.
    hrVisual = handler.As (&visuals);

    if (SUCCEEDED (hrVisual))
    {
        hrColor = visuals->SetBackgroundColor (state.header.background);
        IGNORE_RETURN_VALUE (hrColor, S_OK);

        hrColor = visuals->SetTextColor (state.header.text);
        IGNORE_RETURN_VALUE (hrColor, S_OK);
    }

    GetClientRect (state.window, &client);

    hr = handler->SetWindow (state.window, &client);
    CHR (hr);

    hr = handler->DoPreview();
    CHR (hr);

    state.handler = handler;

Error:
    if (FAILED (hr) && handler)
    {
        handler->Unload();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::CreateHandler
//
//  In this process first, on this per-monitor DPI aware thread; then out of
//  process, for a handler registered only as a local server.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PreviewHelper::CreateHandler (const CLSID & id, IPreviewHandler ** outHandler)
{
    HRESULT  hr = CoCreateInstance (id, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (outHandler));



    if (FAILED (hr))
    {
        hr = CoCreateInstance (id, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS (outHandler));
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::UnloadHandler
//
////////////////////////////////////////////////////////////////////////////////

void Win32PreviewHelper::UnloadHandler (State & state)
{
    HRESULT  hr = S_OK;



    if (state.handler)
    {
        hr = state.handler->Unload();
        IGNORE_RETURN_VALUE (hr, S_OK);

        state.handler.Reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PreviewHelper::MakeLowIntegrityToken
//
//  A primary token for a new process: this process's, labeled low
//  integrity. Starting a process with a copy of the caller's own token
//  requires no privilege.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PreviewHelper::MakeLowIntegrityToken (HANDLE * outToken)
{
    HRESULT                   hr      = S_OK;
    HANDLE                    process = nullptr;
    PSID                      sid     = nullptr;
    SID_IDENTIFIER_AUTHORITY  label   = SECURITY_MANDATORY_LABEL_AUTHORITY;
    TOKEN_MANDATORY_LABEL     level   = {};
    BOOL                      ok      = FALSE;



    *outToken = nullptr;

    ok = OpenProcessToken (GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &process);
    CWR (ok);

    ok = DuplicateTokenEx (process, TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT, nullptr,
                           SecurityImpersonation, TokenPrimary, outToken);
    CWR (ok);

    ok = AllocateAndInitializeSid (&label, 1, SECURITY_MANDATORY_LOW_RID, 0, 0, 0, 0, 0, 0, 0, &sid);
    CWR (ok);

    level.Label.Sid        = sid;
    level.Label.Attributes = SE_GROUP_INTEGRITY;

    ok = SetTokenInformation (*outToken, TokenIntegrityLevel, &level, sizeof (level) + GetLengthSid (sid));
    CWR (ok);

Error:
    if (FAILED (hr) && *outToken != nullptr)
    {
        CloseHandle (*outToken);
        *outToken = nullptr;
    }

    if (sid != nullptr)
    {
        FreeSid (sid);
    }

    if (process != nullptr)
    {
        CloseHandle (process);
    }

    return hr;
}
