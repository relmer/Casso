#include "Pch.h"

#include "DiskIIDebugDialog.h"
#include "RichEditSquiggle.h"
#include "Ehm.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope constants and helpers
//
////////////////////////////////////////////////////////////////////////////////

static const wchar_t  s_kpszDebugWndClass[]      = L"CassoDiskIIDebugWindow";
static const wchar_t  s_kpszDebugWndTitle[]      = L"Disk II Debug";

static const wchar_t  s_kpszFilterRichEditClass[]  = L"RICHEDIT50W";
static const wchar_t  s_kpszFilterRichEditModule[] = L"msftedit.dll";

static HMODULE        s_hMsftEditModule          = nullptr;

static constexpr int  kDialogDefaultWidth   = 900;
static constexpr int  kDialogDefaultHeight  = 600;

static constexpr int  kDrainTimerMs         = 33;
static constexpr int  kFilterTextDebounceMs = 250;

static constexpr int  kMargin               = 8;
static constexpr int  kRowHeight            = 22;
static constexpr int  kCheckWidth           = 92;
static constexpr int  kAudioCheckWidth      = 86;
static constexpr int  kRadioWidth           = 60;
static constexpr int  kEditWidth            = 140;
static constexpr int  kLabelWidth           = 56;
static constexpr int  kIgnoredLabelHeight   = 18;
static constexpr int  kButtonWidth          = 90;
static constexpr int  kButtonHeight         = 26;
static constexpr int  kRowGap               = 4;

// Control IDs. Range 100-199 reserved for the dialog's own children.
enum DiskIIDebugDialogCtrlId : int
{
    kIdListView           = 100,

    kIdChkEventTypeFirst  = 110,    // 8 contiguous slots
    kIdChkAudioMaster     = 119,

    kIdChkAudioSubFirst   = 120,    // 4 contiguous slots

    kIdRdoDriveFirst      = 130,    // 3 contiguous slots

    kIdEdtTrack           = 140,
    kIdEdtSector          = 141,
    kIdLblTrackIgnored    = 142,
    kIdLblSectorIgnored   = 143,
    kIdChkTrackRawQt      = 144,

    kIdBtnPause           = 150,
    kIdBtnClear           = 151,
};

static const wchar_t * const  s_kpszEventTypeLabels[8] =
{
    L"Motor",
    L"HeadStep",
    L"HeadBump",
    L"AddrMark",
    L"Read",
    L"Write",
    L"Door",
    L"DriveSelect",
};

static const wchar_t * const  s_kpszAudioSubLabels[4] =
{
    L"Started",
    L"Restarted",
    L"Continued",
    L"Silent",
};

static const wchar_t * const  s_kpszDriveLabels[3] =
{
    L"All",
    L"Drive 1",
    L"Drive 2",
};

static bool   s_classRegistered  = false;





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureMsftEditLoaded
//
//  RichEdit 4.1 (RICHEDIT50W) is exposed by msftedit.dll. We load the
//  module exactly once on first dialog open and intentionally leak the
//  handle for the process lifetime per plan.md (process-lifetime cost
//  is negligible and standard for RichEdit usage).
//
////////////////////////////////////////////////////////////////////////////////

