#include "Pch.h"

#include "OptionsDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope state
//
////////////////////////////////////////////////////////////////////////////////

// IDs are local to the dialog template; no resource.h entries required.
static constexpr WORD  s_kIdcDriveAudioCheck = 1001;
static constexpr WORD  s_kIdcMechanismLabel  = 1002;
static constexpr WORD  s_kIdcMechanismCombo  = 1003;
static constexpr WORD  s_kIdcOkButton        = IDOK;
static constexpr WORD  s_kIdcCancelButton    = IDCANCEL;

struct OptionsDialogState
{
    bool      driveAudioEnabled;
    wstring   mechanism;
    bool      accepted;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppendWide
//
//  Helper that writes a NUL-terminated UTF-16 string into a contiguous
//  byte buffer at a 2-byte-aligned offset. Used by BuildTemplate to
//  emit the variable-length text fields inside DLGTEMPLATEEX /
//  DLGITEMTEMPLATEEX.
//
////////////////////////////////////////////////////////////////////////////////

static void AppendWide (vector<BYTE> & buf, const wchar_t * text)
{
    size_t  i   = 0;
    size_t  len = 0;

    if (text == nullptr)
    {
        buf.push_back (0);
        buf.push_back (0);
        return;
    }

    len = wcslen (text);

    for (i = 0; i <= len; i++)
    {
        WORD  ch = static_cast<WORD> (text[i]);

        buf.push_back (static_cast<BYTE> (ch & 0xFF));
        buf.push_back (static_cast<BYTE> ((ch >> 8) & 0xFF));
    }
}


static void AlignDword (vector<BYTE> & buf)
{
    while ((buf.size() & 0x3) != 0)
    {
        buf.push_back (0);
    }
}


static void AppendItem (
    vector<BYTE> &  buf,
    DWORD           style,
    DWORD           exStyle,
    short           x,
    short           y,
    short           cx,
    short           cy,
    WORD            id,
    const wchar_t * windowClass,
    const wchar_t * title)
{
    DLGITEMTEMPLATE  item = {};

    AlignDword (buf);

    item.style           = style;
    item.dwExtendedStyle = exStyle;
    item.x               = x;
    item.y               = y;
    item.cx              = cx;
    item.cy              = cy;
    item.id              = id;

    for (size_t i = 0; i < sizeof (item); i++)
    {
        buf.push_back (reinterpret_cast<const BYTE *> (&item)[i]);
    }

    // Window class -- use the "atom-style" 0xFFFF prefix for the
    // standard controls.
    if (windowClass != nullptr)
    {
        WORD  marker = 0xFFFF;
        WORD  atom   = 0;

        if (wcscmp (windowClass, L"BUTTON") == 0)
        {
            atom = 0x0080;
        }
        else if (wcscmp (windowClass, L"STATIC") == 0)
        {
            atom = 0x0082;
        }
        else if (wcscmp (windowClass, L"COMBOBOX") == 0)
        {
            atom = 0x0085;
        }
        else
        {
            // Unsupported class; emit as a string instead.
            AppendWide (buf, windowClass);
            atom = 0;
        }

        if (atom != 0)
        {
            buf.push_back (static_cast<BYTE> (marker & 0xFF));
            buf.push_back (static_cast<BYTE> ((marker >> 8) & 0xFF));
            buf.push_back (static_cast<BYTE> (atom & 0xFF));
            buf.push_back (static_cast<BYTE> ((atom >> 8) & 0xFF));
        }
    }
    else
    {
        buf.push_back (0);
        buf.push_back (0);
    }

    AppendWide (buf, title);

    // Creation data length (0 = no creation data).
    buf.push_back (0);
    buf.push_back (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildTemplate
//
//  Constructs an in-memory DLGTEMPLATE describing the Options dialog.
//  Three controls: a "Drive Audio" check box, an OK button, a
//  Cancel button.
//
////////////////////////////////////////////////////////////////////////////////

static void BuildTemplate (vector<BYTE> & buf)
{
    DLGTEMPLATE      tmpl = {};

    tmpl.style           = WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION |
                           DS_MODALFRAME | DS_CENTER | DS_SETFONT;
    tmpl.dwExtendedStyle = 0;
    tmpl.cdit            = 5;
    tmpl.x               = 0;
    tmpl.y               = 0;
    tmpl.cx              = 220;
    tmpl.cy              = 130;

    buf.clear();

    for (size_t i = 0; i < sizeof (tmpl); i++)
    {
        buf.push_back (reinterpret_cast<const BYTE *> (&tmpl)[i]);
    }

    // Menu, class, title.
    AppendWide (buf, nullptr);
    AppendWide (buf, nullptr);
    AppendWide (buf, L"Options");

    // Font (required by DS_SETFONT): point size + face name.
    buf.push_back (8);
    buf.push_back (0);
    AppendWide (buf, L"MS Shell Dlg");

    AppendItem (buf,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                0, 14, 14, 180, 12,
                s_kIdcDriveAudioCheck,
                L"BUTTON",
                L"&Drive Audio");

    AppendItem (buf,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                0, 14, 38, 80, 10,
                s_kIdcMechanismLabel,
                L"STATIC",
                L"Disk II &mechanism:");

    AppendItem (buf,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                0, 96, 36, 110, 80,
                s_kIdcMechanismCombo,
                L"COMBOBOX",
                nullptr);

    AppendItem (buf,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                0, 54, 100, 50, 16,
                s_kIdcOkButton,
                L"BUTTON",
                L"OK");

    AppendItem (buf,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                0, 114, 100, 50, 16,
                s_kIdcCancelButton,
                L"BUTTON",
                L"Cancel");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogProc
//
////////////////////////////////////////////////////////////////////////////////

static INT_PTR CALLBACK DialogProc (
    HWND   hwnd,
    UINT   uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    OptionsDialogState *  state = nullptr;
    HWND                  hCombo = nullptr;
    LRESULT               sel    = 0;
    wchar_t               buf[32] = {};

    switch (uMsg)
    {
        case WM_INITDIALOG:
            state = reinterpret_cast<OptionsDialogState *> (lParam);
            SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (state));

            CheckDlgButton (hwnd, s_kIdcDriveAudioCheck,
                            state->driveAudioEnabled ? BST_CHECKED : BST_UNCHECKED);

            hCombo = GetDlgItem (hwnd, s_kIdcMechanismCombo);
            SendMessageW (hCombo, CB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM> (L"Shugart SA400"));
            SendMessageW (hCombo, CB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM> (L"Alps 2124A"));

            // Default to entry 0 (Shugart) unless the caller's
            // current selection matches entry 1 (Alps). Mapping is
            // local to this dialog -- callers only see the bare
            // mechanism token (L"Shugart" / L"Alps").
            SendMessageW (hCombo, CB_SETCURSEL,
                          (state->mechanism == L"Alps") ? 1 : 0,
                          0);
            return TRUE;

        case WM_COMMAND:
            state = reinterpret_cast<OptionsDialogState *> (
                GetWindowLongPtr (hwnd, GWLP_USERDATA));

            if (state == nullptr)
            {
                return FALSE;
            }

            if (LOWORD (wParam) == s_kIdcOkButton)
            {
                state->driveAudioEnabled =
                    (IsDlgButtonChecked (hwnd, s_kIdcDriveAudioCheck) == BST_CHECKED);

                hCombo = GetDlgItem (hwnd, s_kIdcMechanismCombo);
                sel    = SendMessageW (hCombo, CB_GETCURSEL, 0, 0);

                // Map combobox row -> mechanism token (the spec /
                // registry contract uses the unadorned brand name).
                state->mechanism = (sel == 1) ? L"Alps" : L"Shugart";

                // Defensive: if some future translation broke the
                // mapping above, read the combo string and fall back
                // on the first token match.
                if (state->mechanism != L"Alps" && state->mechanism != L"Shugart")
                {
                    SendMessageW (hCombo, CB_GETLBTEXT, sel,
                                  reinterpret_cast<LPARAM> (buf));
                    state->mechanism = (wcsncmp (buf, L"Alps", 4) == 0) ? L"Alps" : L"Shugart";
                }

                state->accepted = true;
                EndDialog (hwnd, IDOK);
                return TRUE;
            }

            if (LOWORD (wParam) == s_kIdcCancelButton)
            {
                state->accepted = false;
                EndDialog (hwnd, IDCANCEL);
                return TRUE;
            }

            return FALSE;

        case WM_CLOSE:
            EndDialog (hwnd, IDCANCEL);
            return TRUE;
    }

    return FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OptionsDialog::Show (
    HWND            hwndParent,
    HINSTANCE       hInstance,
    bool            currentDriveAudioEnabled,
    const wstring & currentMechanism,
    bool          & outDriveAudioEnabled,
    wstring       & outMechanism)
{
    HRESULT             hr     = S_OK;
    vector<BYTE>        tmpl;
    OptionsDialogState  state  = { currentDriveAudioEnabled, currentMechanism, false };
    INT_PTR             result = 0;

    BuildTemplate (tmpl);

    result = DialogBoxIndirectParam (
        hInstance,
        reinterpret_cast<LPCDLGTEMPLATE> (tmpl.data()),
        hwndParent,
        DialogProc,
        reinterpret_cast<LPARAM> (&state));

    if (result == -1)
    {
        hr = HRESULT_FROM_WIN32 (GetLastError());
        goto Error;
    }

    if (state.accepted)
    {
        outDriveAudioEnabled = state.driveAudioEnabled;
        outMechanism         = state.mechanism;
        hr                   = S_OK;
    }
    else
    {
        hr = S_FALSE;
    }

Error:
    return hr;
}
