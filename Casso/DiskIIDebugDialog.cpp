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
static constexpr int  kRawQtCheckWidth      = 150;
static constexpr int  kAudioCheckWidth      = 86;
static constexpr int  kRadioWidth           = 60;
static constexpr int  kEditWidth            = 140;
static constexpr int  kFilterLabelWidth     = 78;
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
    kIdLblTrackInvalid    = 142,
    kIdLblSectorInvalid   = 143,
    kIdChkTrackRawQt      = 144,

    kIdBtnPause           = 150,
    kIdBtnClear           = 151,

    // Header right-click popup column-toggle items. Range 160..164
    // maps directly to LogicalColumn id 0..4 (Wall, Uptime, Cycle,
    // Event, Detail) per DiskIIDebugDialogState.
    kIdColumnToggleFirst  = 160,
    kIdColumnToggleLast   = 165,
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

static void EnsureMsftEditLoaded()
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

DiskIIDebugDialog::DiskIIDebugDialog()
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

DiskIIDebugDialog::~DiskIIDebugDialog()
{
    Destroy();
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

    EnsureMsftEditLoaded();

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

void DiskIIDebugDialog::Show()
{
    HRESULT hr = S_OK;

    CBR (m_hwnd != nullptr);

    ShowWindow (m_hwnd, SW_SHOW);
    SetForegroundWindow (m_hwnd);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::Hide()
{
    HRESULT hr = S_OK;

    CBR (m_hwnd != nullptr);

    ShowWindow (m_hwnd, SW_HIDE);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Destroy
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::Destroy()
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

HFONT DiskIIDebugDialog::AcquireUiFont()
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

    RebuildListViewColumns();

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

    font = AcquireUiFont();

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
                                         L"Quarter-track steps",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_AUTOCHECKBOX,
                                         0, 0, 0, 0,
                                         hwnd,
                                         reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdChkTrackRawQt)),
                                         m_hInstance,
                                         nullptr);
    CWRA (m_trackRawQtCheck);
    SendMessageW (m_trackRawQtCheck, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);

    m_trackFilterLabel = CreateWindowExW (0,
                                          L"STATIC",
                                          L"Track filter:",
                                          WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                          0, 0, 0, 0,
                                          hwnd,
                                          nullptr,
                                          m_hInstance,
                                          nullptr);
    CWRA (m_trackFilterLabel);
    SendMessageW (m_trackFilterLabel, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);

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

    m_sectorFilterLabel = CreateWindowExW (0,
                                           L"STATIC",
                                           L"Sector filter:",
                                           WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                           0, 0, 0, 0,
                                           hwnd,
                                           nullptr,
                                           m_hInstance,
                                           nullptr);
    CWRA (m_sectorFilterLabel);
    SendMessageW (m_sectorFilterLabel, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);

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

    m_trackInvalidLabel = CreateWindowExW (0,
                                           L"STATIC",
                                           L"",
                                           WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                                           0, 0, 0, 0,
                                           hwnd,
                                           reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdLblTrackInvalid)),
                                           m_hInstance,
                                           nullptr);
    CWRA (m_trackInvalidLabel);
    SendMessageW (m_trackInvalidLabel, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);

    m_sectorInvalidLabel = CreateWindowExW (0,
                                            L"STATIC",
                                            L"",
                                            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                                            0, 0, 0, 0,
                                            hwnd,
                                            reinterpret_cast<HMENU> (static_cast<INT_PTR> (kIdLblSectorInvalid)),
                                            m_hInstance,
                                            nullptr);
    CWRA (m_sectorInvalidLabel);
    SendMessageW (m_sectorInvalidLabel, WM_SETFONT, reinterpret_cast<WPARAM> (font), TRUE);

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

    hr = InstallListViewSubclass();
    CHR (hr);

    // Spec-006 bug-fix. Inline tooltip control documenting the
    // Track / Sector filter syntax (comma-separated integers, ranges,
    // and -- for Track only -- decimals interpreted as quarter-track
    // positions when "Quarter-track steps" is unchecked).
    m_filterTooltip = CreateWindowExW (WS_EX_TOPMOST,
                                       TOOLTIPS_CLASS,
                                       nullptr,
                                       WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       hwnd,
                                       nullptr,
                                       m_hInstance,
                                       nullptr);

    if (m_filterTooltip != nullptr)
    {
        TOOLINFOW  ti = {};

        SetWindowPos (m_filterTooltip, HWND_TOPMOST, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        ti.cbSize   = sizeof (ti);
        ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd     = hwnd;
        ti.uId      = reinterpret_cast<UINT_PTR> (m_trackRichEdit);
        ti.lpszText = const_cast<LPWSTR>
            (L"Track filter syntax:\r\n"
             L"  \u2022  Whole tracks: 0 \u2013 39\r\n"
             L"  \u2022  Quarter tracks: 0.0, 0.25, 0.5, 0.75, \u2026 up to 39.75\r\n"
             L"  \u2022  Ranges: 17-22, 5.0-5.75\r\n"
             L"  \u2022  Lists: 0, 5, 17-22, 34.5\r\n"
             L"  \u2022  Empty matches all tracks\r\n"
             L"\r\n"
             L"When 'Quarter-track steps' is checked, bare integers are "
             L"interpreted as quarter-track indices (0 \u2013 159) instead "
             L"of whole tracks.\r\n"
             L"\r\n"
             L"Out-of-range or unparseable tokens get a red squiggle and "
             L"are listed in the red invalid-token label below.");
        SendMessageW (m_filterTooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM> (&ti));

        ti.uId      = reinterpret_cast<UINT_PTR> (m_sectorRichEdit);
        ti.lpszText = const_cast<LPWSTR>
            (L"Sector filter syntax:\r\n"
             L"  \u2022  Sectors: 0 \u2013 15 (whole numbers only)\r\n"
             L"  \u2022  Ranges: 0-15, 8-12\r\n"
             L"  \u2022  Lists: 0, 1, 8-15\r\n"
             L"  \u2022  Empty matches all sectors\r\n"
             L"\r\n"
             L"Stock DOS 3.3 uses sectors 0 \u2013 15. Out-of-range or "
             L"unparseable tokens get a red squiggle and are listed in "
             L"the red invalid-token label below.");
        SendMessageW (m_filterTooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM> (&ti));

        SendMessageW (m_filterTooltip, TTM_SETMAXTIPWIDTH, 0, 360);
        SendMessageW (m_filterTooltip, TTM_ACTIVATE, TRUE, 0);
    }

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
    int  x           = 0;
    int  y           = 0;
    int  i           = 0;
    int  listViewTop = 0;
    int  trackEditX  = 0;
    int  sectorEditX = 0;

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

    // Row 3: Drive radio + Track label + Track edit + Sector label + Sector edit
    x = kMargin;
    y += kRowHeight + kRowGap;

    for (i = 0; i < 3; i++)
    {
        MoveWindow (m_driveRadio[i], x, y, kRadioWidth, kRowHeight, TRUE);
        x += kRadioWidth;
    }

    x += kRowGap;

    MoveWindow (m_trackFilterLabel, x, y + 3, kFilterLabelWidth, kRowHeight, TRUE);
    x += kFilterLabelWidth + kRowGap;

    // Spec-006 bug-fix. The Quarter-track steps checkbox modifies how
    // bare integers in the track edit are interpreted, so it lives in
    // row 4 directly under the track edit (trackEditX is captured here
    // so row 4 and the row-5 invalid label can align to the same x).
    trackEditX = x;

    MoveWindow (m_trackRichEdit, x, y, kEditWidth, kRowHeight, TRUE);
    x += kEditWidth + kRowGap;

    MoveWindow (m_sectorFilterLabel, x, y + 3, kFilterLabelWidth, kRowHeight, TRUE);
    x += kFilterLabelWidth + kRowGap;

    sectorEditX = x;

    MoveWindow (m_sectorRichEdit, x, y, kEditWidth, kRowHeight, TRUE);

    // Row 4: Quarter-track steps checkbox aligned under the track edit.
    y += kRowHeight + kRowGap;
    MoveWindow (m_trackRawQtCheck, trackEditX, y, kRawQtCheckWidth, kRowHeight, TRUE);

    // Row 5: per-side invalid labels (red text, SS_ENDELLIPSIS so over-
    // long token lists truncate rather than disappear off-edge). Track
    // label hangs under the track edit, sector label under the sector
    // edit. Each consumes the width of its own edit-column extending
    // out to the right margin / next column boundary.
    y += kRowHeight + kRowGap;

    MoveWindow (m_trackInvalidLabel,
                trackEditX,
                y,
                sectorEditX - trackEditX - kRowGap,
                kIgnoredLabelHeight,
                TRUE);

    MoveWindow (m_sectorInvalidLabel,
                sectorEditX,
                y,
                width - sectorEditX - kMargin,
                kIgnoredLabelHeight,
                TRUE);

    // Row 6: Pause + Clear
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

void DiskIIDebugDialog::RebuildListViewColumns()
{
    int  virtualIdx   = 0;
    int  i            = 0;
    int  contentWidth = 0;

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

        m_visibleOrdinalToLogicalId[virtualIdx] = m_columns[i].id;

        // Spec-006 bug-fix. Auto-size to MAX (header, widest current
        // cell) on first show; the previous header-only auto-size
        // truncated multi-digit Cycle / wide Detail strings to the
        // header's width. The Detail column is special-cased: it
        // always flexes to fill the LV client remainder so the user
        // never sees a Detail column that's narrower than its data
        // AND ends with empty space to its right.
        if (m_columns[i].id != kDetailColumnId && !m_columns[i].autoSizedYet)
        {
            contentWidth = MeasureColumnContentWidth (m_columns[i].id, 0);
            ListView_SetColumnWidth (m_listView, virtualIdx, contentWidth);
            m_columns[i].savedWidth   = contentWidth;
            m_columns[i].autoSizedYet = true;
        }

        virtualIdx++;
    }

    SizeDetailColumnToRemainder();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureColumnContentWidth
//
//  Walk the dialog's deque starting at startIdx, project each row's
//  cell text through AppendColumnText into a scratch buffer, and ask
//  the ListView for the pixel width via LVM_GETSTRINGWIDTH (which uses
//  the LV's font). Returns max (header text width, max cell width in
//  the [startIdx, end) chunk) + padding.
//
//  The auto-grow caller in HandleDrainTick passes startIdx =
//  m_dequeSizeAtLastGrow so each periodic check only measures rows
//  added since the last grow -- the "never shrink" invariant means we
//  can compare the new chunk's max width against the current
//  savedWidth and grow if larger without re-walking history. The first
//  auto-fit pass passes startIdx = 0 to measure the whole deque.
//
////////////////////////////////////////////////////////////////////////////////

int DiskIIDebugDialog::MeasureColumnContentWidth (int logicalId, size_t startIdx) const
{
    constexpr int     kAutoSizePaddingPx = 16;
    constexpr int     kMinColumnWidth    = 32;

    int               headerWidth        = 0;
    int               maxCellWidth       = 0;
    int               cellWidth          = 0;
    size_t            i                  = 0;
    std::wstring      scratch;

    if (m_listView == nullptr || logicalId < 0 || logicalId >= kColumnCount)
    {
        return kMinColumnWidth;
    }

    headerWidth = ListView_GetStringWidth (m_listView, m_columns[logicalId].headerText);

    for (i = startIdx; i < m_deque.size(); i++)
    {
        scratch.clear();
        AppendColumnText (scratch, m_deque[i], logicalId);

        if (!scratch.empty())
        {
            cellWidth = ListView_GetStringWidth (m_listView, scratch.c_str());

            if (cellWidth > maxCellWidth)
            {
                maxCellWidth = cellWidth;
            }
        }
    }

    return std::max (headerWidth, maxCellWidth) + kAutoSizePaddingPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SizeDetailColumnToRemainder
//
//  Spec-006 bug-fix. After every other visible column has its width
//  set, give the Detail column whatever's left of the LV client area
//  (clamped to a sensible minimum). Called from
//  RebuildListViewColumns AND from OnSize so user resize flexes the
//  Detail column rather than leaving the trailing whitespace dead.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::SizeDetailColumnToRemainder()
{
    constexpr int  kDetailMinWidth    = 200;

    RECT           rc                 = {};
    int            clientWidth        = 0;
    int            usedWidth          = 0;
    int            detailVirtualIdx   = -1;
    int            virtualIdx         = 0;
    int            i                  = 0;

    if (m_listView == nullptr)
    {
        return;
    }

    if (!m_columns[kDetailColumnId].visible)
    {
        return;
    }

    if (!GetClientRect (m_listView, &rc))
    {
        return;
    }

    clientWidth = rc.right - rc.left;

    for (i = 0; i < kColumnCount; i++)
    {
        if (!m_columns[i].visible)
        {
            continue;
        }

        if (m_columns[i].id == kDetailColumnId)
        {
            detailVirtualIdx = virtualIdx;
        }
        else
        {
            usedWidth += ListView_GetColumnWidth (m_listView, virtualIdx);
        }

        virtualIdx++;
    }

    if (detailVirtualIdx < 0)
    {
        return;
    }

    int  remainder = clientWidth - usedWidth;

    if (remainder < kDetailMinWidth)
    {
        remainder = kDetailMinWidth;
    }

    ListView_SetColumnWidth (m_listView, detailVirtualIdx, remainder);
    m_columns[kDetailColumnId].savedWidth = remainder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleColumn
//
//  Flip one LogicalColumn's visible bit and rebuild the LV's column
//  set. The pre-flip CaptureCurrentWidthsIntoModel() preserves any
//  user-dragged width that hasn't been written back yet (HDN_ENDTRACK
//  catches drag-end but Win32 doesn't fire it for programmatic
//  changes). Hiding all five columns is allowed -- the LV draws a
//  blank canvas and the user can re-show via the same popup.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::ToggleColumn (int id)
{
    if (id < 0 || id >= kColumnCount)
    {
        return;
    }

    CaptureCurrentWidthsIntoModel();

    m_columns[id].visible = !m_columns[id].visible;

    RebuildListViewColumns();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureCurrentWidthsIntoModel
//
//  Walk the ListView's currently-visible columns and copy each one's
//  width back into the matching LogicalColumn::savedWidth via the
//  m_visibleOrdinalToLogicalId map. Called before any rebuild that
//  could lose user-dragged widths, and from the HDN_ENDTRACK notify
//  when the user finishes a drag.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::CaptureCurrentWidthsIntoModel()
{
    int  width     = 0;
    int  logicalId = 0;
    int  i         = 0;

    if (m_listView == nullptr)
    {
        return;
    }

    for (i = 0; i < kColumnCount; i++)
    {
        logicalId = m_visibleOrdinalToLogicalId[i];

        if (logicalId < 0 || logicalId >= kColumnCount)
        {
            continue;
        }

        width = ListView_GetColumnWidth (m_listView, i);

        if (width > 0)
        {
            m_columns[logicalId].savedWidth = width;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowHeaderContextMenu
//
//  Build and display the FR-026 column-visibility popup. Each item
//  is keyed by IDM = kIdColumnToggleFirst + LogicalColumn.id, with
//  MFS_CHECKED tracking m_columns[id].visible. TPM_RETURNCMD lets us
//  consume the user's selection inline instead of routing through
//  the dialog's WM_COMMAND -- the popup is owned-this-call only and
//  doesn't need to coexist with kIdBtnPause / kIdBtnClear dispatch.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::ShowHeaderContextMenu (int x, int y)
{
    HMENU  hMenu     = nullptr;
    BOOL   appended  = FALSE;
    int    cmdResult = 0;
    int    chosenId  = 0;
    int    i         = 0;
    UINT   flags     = 0;

    if (m_hwnd == nullptr)
    {
        return;
    }

    hMenu = CreatePopupMenu();

    if (hMenu == nullptr)
    {
        return;
    }

    for (i = 0; i < kColumnCount; i++)
    {
        flags    = MF_STRING | (m_columns[i].visible ? MF_CHECKED : MF_UNCHECKED);
        appended = AppendMenuW (hMenu,
                                flags,
                                static_cast<UINT_PTR> (kIdColumnToggleFirst + i),
                                m_columns[i].headerText);

        if (!appended)
        {
            DestroyMenu (hMenu);
            return;
        }
    }

    cmdResult = TrackPopupMenu (hMenu,
                                TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                x, y, 0,
                                m_hwnd, nullptr);

    DestroyMenu (hMenu);

    if (cmdResult >= kIdColumnToggleFirst && cmdResult <= kIdColumnToggleLast)
    {
        chosenId = cmdResult - kIdColumnToggleFirst;
        ToggleColumn (chosenId);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMultiControllerHint
//
//  FR-017. Append " (controller #0 only)" when the active machine
//  config has more than one Disk II controller, otherwise restore the
//  base title. Safe to call before or after Create().
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::SetMultiControllerHint (bool isMulti) noexcept
{
    if (m_hwnd == nullptr)
    {
        return;
    }

    if (isMulti)
    {
        SetWindowTextW (m_hwnd, L"Disk II Debug (controller #0 only)");
    }
    else
    {
        SetWindowTextW (m_hwnd, s_kpszDebugWndTitle);
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

    // Spec-006 bug-fix. The Detail column flexes with the dialog so
    // user resize moves the trailing free space INTO the Detail
    // cell rather than leaving it dead at the right edge of the LV.
    SizeDetailColumnToRemainder();

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

    Hide();
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyDown
//
//  Forwards Alt+F4 to the owner Casso window so the whole app exits
//  rather than just hiding this dialog -- matches the user's mental
//  model that Alt+F4 is an "exit Casso" gesture.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugDialog::OnKeyDown (WPARAM vk, LPARAM lParam)
{
    static constexpr LONG_PTR  s_kAltContextBit = 1LL << 29;

    HWND  hwndOwner = nullptr;


    if (vk == VK_F4 && (lParam & s_kAltContextBit))
    {
        hwndOwner = GetWindow (m_hwnd, GW_OWNER);
        if (hwndOwner != nullptr)
        {
            PostMessage (hwndOwner, WM_CLOSE, 0, 0);
            return false;
        }
    }

    return Window::OnKeyDown (vk, lParam);
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
        if (notifyCode == BN_CLICKED)
        {
            if (id == kIdBtnPause)
            {
                m_paused = !m_paused;
                SetWindowTextW (m_pauseButton, m_paused ? L"Resume" : L"Pause");
            }
            else
            {
                m_deque.clear();
                m_filteredIndices.clear();
                m_lastPublishedCount = -1;

                if (m_listView != nullptr)
                {
                    ListView_SetItemCountEx (m_listView, 0, LVSICF_NOSCROLL);
                    InvalidateRect (m_listView, nullptr, FALSE);
                }
            }
        }

        return false;
    }

    if (id == kIdEdtTrack || id == kIdEdtSector)
    {
        if (notifyCode == EN_CHANGE)
        {
            OnFilterTextChanged();
        }
        else if (notifyCode == EN_KILLFOCUS)
        {
            OnFilterTextKillFocus();
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
    bool      checked              = false;
    uint32_t  catBit               = 0;
    int       slot                 = 0;
    int       priorFocusedDequeIdx = 0;
    bool      didRefilter          = true;

    if (hCtl != nullptr)
    {
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
            UpdateAudioSubEnableState();
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
            SetWindowTextW (m_trackFilterLabel,
                            checked ? L"Quarter-track:" : L"Track filter:");
            // raw-qt re-interprets bare integers as quarter tracks so the
            // track predicate has to be re-parsed against the new flag.
            FlushFilterDebounce();
            didRefilter = false;
        }
        else
        {
            didRefilter = false;
        }

        if (didRefilter)
        {
            priorFocusedDequeIdx = CapturedFocusedDequeIdx();

            RebuildFilteredIndices();
            InvalidateListView();
            RestoreFocusedDequeIdx (priorFocusedDequeIdx);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateAudioSubEnableState
//
//  FR-014c: when the Audio master is unchecked the four sub-checkboxes
//  grey out but keep their checked state for restoration.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::UpdateAudioSubEnableState()
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

void DiskIIDebugDialog::OnFilterTextChanged()
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

void DiskIIDebugDialog::OnFilterTextKillFocus()
{
    if (m_hwnd != nullptr && m_filterDebouncePending)
    {
        KillTimer (m_hwnd, m_filterDebounceTimerId);
        m_filterDebouncePending = false;
    }

    FlushFilterDebounce();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadEditText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskIIDebugDialog::ReadEditText (HWND hEdit) const
{
    HRESULT       hr  = S_OK;
    int           len = 0;
    std::wstring  out;

    CBR (hEdit != nullptr);

    len = GetWindowTextLengthW (hEdit);
    CBR (len > 0);

    out.resize (static_cast<size_t> (len));
    GetWindowTextW (hEdit, out.data(), len + 1);

Error:
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

void DiskIIDebugDialog::FlushFilterDebounce()
{
    std::wstring  trackText;
    std::wstring  sectorText;
    bool          trackTextChanged       = false;
    bool          sectorTextChanged      = false;
    int           priorFocusedDequeIdx   = 0;

    m_filterDebouncePending = false;

    trackText  = ReadEditText (m_trackRichEdit);
    sectorText = ReadEditText (m_sectorRichEdit);

    m_filter.trackFilter  = TrackSectorPredicate::Parse (trackText,
                                                         TrackSectorPredicate::Mode::Track,
                                                         m_filter.trackFilterRawQt);
    m_filter.sectorFilter = TrackSectorPredicate::Parse (sectorText,
                                                         TrackSectorPredicate::Mode::Sector);

    priorFocusedDequeIdx = CapturedFocusedDequeIdx();

    RebuildFilteredIndices();
    InvalidateListView();
    RestoreFocusedDequeIdx (priorFocusedDequeIdx);

    // Spec-006 bug 4. Only re-apply squiggle formats when the text
    // content actually changed. Re-painting the same squiggles on
    // every EN_CHANGE / debounce tick re-anchors the selection and
    // breaks right-to-left mouse drags.
    trackTextChanged  = (trackText  != m_lastFormattedTrackText);
    sectorTextChanged = (sectorText != m_lastFormattedSectorText);

    if (trackTextChanged)
    {
        ApplyRejectedTokenSquiggles (m_trackRichEdit, m_filter.trackFilter.RejectedSpans());
        m_lastFormattedTrackText = trackText;
    }

    if (sectorTextChanged)
    {
        ApplyRejectedTokenSquiggles (m_sectorRichEdit, m_filter.sectorFilter.RejectedSpans());
        m_lastFormattedSectorText = sectorText;
    }

    SetPerSideInvalidLabel (m_trackInvalidLabel,
                            L"Invalid track: ",
                            trackText, m_filter.trackFilter.RejectedSpans());
    SetPerSideInvalidLabel (m_sectorInvalidLabel,
                            L"Invalid sector: ",
                            sectorText, m_filter.sectorFilter.RejectedSpans());
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

void DiskIIDebugDialog::RebuildFilteredIndices()
{
    size_t  i = 0;

    m_filteredIndices.clear();

    for (i = 0; i < m_deque.size(); i++)
    {
        if (MatchesFilter (m_deque[i], m_filter))
        {
            m_filteredIndices.push_back (static_cast<uint32_t> (i));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CapturedFocusedDequeIdx
//
//  Snapshot helper for PreserveFocusedRowAcrossRebuild. Maps the
//  currently-focused ListView item index through the pre-rebuild
//  filtered-indices vector to the underlying deque index. Returns -1
//  when no row is focused or the ListView isn't realized.
//
////////////////////////////////////////////////////////////////////////////////

int DiskIIDebugDialog::CapturedFocusedDequeIdx() const noexcept
{
    int  focused = -1;

    if (m_listView == nullptr)
    {
        return -1;
    }

    focused = ListView_GetNextItem (m_listView, -1, LVNI_FOCUSED);

    if (focused < 0)
    {
        return -1;
    }

    if (static_cast<size_t> (focused) >= m_filteredIndices.size())
    {
        return -1;
    }

    return static_cast<int> (m_filteredIndices[static_cast<size_t> (focused)]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreFocusedDequeIdx
//
//  Restore the focused / selected / visible state after a filter
//  rebuild. Resolves the target LV item index via
//  DebugDialogProjection::PreservedFocusItem and applies LVIS_FOCUSED
//  + LVIS_SELECTED + EnsureVisible. priorDequeIdx == -1 (nothing was
//  focused) is a no-op.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::RestoreFocusedDequeIdx (int priorDequeIdx) noexcept
{
    int  newItem = -1;

    if (m_listView == nullptr || priorDequeIdx < 0)
    {
        return;
    }

    newItem = DebugDialogProjection::PreservedFocusItem (
                  static_cast<uint32_t> (priorDequeIdx),
                  m_filteredIndices);

    if (newItem < 0)
    {
        return;
    }

    ListView_SetItemState (m_listView,
                           newItem,
                           LVIS_FOCUSED | LVIS_SELECTED,
                           LVIS_FOCUSED | LVIS_SELECTED);
    ListView_EnsureVisible (m_listView, newItem, FALSE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InvalidateListView
//
//  Spec-006 bug-fix. Filter-toggle / debounce-flush rebuilds used to
//  flash because the SetItemCount + InvalidateRect combo redrew the
//  full LV even when the filtered set was unchanged. Now: short-
//  circuit when the count and head/tail signature match the last
//  publish, and wrap the count update + invalidate in
//  WM_SETREDRAW(FALSE)..(TRUE) so the LV repaints once at the end of
//  the rebuild rather than flickering through intermediate states.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::InvalidateListView()
{
    int       newCount = 0;
    uint32_t  firstIdx = 0;
    uint32_t  lastIdx  = 0;

    if (m_listView == nullptr)
    {
        return;
    }

    newCount = static_cast<int> (m_filteredIndices.size());

    if (newCount > 0)
    {
        firstIdx = m_filteredIndices.front();
        lastIdx  = m_filteredIndices.back();
    }

    if (newCount == m_lastPublishedCount
        && firstIdx == m_lastPublishedFirstIdx
        && lastIdx  == m_lastPublishedLastIdx)
    {
        // Same projection as last publish; nothing to redraw.
        return;
    }

    SendMessageW (m_listView, WM_SETREDRAW, FALSE, 0);

    ListView_SetItemCountEx (m_listView,
                             newCount,
                             LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

    SendMessageW (m_listView, WM_SETREDRAW, TRUE, 0);
    InvalidateRect (m_listView, nullptr, FALSE);

    m_lastPublishedCount    = newCount;
    m_lastPublishedFirstIdx = firstIdx;
    m_lastPublishedLastIdx  = lastIdx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstallListViewSubclass
//
//  Hook the ListView's WndProc so Ctrl+C copies the current selection.
//  GWLP_USERDATA holds the dialog `this` pointer; the original WndProc
//  is saved on the dialog so we can chain.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIDebugDialog::InstallListViewSubclass()
{
    HRESULT  hr   = S_OK;
    LONG_PTR prev = 0;

    if (m_listView == nullptr)
    {
        return E_UNEXPECTED;
    }

    SetWindowLongPtrW (m_listView, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (this));

    prev = SetWindowLongPtrW (m_listView,
                              GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR> (s_ListViewSubclassProc));
    CWRA (prev);

    m_originalListViewProc = reinterpret_cast<WNDPROC> (prev);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  s_ListViewSubclassProc
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK DiskIIDebugDialog::s_ListViewSubclassProc (
    HWND    hwnd,
    UINT    msg,
    WPARAM  wParam,
    LPARAM  lParam)
{
    DiskIIDebugDialog *  pThis = reinterpret_cast<DiskIIDebugDialog *> (
                                     GetWindowLongPtrW (hwnd, GWLP_USERDATA));

    if (msg == WM_KEYDOWN
        && wParam == L'C'
        && (GetKeyState (VK_CONTROL) & 0x8000))
    {
        if (pThis != nullptr)
        {
            pThis->CopySelectedRowsToClipboard();
        }

        return 0;
    }

    if (pThis != nullptr && pThis->m_originalListViewProc != nullptr)
    {
        return CallWindowProcW (pThis->m_originalListViewProc, hwnd, msg, wParam, lParam);
    }

    return DefWindowProcW (hwnd, msg, wParam, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopySelectedRowsToClipboard
//
//  FR-019: enumerate ListView selection, format each row as
//  tab-separated UTF-16 in visible-column order, and stage on the
//  clipboard as CF_UNICODETEXT.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::CopySelectedRowsToClipboard()
{
    HRESULT                                       hr        = S_OK;
    int                                           selIdx    = -1;
    uint32_t                                      deqIdx    = 0;
    std::vector<const DiskIIEventDisplay *>       selected;
    std::wstring                                  payload;
    HGLOBAL                                       hMem      = nullptr;
    wchar_t  *                                    pMem      = nullptr;
    size_t                                        byteCount = 0;
    bool                                          opened    = false;
    BOOL                                          fSuccess  = FALSE;
    HANDLE                                        hClipData = nullptr;

    if (m_listView == nullptr || m_hwnd == nullptr)
    {
        return;
    }

    selIdx = ListView_GetNextItem (m_listView, -1, LVNI_SELECTED);

    while (selIdx >= 0)
    {
        if (static_cast<size_t> (selIdx) < m_filteredIndices.size())
        {
            deqIdx = m_filteredIndices[selIdx];

            if (deqIdx < m_deque.size())
            {
                selected.push_back (&m_deque[deqIdx]);
            }
        }

        selIdx = ListView_GetNextItem (m_listView, selIdx, LVNI_SELECTED);
    }

    if (selected.empty())
    {
        return;
    }

    payload = BuildClipboardText (selected, m_columns);

    byteCount = (payload.size() + 1) * sizeof (wchar_t);

    hMem = GlobalAlloc (GMEM_MOVEABLE, byteCount);
    CWRA (hMem);

    pMem = static_cast<wchar_t *> (GlobalLock (hMem));
    CWRA (pMem);

    memcpy (pMem, payload.data(), byteCount - sizeof (wchar_t));
    pMem[payload.size()] = L'\0';
    GlobalUnlock (hMem);

    fSuccess = OpenClipboard (m_hwnd);
    CWRA (fSuccess);
    opened = true;

    fSuccess = EmptyClipboard();
    CWRA (fSuccess);

    hClipData = SetClipboardData (CF_UNICODETEXT, hMem);
    CWRA (hClipData);

    // SetClipboardData succeeded -> the system now owns hMem.
    hMem = nullptr;

Error:

    if (opened)
    {
        CloseClipboard();
    }

    if (hMem != nullptr)
    {
        GlobalFree (hMem);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCtlColorStatic
//
//  Spec-006 bug-fix. Paints the single combined invalid label red so
//  the error message stands out beside the filter inputs. All other
//  static controls (filter labels, etc.) fall through to the default
//  system colors.
//
////////////////////////////////////////////////////////////////////////////////

HBRUSH DiskIIDebugDialog::OnCtlColorStatic (HWND hwndDlg, HDC hdc, HWND hwndStatic)
{
    UNREFERENCED_PARAMETER (hwndDlg);

    if (hwndStatic == m_trackInvalidLabel || hwndStatic == m_sectorInvalidLabel)
    {
        SetTextColor (hdc, RGB (200, 0, 0));
        SetBkMode    (hdc, TRANSPARENT);
        return GetSysColorBrush (COLOR_BTNFACE);
    }

    return nullptr;
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
        HandleDrainTick();
        return false;
    }

    if (timerId == m_filterDebounceTimerId)
    {
        if (m_filterDebouncePending)
        {
            KillTimer (m_hwnd, m_filterDebounceTimerId);
            m_filterDebouncePending = false;
        }

        FlushFilterDebounce();
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

void DiskIIDebugDialog::HandleDrainTick()
{
    int       topIndex        = 0;
    int       countPerPage    = 0;
    int       oldCount        = 0;
    int       newCount        = 0;
    bool      wasAtTail       = false;
    bool      shouldPaint     = false;
    uint32_t  dropped         = 0;
    size_t    preDequeSize    = 0;
    size_t    postDequeSize   = 0;
    bool      hitCap          = false;
    bool      firstFit        = false;
    size_t    rowsSinceCheck  = 0;
    bool      periodicFit     = false;
    size_t    measureStart    = 0;
    int       virtualIdx      = 0;
    int       i               = 0;
    int       width           = 0;
    bool      anyGrew         = false;

    if (m_listView == nullptr)
    {
        return;
    }

    oldCount     = static_cast<int> (m_filteredIndices.size());
    topIndex     = ListView_GetTopIndex     (m_listView);
    countPerPage = ListView_GetCountPerPage (m_listView);
    wasAtTail    = ComputeWasAtTail (topIndex, countPerPage, oldCount);

    preDequeSize = m_deque.size();
    dropped      = m_droppedSinceLastDrain.exchange (0, std::memory_order_acq_rel);

    DebugDialogProjection::DrainAndProject (m_ring, m_deque, dropped, m_uptimeAnchor);

    postDequeSize = m_deque.size();

    // pop_front happens only when the rolling cap is hit. When either
    // the pre- or post-drain deque sized at the cap, indices may be
    // stale by some unknown shift; rebuild from scratch. Otherwise
    // run the cheap incremental append for slots [pre..post).
    hitCap = preDequeSize  >= DebugDialogProjection::kDisplayDequeCap
          || postDequeSize >= DebugDialogProjection::kDisplayDequeCap;

    if (hitCap)
    {
        RebuildFilteredIndices();
    }
    else
    {
        AppendFilteredIndicesFor (preDequeSize);
    }

    newCount = static_cast<int> (m_filteredIndices.size());

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

    // Spec-006 bug fix. Auto-grow non-Detail columns periodically so
    // wider data arriving later (e.g. cycle counts crossing the
    // 7-digit / 9-digit boundary) expands the column instead of
    // clipping. Pure grow -- existing widths are never shrunk so any
    // user drag persists. Once the user explicitly resizes a column
    // (HDN_ENDTRACK flips userResized), this loop stops touching that
    // column. Throttled to every kAutoGrowRowThreshold rows so a
    // sustained drain doesn't pay the string-width cost on every
    // WM_TIMER tick.
    firstFit       = !m_firstAutoFitDone && !m_deque.empty();
    rowsSinceCheck = (m_deque.size() >= m_dequeSizeAtLastGrow)
                         ? (m_deque.size() - m_dequeSizeAtLastGrow)
                         : m_deque.size();
    periodicFit    = m_firstAutoFitDone && rowsSinceCheck >= kAutoGrowRowThreshold;

    if (firstFit || periodicFit)
    {
        // First-ever fit measures the entire deque; subsequent
        // periodic fits only measure rows added since the last grow
        // (the "never shrink" invariant means existing wider columns
        // already hold their floor, so comparing the new chunk's max
        // against savedWidth suffices). If the deque shrunk under us
        // (rolling-cap pop_front), fall back to measuring everything.
        measureStart = firstFit ? 0 : (m_deque.size() - rowsSinceCheck);

        for (i = 0; i < kColumnCount; i++)
        {
            if (!m_columns[i].visible)
            {
                continue;
            }

            if (m_columns[i].id != kDetailColumnId && !m_columns[i].userResized)
            {
                width = MeasureColumnContentWidth (m_columns[i].id, measureStart);

                if (width > m_columns[i].savedWidth)
                {
                    ListView_SetColumnWidth (m_listView, virtualIdx, width);
                    m_columns[i].savedWidth = width;
                    anyGrew                 = true;
                }

                m_columns[i].autoSizedYet = true;
            }

            virtualIdx++;
        }

        if (firstFit || anyGrew)
        {
            SizeDetailColumnToRemainder();
        }

        m_firstAutoFitDone     = true;
        m_dequeSizeAtLastGrow  = m_deque.size();
    }

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
    if (m_filteredIndices.size() > m_deque.size())
    {
        RebuildFilteredIndices();
        return;
    }

    if (startIdx > m_deque.size())
    {
        startIdx = m_deque.size();
    }

    for (i = startIdx; i < m_deque.size(); i++)
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
    NMHDR  *  pHdr      = reinterpret_cast<NMHDR *> (lParam);
    HWND      header    = nullptr;
    DWORD     msgPos    = 0;
    POINT     pt        = {};

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

    if (m_listView != nullptr)
    {
        header = ListView_GetHeader (m_listView);
    }

    // FR-026 / FR-027. The ListView's header subcontrol surfaces
    // right-clicks as NM_RCLICK and user width-drag completion as
    // HDN_ENDTRACK. Both fire through the parent's WM_NOTIFY.
    if (header != nullptr && pHdr->hwndFrom == header)
    {
        if (pHdr->code == NM_RCLICK)
        {
            msgPos = static_cast<DWORD> (GetMessagePos());
            pt.x   = static_cast<int> (static_cast<short> (LOWORD (msgPos)));
            pt.y   = static_cast<int> (static_cast<short> (HIWORD (msgPos)));
            ShowHeaderContextMenu (pt.x, pt.y);
            return false;
        }

        if (pHdr->code == HDN_ENDTRACKW || pHdr->code == HDN_ENDTRACKA)
        {
            NMHEADERW *  hdrN     = reinterpret_cast<NMHEADERW *> (lParam);
            int          logicalId = -1;

            CaptureCurrentWidthsIntoModel();

            // Spec-006 bug fix. Mark the dragged column as "user
            // resized" so the periodic auto-grow check in the drain
            // tick stops widening it past the user's chosen width.
            if (hdrN != nullptr && hdrN->iItem >= 0 && hdrN->iItem < kColumnCount)
            {
                logicalId = m_visibleOrdinalToLogicalId[hdrN->iItem];

                if (logicalId >= 0 && logicalId < kColumnCount)
                {
                    m_columns[logicalId].userResized = true;
                }
            }

            return false;
        }
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
    thread_local wchar_t  s_driveBuf[4]       = {};

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

    if (item.iItem < 0 || static_cast<size_t> (item.iItem) >= m_filteredIndices.size())
    {
        return;
    }

    deqIdx = m_filteredIndices[item.iItem];

    if (deqIdx >= m_deque.size())
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
            item.pszText = const_cast<LPWSTR> (e.wallStr.data());
            break;

        case 1:
            item.pszText = const_cast<LPWSTR> (e.uptimeStr.data());
            break;

        case 2:
            item.pszText = const_cast<LPWSTR> (e.cycleStr.data());
            break;

        case 3:
            if (e.drive != DiskIIEventDisplay::kFieldNotApplicable)
            {
                swprintf_s (s_driveBuf, L"%d", e.drive + 1);
                item.pszText = s_driveBuf;
            }
            else
            {
                s_driveBuf[0] = L'\0';
                item.pszText  = s_driveBuf;
            }
            break;

        case 4:
            label   = DebugDialogProjection::EventLabel (e.category, e.type);
            copyLen = std::min<size_t> (label.size(),
                                        (sizeof (s_eventLabelBuf) / sizeof (s_eventLabelBuf[0])) - 1);
            std::copy_n (label.data(), copyLen, s_eventLabelBuf);
            s_eventLabelBuf[copyLen] = L'\0';
            item.pszText = s_eventLabelBuf;
            break;

        case 5:
            item.pszText = const_cast<LPWSTR> (e.detail.c_str());
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
    DiskIIEvent  stamped = e;

    if (m_cycleCounter != nullptr)
    {
        stamped.cycle = *m_cycleCounter;
    }

    if (!m_ring.TryPush (stamped))
    {
        m_droppedSinceLastDrain.fetch_add (1, std::memory_order_relaxed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearEvents
//
//  Wipe the dialog's display state and discard any in-flight producer
//  events. Called by EmulatorShell::ResetUptimeAnchor on every soft
//  reset / power cycle so the debug log doesn't carry stale rows
//  from the pre-reset boot into the post-reset uptime anchor.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::ClearEvents() noexcept
{
    // Stack scratch buffer for the drain loop. 64 entries x ~32 bytes
    // per DiskIIEvent = ~2 KB on the stack -- well under any sensible
    // thread-stack reservation. Larger batch sizes save a few drain
    // calls on huge backlogs at no behavior cost.
    constexpr uint32_t  kClearDrainBatchSize = 64;

    DiskIIEvent  scratch[kClearDrainBatchSize] = {};
    uint32_t     drained                       = 0;

    m_droppedSinceLastDrain.store (0, std::memory_order_release);

    do
    {
        drained = m_ring.Drain (scratch, kClearDrainBatchSize);
    }
    while (drained > 0);

    m_deque.clear();
    m_filteredIndices.clear();
    m_firstAutoFitDone     = false;
    m_dequeSizeAtLastGrow  = 0;
    m_lastPublishedCount   = -1;
    m_currentDrive         = 0;

    if (m_listView != nullptr)
    {
        ListView_SetItemCountEx (m_listView, 0, LVSICF_NOSCROLL);
        InvalidateRect (m_listView, nullptr, FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeStampedEvent
//
//  Boilerplate factory for the seven Push*Event helpers. Stamps the
//  category, the event type, and the currently-selected drive (each
//  per-payload Push* helper then fills in its own payload struct).
//
////////////////////////////////////////////////////////////////////////////////

DiskIIEvent DiskIIDebugDialog::MakeStampedEvent (EventCategory cat, DiskIIEventType type) const noexcept
{
    DiskIIEvent  e = {};

    e.category = cat;
    e.type     = type;
    e.drive    = static_cast<int8_t> (m_currentDrive);

    return e;
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
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, type);

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushHeadStepEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushHeadStepEvent (int prevQt, int newQt) noexcept
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::HeadStep);

    e.payload.step.prevQt = prevQt;
    e.payload.step.newQt  = newQt;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushHeadBumpEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushHeadBumpEvent (int atQt) noexcept
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::HeadBump);

    e.payload.bump.atQt = atQt;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushAddrMarkEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushAddrMarkEvent (int track, int sector, int volume) noexcept
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::AddrMark);

    e.payload.addrMark.track  = track;
    e.payload.addrMark.sector = sector;
    e.payload.addrMark.volume = volume;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushDataMarkEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushDataMarkEvent (DiskIIEventType type, int track, int sector, int volume, int byteCount) noexcept
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, type);

    e.payload.dataMark.track     = track;
    e.payload.dataMark.sector    = sector;
    e.payload.dataMark.volume    = volume;
    e.payload.dataMark.byteCount = byteCount;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushDriveEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::PushDriveEvent (DiskIIEventType type, int drive) noexcept
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, type);

    // Drive events carry their target drive explicitly (eject / insert
    // routes through whichever bay the user touched, not the currently
    // selected one).
    e.drive               = static_cast<int8_t> (drive);
    e.payload.drive.drive = drive;

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
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Audio, type);

    // Audio events carry their target drive explicitly (sourced from
    // the per-drive DiskIIAudioSource, not the controller's currently
    // selected drive).
    e.drive                = static_cast<int8_t> (drive);
    e.payload.audio.kind   = kind;
    e.payload.audio.reason = reason;
    e.payload.audio.drive  = drive;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskIIEventSink overrides
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugDialog::OnMotorCommandOn()  { PushControllerEvent (DiskIIEventType::MotorCommandOn);   }
void DiskIIDebugDialog::OnMotorEngaged()    { PushControllerEvent (DiskIIEventType::MotorEngaged);     }
void DiskIIDebugDialog::OnMotorCommandOff() { PushControllerEvent (DiskIIEventType::MotorCommandOff);  }
void DiskIIDebugDialog::OnMotorDisengaged() { PushControllerEvent (DiskIIEventType::MotorDisengaged);  }

void DiskIIDebugDialog::OnHeadStep         (int prevQt, int newQt)              { PushHeadStepEvent (prevQt, newQt); }
void DiskIIDebugDialog::OnHeadBump         (int atQt)                           { PushHeadBumpEvent (atQt); }
void DiskIIDebugDialog::OnAddressMark      (int track, int sector, int volume)  { PushAddrMarkEvent (track, sector, volume); }
void DiskIIDebugDialog::OnDataMarkRead     (int track, int sector, int volume, int byteCount) { PushDataMarkEvent (DiskIIEventType::DataRead,  track, sector, volume, byteCount); }
void DiskIIDebugDialog::OnDataMarkWrite    (int track, int sector, int volume, int byteCount) { PushDataMarkEvent (DiskIIEventType::DataWrite, track, sector, volume, byteCount); }
void DiskIIDebugDialog::OnDriveSelect      (int drive)                          { m_currentDrive = drive; PushDriveEvent    (DiskIIEventType::DriveSelect,  drive); }
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