static void EnsureMsftEditLoaded ()
{
    if (s_hMsftEditModule == nullptr)
    {
        s_hMsftEditModule = LoadLibraryW (s_kpszFilterRichEditModule);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugDialog
//
////////////////////////////////////////////////////////////////////////////////

DiskIIDebugDialog::DiskIIDebugDialog ()
{
    m_kpszWndClass  = s_kpszDebugWndClass;
    m_hbrBackground = reinterpret_cast<HBRUSH> (COLOR_BTNFACE + 1);
    m_uptimeAnchor  = std::chrono::steady_clock::now();

    SeedDefaultColumns (m_columns);

    for (int i = 0; i < kColumnCount; i++)
    {
        m_visibleOrdinalToLogicalId[i] = i;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DiskIIDebugDialog
//
////////////////////////////////////////////////////////////////////////////////

DiskIIDebugDialog::~DiskIIDebugDialog ()
{
    Destroy ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIDebugDialog::Create (HINSTANCE hInstance, HWND parentHwnd)
{
    HRESULT hr = S_OK;

    if (m_hwnd != nullptr)
    {
        return S_OK;
    }

    EnsureMsftEditLoaded ();

    if (!s_classRegistered)
    {
        hr = Window::Initialize (hInstance);
        CHR (hr);

        s_classRegistered = true;
    }
    else
    {
        m_hInstance = hInstance;
    }

    hr = Window::Create (0,
                         s_kpszDebugWndTitle,
                         WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         kDialogDefaultWidth, kDialogDefaultHeight,
                         parentHwnd);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::Show ()
{
    if (m_hwnd == nullptr)
    {
        return;
    }

    ShowWindow (m_hwnd, SW_SHOW);
    SetForegroundWindow (m_hwnd);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::Hide ()
{
    if (m_hwnd == nullptr)
    {
        return;
    }

    ShowWindow (m_hwnd, SW_HIDE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Destroy
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::Destroy ()
{
    if (m_hwnd != nullptr)
    {
        DestroyWindow (m_hwnd);
        m_hwnd = nullptr;
    }

    if (m_uiFont != nullptr)
    {
        DeleteObject (m_uiFont);
        m_uiFont = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AcquireUiFont
//
//  Cache the standard message-box font and apply it to every child
//  control we create. Caller owns nothing; the dialog deletes the
//  font in Destroy().
//
////////////////////////////////////////////////////////////////////////////////

HFONT DiskIIDebugDialog::AcquireUiFont ()
{
    NONCLIENTMETRICSW  ncm = {};

    if (m_uiFont != nullptr)
    {
        return m_uiFont;
    }

    ncm.cbSize = sizeof (ncm);

    if (SystemParametersInfoW (SPI_GETNONCLIENTMETRICS, sizeof (ncm), &ncm, 0))
    {
        m_uiFont = CreateFontIndirectW (&ncm.lfMessageFont);
    }

    if (m_uiFont == nullptr)
    {
        m_uiFont = reinterpret_cast<HFONT> (GetStockObject (DEFAULT_GUI_FONT));
    }

    return m_uiFont;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DiskIIDebugDialog::OnCreate (HWND hwnd, CREATESTRUCT * pcs)
{
    HRESULT  hr  = S_OK;
    RECT     rc  = {};

    UNREFERENCED_PARAMETER (pcs);

    m_hwnd = hwnd;

    hr = CreateChildControls (hwnd);
    CHR (hr);

    RebuildListViewColumns ();

    if (GetClientRect (hwnd, &rc))
    {
        LayoutChildControls (rc.right - rc.left, rc.bottom - rc.top);
    }

    SetTimer (hwnd, m_drainTimerId, kDrainTimerMs, nullptr);
    m_drainTimerActive = true;

Error:
    return SUCCEEDED (hr) ? 0 : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateChildControls
//
//  Creates the FR-014 filter controls, Pause/Clear buttons, and the
//  virtual-mode ListView. Layout is deferred to LayoutChildControls
//  so the WM_SIZE handler can re-flow on user resize.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIDebugDialog::CreateChildControls (HWND hwnd)
{
    HRESULT  hr   = S_OK;
    HFONT    font = nullptr;
    int      i    = 0;

    font = AcquireUiFont ();

    for (i = 0; i < 8; i++)
    {
        m_eventTypeChecks[i] = CreateWindowExW (0,
                                                L"BUTTON",
                                                s_kpszEventTypeLabels[i],
                                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                                0, 0, 0, 0,
                                                hwnd,
                                                reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdChkEventTypeFirst + i)),
                                                m_hInstance,
                                                nullptr);
        CWRA (m_eventTypeChecks[i]);
        SendMessageW (m_eventTypeChecks[i], WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);
        SendMessageW (m_eventTypeChecks[i], BM_SETCHECK, BST_CHECKED, 0);
    }

    m_audioMasterCheck = CreateWindowExW (0,
                                          L"BUTTON",
                                          L"Audio",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                          0, 0, 0, 0,
                                          hwnd,
                                          reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdChkAudioMaster)),
                                          m_hInstance,
                                          nullptr);
    CWRA (m_audioMasterCheck);
    SendMessageW (m_audioMasterCheck, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);
    SendMessageW (m_audioMasterCheck, BM_SETCHECK, BST_CHECKED, 0);

    for (i = 0; i < 4; i++)
    {
        m_audioSubCheck[i] = CreateWindowExW (0,
                                              L"BUTTON",
                                              s_kpszAudioSubLabels[i],
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                              0, 0, 0, 0,
                                              hwnd,
                                              reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdChkAudioSubFirst + i)),
                                              m_hInstance,
                                              nullptr);
        CWRA (m_audioSubCheck[i]);
        SendMessageW (m_audioSubCheck[i], WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);
        SendMessageW (m_audioSubCheck[i], BM_SETCHECK, BST_CHECKED, 0);
    }

    for (i = 0; i < 3; i++)
    {
        DWORD  style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON;

        if (i == 0)
        {
            style |= WS_GROUP;
        }

        m_driveRadio[i] = CreateWindowExW (0,
                                           L"BUTTON",
                                           s_kpszDriveLabels[i],
                                           style,
                                           0, 0, 0, 0,
                                           hwnd,
                                           reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdRdoDriveFirst + i)),
                                           m_hInstance,
                                           nullptr);
        CWRA (m_driveRadio[i]);
        SendMessageW (m_driveRadio[i], WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);
    }

    SendMessageW (m_driveRadio[0], BM_SETCHECK, BST_CHECKED, 0);

    m_trackRawQtCheck = CreateWindowExW (0,
                                         L"BUTTON",
                                         L"raw qt",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_AUTOCHECKBOX,
                                         0, 0, 0, 0,
                                         hwnd,
                                         reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdChkTrackRawQt)),
                                         m_hInstance,
                                         nullptr);
    CWRA (m_trackRawQtCheck);
    SendMessageW (m_trackRawQtCheck, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);

    m_trackRichEdit = CreateWindowExW (WS_EX_CLIENTEDGE,
                                       s_kpszFilterRichEditClass,
                                       L"",
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                       0, 0, 0, 0,
                                       hwnd,
                                       reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdEdtTrack)),
                                       m_hInstance,
                                       nullptr);
    CWRA (m_trackRichEdit);
    SendMessageW (m_trackRichEdit, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);
    SendMessageW (m_trackRichEdit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_KEYEVENTS);

    m_sectorRichEdit = CreateWindowExW (WS_EX_CLIENTEDGE,
                                        s_kpszFilterRichEditClass,
                                        L"",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                        0, 0, 0, 0,
                                        hwnd,
                                        reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdEdtSector)),
                                        m_hInstance,
                                        nullptr);
    CWRA (m_sectorRichEdit);
    SendMessageW (m_sectorRichEdit, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);
    SendMessageW (m_sectorRichEdit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_KEYEVENTS);

    m_trackIgnoredLabel = CreateWindowExW (0,
                                           L"STATIC",
                                           L"",
                                           WS_CHILD | WS_VISIBLE | SS_LEFT,
                                           0, 0, 0, 0,
                                           hwnd,
                                           reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdLblTrackIgnored)),
                                           m_hInstance,
                                           nullptr);
    CWRA (m_trackIgnoredLabel);
    SendMessageW (m_trackIgnoredLabel, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);

    m_sectorIgnoredLabel = CreateWindowExW (0,
                                            L"STATIC",
                                            L"",
                                            WS_CHILD | WS_VISIBLE | SS_LEFT,
                                            0, 0, 0, 0,
                                            hwnd,
                                            reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdLblSectorIgnored)),
                                            m_hInstance,
                                            nullptr);
    CWRA (m_sectorIgnoredLabel);
    SendMessageW (m_sectorIgnoredLabel, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);

    m_pauseButton = CreateWindowExW (0,
                                     L"BUTTON",
                                     L"Pause",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                     0, 0, 0, 0,
                                     hwnd,
                                     reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdBtnPause)),
                                     m_hInstance,
                                     nullptr);
    CWRA (m_pauseButton);
    SendMessageW (m_pauseButton, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);

    m_clearButton = CreateWindowExW (0,
                                     L"BUTTON",
                                     L"Clear",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                     0, 0, 0, 0,
                                     hwnd,
                                     reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdBtnClear)),
                                     m_hInstance,
                                     nullptr);
    CWRA (m_clearButton);
    SendMessageW (m_clearButton, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);

    m_listView = CreateWindowExW (WS_EX_CLIENTEDGE,
                                  WC_LISTVIEWW,
                                  L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                      | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS,
                                  0, 0, 0, 0,
                                  hwnd,
                                  reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdListView)),
                                  m_hInstance,
                                  nullptr);
    CWRA (m_listView);
    SendMessageW (m_listView, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);
    ListView_SetExtendedListViewStyle (m_listView, LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutChildControls
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::LayoutChildControls (int width, int height)
{
    int  x = 0;
    int  y = 0;
    int  i = 0;
    int  listViewTop = 0;

    if (m_listView == nullptr)
    {
        return;
    }

    // Row 1: event-type checkboxes
    x = kMargin;
    y = kMargin;

    for (i = 0; i < 8; i++)
    {
        MoveWindow (m_eventTypeChecks[i], x, y, kCheckWidth, kRowHeight, TRUE);
        x += kCheckWidth;
    }

    // Row 2: Audio master + 4 sub-checkboxes
    x = kMargin;
    y += kRowHeight + kRowGap;

    MoveWindow (m_audioMasterCheck, x, y, kCheckWidth, kRowHeight, TRUE);
    x += kCheckWidth;

    for (i = 0; i < 4; i++)
    {
        MoveWindow (m_audioSubCheck[i], x, y, kAudioCheckWidth, kRowHeight, TRUE);
        x += kAudioCheckWidth;
    }

    // Row 3: Drive radio + raw-qt + Track label + Track edit + Sector label + Sector edit
    x = kMargin;
    y += kRowHeight + kRowGap;

    for (i = 0; i < 3; i++)
    {
        MoveWindow (m_driveRadio[i], x, y, kRadioWidth, kRowHeight, TRUE);
        x += kRadioWidth;
    }

    x += kRowGap;
    MoveWindow (m_trackRawQtCheck, x, y, kCheckWidth, kRowHeight, TRUE);
    x += kCheckWidth + kRowGap;

    MoveWindow (m_trackRichEdit, x, y, kEditWidth, kRowHeight, TRUE);
    x += kEditWidth + kRowGap;

    MoveWindow (m_sectorRichEdit, x, y, kEditWidth, kRowHeight, TRUE);

    // Row 4: Ignored labels (track + sector side by side)
    y += kRowHeight + kRowGap;

    MoveWindow (m_trackIgnoredLabel,
                kMargin,
                y,
                width / 2 - kMargin,
                kIgnoredLabelHeight,
                TRUE);
    MoveWindow (m_sectorIgnoredLabel,
                width / 2,
                y,
                width / 2 - kMargin,
                kIgnoredLabelHeight,
                TRUE);

    // Row 5: Pause + Clear
    y += kIgnoredLabelHeight + kRowGap;

    MoveWindow (m_pauseButton, kMargin, y, kButtonWidth, kButtonHeight, TRUE);
    MoveWindow (m_clearButton, kMargin + kButtonWidth + kRowGap, y, kButtonWidth, kButtonHeight, TRUE);

    listViewTop = y + kButtonHeight + kRowGap;

    if (height > listViewTop + kMargin)
    {
        MoveWindow (m_listView,
                    kMargin,
                    listViewTop,
                    width - 2 * kMargin,
                    height - listViewTop - kMargin,
                    TRUE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildListViewColumns
//
//  Recreate the ListView's columns from the logical column model.
//  Hidden columns are absent from the LV entirely (no zero-width).
//  Each newly-shown column gets an FR-027 auto-size-to-header pass
//  on its first appearance.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::RebuildListViewColumns ()
{
    int  virtualIdx = 0;
    int  i          = 0;

    if (m_listView == nullptr)
    {
        return;
    }

    while (ListView_DeleteColumn (m_listView, 0))
    {
        // loop until empty
    }

    for (i = 0; i < kColumnCount; i++)
    {
        m_visibleOrdinalToLogicalId[i] = -1;
    }

    for (i = 0; i < kColumnCount; i++)
    {
        LVCOLUMNW  lvc = {};

        if (!m_columns[i].visible)
        {
            continue;
        }

        lvc.mask    = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText = const_cast<LPWSTR> (m_columns[i].headerText);
        lvc.cx      = m_columns[i].savedWidth;

        ListView_InsertColumn (m_listView, virtualIdx, &lvc);

        if (!m_columns[i].autoSizedYet)
        {
            ListView_SetColumnWidth (m_listView, virtualIdx, LVSCW_AUTOSIZE_USEHEADER);
            m_columns[i].savedWidth   = ListView_GetColumnWidth (m_listView, virtualIdx);
            m_columns[i].autoSizedYet = true;
        }

        m_visibleOrdinalToLogicalId[virtualIdx] = m_columns[i].id;
        virtualIdx++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSize
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugDialog::OnSize (HWND hwnd, UINT width, UINT height)
{
    UNREFERENCED_PARAMETER (hwnd);

    LayoutChildControls (static_cast<int> (width), static_cast<int> (height));
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnClose
//
//  WM_CLOSE hides the dialog. The dialog is reused across opens; only
//  shell shutdown calls Destroy() to actually tear it down.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugDialog::OnClose (HWND hwnd)
{
    UNREFERENCED_PARAMETER (hwnd);

    Hide ();
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDestroy
//
//  Cancel any timers, drop the HWND. Override the base Window
//  PostQuitMessage default so closing the debug dialog does not
//  terminate the host application.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugDialog::OnDestroy (HWND hwnd)
{
    if (m_drainTimerActive)
    {
        KillTimer (hwnd, m_drainTimerId);
        m_drainTimerActive = false;
    }

    if (m_filterDebouncePending)
    {
        KillTimer (hwnd, m_filterDebounceTimerId);
        m_filterDebouncePending = false;
    }

    m_hwnd = nullptr;
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCommand
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
//  OnCommandEx
//
//  Filter checkboxes / radios fire BN_CLICKED. Track/Sector RichEdits
//  fire EN_CHANGE on every keystroke and EN_KILLFOCUS when focus
//  moves; the former arms the debounce timer, the latter flushes
//  immediately per FR-014d.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugDialog::OnCommandEx (HWND hwnd, int id, int notifyCode, HWND hCtl)
{
    UNREFERENCED_PARAMETER (hwnd);

    if (id == kIdBtnPause || id == kIdBtnClear)
    {
        // Phase 8 wires Pause / Clear.
        return false;
    }

    if (id == kIdEdtTrack || id == kIdEdtSector)
    {
        if (notifyCode == EN_CHANGE)
        {
            OnFilterTextChanged ();
        }
        else if (notifyCode == EN_KILLFOCUS)
        {
            OnFilterTextKillFocus ();
        }

        return false;
    }

    if (notifyCode == BN_CLICKED)
    {
        OnFilterControlToggled (id, hCtl);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnFilterControlToggled
//
//  Read the new check / select state straight off the control handle
//  rather than tracking a parallel bool per checkbox -- the Win32
//  control is the source of truth and BST_CHECKED is one IPC.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::OnFilterControlToggled (int id, HWND hCtl)
{
    bool      checked   = false;
    uint32_t  catBit    = 0;
    int       slot      = 0;

    if (hCtl == nullptr)
    {
        return;
    }

    checked = SendMessageW (hCtl, BM_GETCHECK, 0, 0) == BST_CHECKED;

    if (id >= kIdChkEventTypeFirst && id < kIdChkEventTypeFirst + 8)
    {
        slot   = id - kIdChkEventTypeFirst;
        catBit = 1u << slot;

        if (checked)
        {
            m_filter.eventTypeMask |= catBit;
        }
        else
        {
            m_filter.eventTypeMask &= ~catBit;
        }
    }
    else if (id == kIdChkAudioMaster)
    {
        m_filter.audioMaster = checked;
        UpdateAudioSubEnableState ();
    }
    else if (id >= kIdChkAudioSubFirst && id < kIdChkAudioSubFirst + 4)
    {
        slot = id - kIdChkAudioSubFirst;

        switch (slot)
        {
            case 0: m_filter.audioStarted   = checked; break;
            case 1: m_filter.audioRestarted = checked; break;
            case 2: m_filter.audioContinued = checked; break;
            case 3: m_filter.audioSilent    = checked; break;
            default: break;
        }
    }
    else if (id >= kIdRdoDriveFirst && id < kIdRdoDriveFirst + 3)
    {
        if (checked)
        {
            m_filter.driveFilter = id - kIdRdoDriveFirst;
        }
    }
    else if (id == kIdChkTrackRawQt)
    {
        m_filter.trackFilterRawQt = checked;
        // raw-qt re-interprets bare integers as quarter tracks so the
        // track predicate has to be re-parsed against the new flag.
        FlushFilterDebounce ();
        return;
    }
    else
    {
        return;
    }

    RebuildFilteredIndices ();
    InvalidateListView ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateAudioSubEnableState
//
//  FR-014c: when the Audio master is unchecked the four sub-checkboxes
//  grey out but keep their checked state for restoration.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::UpdateAudioSubEnableState ()
{
    int  i = 0;

    for (i = 0; i < 4; i++)
    {
        EnableWindow (m_audioSubCheck[i], m_filter.audioMaster ? TRUE : FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnFilterTextChanged
//
//  FR-014d: arm a one-shot 250 ms timer; subsequent keystrokes cancel
//  and re-arm so we only re-parse / re-project after the user pauses
//  typing.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::OnFilterTextChanged ()
{
    if (m_hwnd == nullptr)
    {
        return;
    }

    if (m_filterDebouncePending)
    {
        KillTimer (m_hwnd, m_filterDebounceTimerId);
    }

    SetTimer (m_hwnd, m_filterDebounceTimerId, kFilterTextDebounceMs, nullptr);
    m_filterDebouncePending = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnFilterTextKillFocus
//
//  EN_KILLFOCUS bypasses the debounce wait and flushes immediately.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::OnFilterTextKillFocus ()
{
    if (m_hwnd != nullptr && m_filterDebouncePending)
    {
        KillTimer (m_hwnd, m_filterDebounceTimerId);
        m_filterDebouncePending = false;
    }

    FlushFilterDebounce ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadEditText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskIIDebugDialog::ReadEditText (HWND hEdit) const
{
    int           len = 0;
    std::wstring  out;

    if (hEdit == nullptr)
    {
        return out;
    }

    len = GetWindowTextLengthW (hEdit);

    if (len <= 0)
    {
        return out;
    }

    out.resize (static_cast<size_t> (len));
    GetWindowTextW (hEdit, out.data (), len + 1);
    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushFilterDebounce
//
//  Re-parse Track and Sector inputs, refresh m_filter, rebuild the
//  filtered-index vector, repaint the ListView, and update the
//  FR-014e squiggle + ignored-tokens label on each input.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::FlushFilterDebounce ()
{
    std::wstring  trackText;
    std::wstring  sectorText;

    m_filterDebouncePending = false;

    trackText  = ReadEditText (m_trackRichEdit);
    sectorText = ReadEditText (m_sectorRichEdit);

    m_filter.trackFilter  = TrackSectorPredicate::Parse (trackText,  m_filter.trackFilterRawQt);
    m_filter.sectorFilter = TrackSectorPredicate::Parse (sectorText, false);

    RebuildFilteredIndices ();
    InvalidateListView ();

    ApplyRejectedTokenSquiggles (m_trackRichEdit,   m_filter.trackFilter.RejectedSpans  ());
    SetIgnoredTokensLabel       (m_trackIgnoredLabel,  trackText,  m_filter.trackFilter.RejectedSpans  ());

    ApplyRejectedTokenSquiggles (m_sectorRichEdit,  m_filter.sectorFilter.RejectedSpans ());
    SetIgnoredTokensLabel       (m_sectorIgnoredLabel, sectorText, m_filter.sectorFilter.RejectedSpans ());
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFilteredIndices
//
//  Full O(deque) rebuild. Cheap relative to the user's typing cadence
//  and the only correct behavior when the filter state changes mid-
//  stream (re-checking a filter MUST reveal events from the off
//  window in chronological order per User Story 3).
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::RebuildFilteredIndices ()
{
    size_t  i = 0;

    m_filteredIndices.clear ();

    for (i = 0; i < m_deque.size (); i++)
    {
        if (MatchesFilter (m_deque[i], m_filter))
        {
            m_filteredIndices.push_back (static_cast<uint32_t> (i));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InvalidateListView
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::InvalidateListView ()
{
    if (m_listView == nullptr)
    {
        return;
    }

    ListView_SetItemCountEx (m_listView,
                             static_cast<int> (m_filteredIndices.size ()),
                             LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    InvalidateRect (m_listView, nullptr, FALSE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnTimer
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugDialog::OnTimer (HWND hwnd, UINT_PTR timerId)
{
    UNREFERENCED_PARAMETER (hwnd);

    if (timerId == m_drainTimerId)
    {
        HandleDrainTick ();
        return false;
    }

    if (timerId == m_filterDebounceTimerId)
    {
        if (m_filterDebouncePending)
        {
            KillTimer (m_hwnd, m_filterDebounceTimerId);
            m_filterDebouncePending = false;
        }

        FlushFilterDebounce ();
        return false;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleDrainTick
//
//  Implements plan.md "Auto-Tail Scroll Algorithm". Per FR-011 + Q2,
//  the drain runs every tick regardless of m_paused and window
//  visibility -- the deque's 100k cap bounds memory while the dialog
//  is hidden or paused. Only the visible ListView refresh is skipped
//  when the window is hidden / minimized / paused.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::HandleDrainTick ()
{
    int       topIndex     = 0;
    int       countPerPage = 0;
    int       oldCount     = 0;
    int       newCount     = 0;
    bool      wasAtTail    = false;
    bool      shouldPaint  = false;
    uint32_t  dropped      = 0;
    size_t    preDequeSize = 0;
    size_t    postDequeSize = 0;
    bool      hitCap       = false;

    if (m_listView == nullptr)
    {
        return;
    }

    oldCount     = static_cast<int> (m_filteredIndices.size ());
    topIndex     = ListView_GetTopIndex     (m_listView);
    countPerPage = ListView_GetCountPerPage (m_listView);
    wasAtTail    = ComputeWasAtTail (topIndex, countPerPage, oldCount);

    preDequeSize = m_deque.size ();
    dropped      = m_droppedSinceLastDrain.exchange (0, std::memory_order_acq_rel);

    DebugDialogProjection::DrainAndProject (m_ring, m_deque, dropped, m_uptimeAnchor);

    postDequeSize = m_deque.size ();

    // pop_front happens only when the rolling cap is hit. When either
    // the pre- or post-drain deque sized at the cap, indices may be
    // stale by some unknown shift; rebuild from scratch. Otherwise
    // run the cheap incremental append for slots [pre..post).
    hitCap = preDequeSize  >= DebugDialogProjection::kDisplayDequeCap
          || postDequeSize >= DebugDialogProjection::kDisplayDequeCap;

    if (hitCap)
    {
        RebuildFilteredIndices ();
    }
    else
    {
        AppendFilteredIndicesFor (preDequeSize);
    }

    newCount = static_cast<int> (m_filteredIndices.size ());

    if (newCount == oldCount)
    {
        return;
    }

    shouldPaint = IsWindowVisible (m_hwnd) && !IsIconic (m_hwnd) && !m_paused;

    if (!shouldPaint)
    {
        return;
    }

    ListView_SetItemCountEx (m_listView, newCount, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

    if (wasAtTail && newCount > 0)
    {
        ListView_EnsureVisible (m_listView, newCount - 1, FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendFilteredIndicesFor
//
//  Phase 7: extend m_filteredIndices with deque slots from startIdx
//  that pass MatchesFilter. The drain-tick call site hands us the
//  pre-drain filtered-indices count -- but startIdx here is computed
//  by callers as "the deque position at which the next match should
//  begin from", which is the deque size we last fully scanned.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::AppendFilteredIndicesFor (size_t startIdx)
{
    size_t  i = 0;

    // m_deque may have shrunk (rolling-cap pop_front). Resync from
    // scratch when that happens so the indices never dangle.
    if (m_filteredIndices.size () > m_deque.size ())
    {
        RebuildFilteredIndices ();
        return;
    }

    if (startIdx > m_deque.size ())
    {
        startIdx = m_deque.size ();
    }

    for (i = startIdx; i < m_deque.size (); i++)
    {
        if (MatchesFilter (m_deque[i], m_filter))
        {
            m_filteredIndices.push_back (static_cast<uint32_t> (i));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNotify
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugDialog::OnNotify (HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    NMHDR  *  pHdr  = reinterpret_cast<NMHDR *> (lParam);

    UNREFERENCED_PARAMETER (hwnd);
    UNREFERENCED_PARAMETER (wParam);

    if (pHdr == nullptr)
    {
        return false;
    }

    if (pHdr->idFrom == kIdListView && pHdr->code == LVN_GETDISPINFOW)
    {
        HandleGetDispInfo (reinterpret_cast<NMLVDISPINFOW *> (lParam));
        return false;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleGetDispInfo
//
//  Virtual-mode ListView row fetch. The control passes iItem (the
//  visible-row index into m_filteredIndices) and iSubItem (the
//  visible-subset ordinal); we translate both back to the source
//  deque entry and the logical column id before picking a string.
//
//  pszText must remain valid only until the next message. Wall /
//  Uptime / Cycle / Detail strings are stored on the deque entry so
//  they are stable for the message duration. Event uses a thread-
//  local scratch buffer copied from the wstring_view returned by
//  DebugDialogProjection::EventLabel.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::HandleGetDispInfo (NMLVDISPINFOW * pInfo)
{
    thread_local wchar_t  s_eventLabelBuf[32] = {};

    LVITEMW           &   item            = pInfo->item;
    uint32_t              deqIdx          = 0;
    int                   logicalId       = 0;
    int                   visibleOrdinal  = 0;
    std::wstring_view     label;
    size_t                copyLen         = 0;

    if ((item.mask & LVIF_TEXT) == 0)
    {
        return;
    }

    if (item.iItem < 0 || static_cast<size_t> (item.iItem) >= m_filteredIndices.size ())
    {
        return;
    }

    deqIdx = m_filteredIndices[item.iItem];

    if (deqIdx >= m_deque.size ())
    {
        return;
    }

    visibleOrdinal = item.iSubItem;

    if (visibleOrdinal < 0 || visibleOrdinal >= kColumnCount)
    {
        return;
    }

    logicalId = m_visibleOrdinalToLogicalId[visibleOrdinal];

    if (logicalId < 0 || logicalId >= kColumnCount)
    {
        return;
    }

    const DiskIIEventDisplay &  e = m_deque[deqIdx];

    switch (logicalId)
    {
        case 0:
            item.pszText = const_cast<LPWSTR> (e.wallStr.data ());
            break;

        case 1:
            item.pszText = const_cast<LPWSTR> (e.uptimeStr.data ());
            break;

        case 2:
            item.pszText = const_cast<LPWSTR> (e.cycleStr.data ());
            break;

        case 3:
            label   = DebugDialogProjection::EventLabel (e.category, e.type);
            copyLen = std::min<size_t> (label.size (),
                                        (sizeof (s_eventLabelBuf) / sizeof (s_eventLabelBuf[0])) - 1);
            std::copy_n (label.data (), copyLen, s_eventLabelBuf);
            s_eventLabelBuf[copyLen] = L'\0';
            item.pszText = s_eventLabelBuf;
            break;

        case 4:
            item.pszText = const_cast<LPWSTR> (e.detail.c_str ());
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishToRing
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PublishToRing (const DiskIIEvent & e) noexcept
{
    if (!m_ring.TryPush (e))
    {
        m_droppedSinceLastDrain.fetch_add (1, std::memory_order_relaxed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushControllerEvent
//
//  Helper for the simple controller-side events whose only payload is
//  the event type itself (motor strobes / engagement edges).
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushControllerEvent (DiskIIEventType type) noexcept
{
    DiskIIEvent  e = {};

    e.category = EventCategory::Controller;
    e.type     = type;
    e.cycle    = 0;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushHeadStepEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushHeadStepEvent (int prevQt, int newQt) noexcept
{
    DiskIIEvent  e = {};

    e.category               = EventCategory::Controller;
    e.type                   = DiskIIEventType::HeadStep;
    e.payload.step.prevQt    = prevQt;
    e.payload.step.newQt     = newQt;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushHeadBumpEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushHeadBumpEvent (int atQt) noexcept
{
    DiskIIEvent  e = {};

    e.category           = EventCategory::Controller;
    e.type               = DiskIIEventType::HeadBump;
    e.payload.bump.atQt  = atQt;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushAddrMarkEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushAddrMarkEvent (int track, int sector, int volume) noexcept
{
    DiskIIEvent  e = {};

    e.category                   = EventCategory::Controller;
    e.type                       = DiskIIEventType::AddrMark;
    e.payload.addrMark.track     = track;
    e.payload.addrMark.sector    = sector;
    e.payload.addrMark.volume    = volume;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushDataMarkEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushDataMarkEvent (DiskIIEventType type, int sector, int byteCount) noexcept
{
    DiskIIEvent  e = {};

    e.category                    = EventCategory::Controller;
    e.type                        = type;
    e.payload.dataMark.sector     = sector;
    e.payload.dataMark.byteCount  = byteCount;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushDriveEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushDriveEvent (DiskIIEventType type, int drive) noexcept
{
    DiskIIEvent  e = {};

    e.category             = EventCategory::Controller;
    e.type                 = type;
    e.payload.drive.drive  = drive;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushAudioEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushAudioEvent (
    DiskIIEventType  type,
    SoundKind        kind,
    int              drive,
    SilentReason     reason) noexcept
{
    DiskIIEvent  e = {};

    e.category              = EventCategory::Audio;
    e.type                  = type;
    e.payload.audio.kind    = kind;
    e.payload.audio.reason  = reason;
    e.payload.audio.drive   = drive;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskIIEventSink overrides
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::OnMotorCommandOn   () { PushControllerEvent (DiskIIEventType::MotorCommandOn);   }
void DiskIIDebugDialog::OnMotorEngaged     () { PushControllerEvent (DiskIIEventType::MotorEngaged);     }
void DiskIIDebugDialog::OnMotorCommandOff  () { PushControllerEvent (DiskIIEventType::MotorCommandOff);  }
void DiskIIDebugDialog::OnMotorDisengaged  () { PushControllerEvent (DiskIIEventType::MotorDisengaged);  }

void DiskIIDebugDialog::OnHeadStep         (int prevQt, int newQt)              { PushHeadStepEvent (prevQt, newQt); }
void DiskIIDebugDialog::OnHeadBump         (int atQt)                           { PushHeadBumpEvent (atQt); }
void DiskIIDebugDialog::OnAddressMark      (int track, int sector, int volume)  { PushAddrMarkEvent (track, sector, volume); }
void DiskIIDebugDialog::OnDataMarkRead     (int sector, int byteCount)          { PushDataMarkEvent (DiskIIEventType::DataRead,  sector, byteCount); }
void DiskIIDebugDialog::OnDataMarkWrite    (int sector, int byteCount)          { PushDataMarkEvent (DiskIIEventType::DataWrite, sector, byteCount); }
void DiskIIDebugDialog::OnDriveSelect      (int drive)                          { PushDriveEvent    (DiskIIEventType::DriveSelect,  drive); }
void DiskIIDebugDialog::OnDiskInserted     (int drive)                          { PushDriveEvent    (DiskIIEventType::DiskInserted, drive); }
void DiskIIDebugDialog::OnDiskEjected      (int drive)                          { PushDriveEvent    (DiskIIEventType::DiskEjected,  drive); }





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveAudioEventSink overrides
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::OnAudioStarted     (SoundKind kind, int drive)                    { PushAudioEvent (DiskIIEventType::AudioStarted,     kind, drive, SilentReason::DriveAudioDisabled); }
void DiskIIDebugDialog::OnAudioRestarted   (SoundKind kind, int drive)                    { PushAudioEvent (DiskIIEventType::AudioRestarted,   kind, drive, SilentReason::DriveAudioDisabled); }
void DiskIIDebugDialog::OnAudioContinued   (SoundKind kind, int drive)                    { PushAudioEvent (DiskIIEventType::AudioContinued,   kind, drive, SilentReason::DriveAudioDisabled); }
void DiskIIDebugDialog::OnAudioSilent      (SoundKind kind, int drive, SilentReason reason) { PushAudioEvent (DiskIIEventType::AudioSilent,    kind, drive, reason); }
void DiskIIDebugDialog::OnAudioLoopStarted (SoundKind kind, int drive)                    { PushAudioEvent (DiskIIEventType::AudioLoopStarted, kind, drive, SilentReason::DriveAudioDisabled); }
void DiskIIDebugDialog::OnAudioLoopStopped (SoundKind kind, int drive)                    { PushAudioEvent (DiskIIEventType::AudioLoopStopped, kind, drive, SilentReason::DriveAudioDisabled); }
