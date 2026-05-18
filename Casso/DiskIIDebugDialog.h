#pragma once

#include "Pch.h"

#include "Window.h"
#include "DiskIIDebugDialogState.h"
#include "DebugDialogProjection.h"
#include "../CassoEmuCore/Devices/IDiskIIEventSink.h"
#include "../CassoEmuCore/Devices/DiskIIEventRing.h"
#include "../CassoEmuCore/Audio/IDriveAudioEventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugDialog
//
//  Spec-006 modeless debug window. Implements BOTH IDiskIIEventSink
//  (the controller-side contract) and IDriveAudioEventSink (the
//  audio-side contract). Each sink callback packs a DiskIIEvent POD
//  and tries to push it onto m_ring; ring-full bumps
//  m_droppedSinceLastDrain (atomic, CPU-thread only) so the next
//  UI-thread drain can emit a single coalesced [N events lost] marker.
//
//  The dialog is owned by EmulatorShell and reused across opens; the
//  WM_CLOSE handler hides the window rather than destroying it.
//
////////////////////////////////////////////////////////////////////////////////

class DiskIIDebugDialog : public Window,
                          public IDiskIIEventSink,
                          public IDriveAudioEventSink
{
public:
    DiskIIDebugDialog();
    ~DiskIIDebugDialog() override;

    HRESULT Create  (HINSTANCE hInstance, HWND parentHwnd);
    void    Show    ();
    void    Hide    ();
    void    Destroy();

    HWND    GetHwnd() const noexcept { return m_hwnd; }

    // Spec-006 / FR-004a. The shell owns the canonical uptime anchor
    // and pokes it through here on construction, on every Open, and
    // on every SoftReset / PowerCycle. The dialog reads its private
    // copy on each WM_TIMER drain when formatting Uptime strings.
    void    SetUptimeAnchor (std::chrono::steady_clock::time_point anchor) noexcept
    {
        m_uptimeAnchor = anchor;
    }

    // Spec-006 bug-fix. SoftReset / PowerCycle wipes the //e back to a
    // known state; the debug log's still-pending events from the old
    // boot are no longer useful at that point. Shell calls this from
    // ResetUptimeAnchor (which already fires on both reset paths).
    // Clears the UI-thread deque, the filtered-index vector, the
    // dropped-since-last-drain counter, and drains the SPSC ring to
    // discard any in-flight producer events. Pause / resume state is
    // intentionally preserved -- the user may be paused inspecting
    // pre-reset state and a reset shouldn't yank them out of pause.
    void    ClearEvents () noexcept;

    // Spec-006 / FR-017. When the active machine config has more
    // than one Disk II controller, append " (controller #0 only)" to
    // the window title so the user knows the dialog is wired to the
    // first controller. Called by the shell at open time.
    void    SetMultiControllerHint (bool isMulti) noexcept;

    // Spec-006 / FR-005 / bug-fix. The shell owns the CPU cycle
    // counter; the dialog dereferences this pointer on every
    // PublishToRing call so each event carries the cycle at which
    // the controller / audio source fired it. nullptr default keeps
    // headless tests (and the pre-Open window) safe.
    void    SetCycleCounter (const uint64_t * counter) noexcept
    {
        m_cycleCounter = counter;
    }

    // IDiskIIEventSink
    void OnMotorCommandOn   () override;
    void OnMotorEngaged     () override;
    void OnMotorCommandOff  () override;
    void OnMotorDisengaged  () override;
    void OnHeadStep         (int prevQt, int newQt) override;
    void OnHeadBump         (int atQt) override;
    void OnAddressMark      (int track, int sector, int volume) override;
    void OnDataMarkRead     (int sector, int byteCount) override;
    void OnDataMarkWrite    (int sector, int byteCount) override;
    void OnDriveSelect      (int drive) override;
    void OnDiskInserted     (int drive) override;
    void OnDiskEjected      (int drive) override;

    // IDriveAudioEventSink
    void OnAudioStarted      (SoundKind kind, int drive) override;
    void OnAudioRestarted    (SoundKind kind, int drive) override;
    void OnAudioContinued    (SoundKind kind, int drive) override;
    void OnAudioSilent       (SoundKind kind, int drive, SilentReason reason) override;
    void OnAudioLoopStarted  (SoundKind kind, int drive) override;
    void OnAudioLoopStopped  (SoundKind kind, int drive) override;

protected:
    LRESULT OnCreate     (HWND hwnd, CREATESTRUCT * pcs)   override;
    bool    OnClose      (HWND hwnd)                       override;
    bool    OnDestroy    (HWND hwnd)                       override;
    bool    OnSize       (HWND hwnd, UINT width, UINT height) override;
    bool    OnCommandEx  (HWND hwnd, int id, int notifyCode, HWND hCtl) override;
    bool    OnTimer      (HWND hwnd, UINT_PTR timerId)     override;
    bool    OnNotify     (HWND hwnd, WPARAM wParam, LPARAM lParam) override;

private:
    void    PushControllerEvent  (DiskIIEventType type) noexcept;
    void    PushHeadStepEvent    (int prevQt, int newQt) noexcept;
    void    PushHeadBumpEvent    (int atQt) noexcept;
    void    PushAddrMarkEvent    (int track, int sector, int volume) noexcept;
    void    PushDataMarkEvent    (DiskIIEventType type, int sector, int byteCount) noexcept;
    void    PushDriveEvent       (DiskIIEventType type, int drive) noexcept;
    void    PushAudioEvent       (DiskIIEventType type, SoundKind kind, int drive, SilentReason reason) noexcept;
    void    PublishToRing        (const DiskIIEvent & e) noexcept;

    HRESULT CreateChildControls         (HWND hwnd);
    void    LayoutChildControls         (int width, int height);
    void    RebuildListViewColumns      ();
    int     MeasureColumnContentWidth   (int logicalId) const;
    void    SizeDetailColumnToRemainder ();
    void    ToggleColumn                (int id);
    void    CaptureCurrentWidthsIntoModel();
    void    ShowHeaderContextMenu       (int x, int y);
    HFONT   AcquireUiFont               ();

    void    HandleDrainTick             ();
    void    HandleGetDispInfo           (NMLVDISPINFOW * pInfo);
    void    AppendFilteredIndicesFor    (size_t startDeqIdx);
    void    RebuildFilteredIndices      ();
    void    InvalidateListView          ();

    void    OnFilterControlToggled      (int id, HWND hCtl);
    void    OnFilterTextChanged         ();
    void    OnFilterTextKillFocus       ();
    void    FlushFilterDebounce         ();
    std::wstring ReadEditText           (HWND hEdit) const;
    void    UpdateAudioSubEnableState   ();

    HRESULT InstallListViewSubclass     ();
    void    CopySelectedRowsToClipboard();
    static LRESULT CALLBACK s_ListViewSubclassProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    WNDPROC                                 m_originalListViewProc  = nullptr;

    HWND                                    m_listView           = nullptr;

    // Event-type checkboxes (FR-014 categories, fixed order)
    // 0 Motor / 1 HeadStep / 2 HeadBump / 3 AddrMark
    // 4 Read  / 5 Write    / 6 Door     / 7 DriveSelect
    std::array<HWND, 8>                     m_eventTypeChecks    {};

    HWND                                    m_audioMasterCheck   = nullptr;

    // Audio sub-checkboxes: Started / Restarted / Continued / Silent
    std::array<HWND, 4>                     m_audioSubCheck      {};

    // Drive radio: 0 = All, 1 = Drive 1, 2 = Drive 2
    std::array<HWND, 3>                     m_driveRadio         {};

    HWND                                    m_trackRichEdit      = nullptr;
    HWND                                    m_sectorRichEdit     = nullptr;
    HWND                                    m_trackFilterLabel   = nullptr;
    HWND                                    m_sectorFilterLabel  = nullptr;
    HWND                                    m_filterTooltip      = nullptr;
    HWND                                    m_trackIgnoredLabel  = nullptr;
    HWND                                    m_sectorIgnoredLabel = nullptr;
    HWND                                    m_trackRawQtCheck    = nullptr;

    // Spec-006 bug 4. The squiggle re-application reaches into the
    // RichEdit's selection state via SetSel + SetCharFormat; even
    // with EM_EXGETSEL/EM_EXSETSEL bracketing, fighting an active
    // user mouse-drag selection can re-anchor the selection caret
    // in the middle of a drag. Skip the squiggle pass entirely
    // when the text content matches what was last formatted.
    std::wstring                            m_lastFormattedTrackText;
    std::wstring                            m_lastFormattedSectorText;

    HWND                                    m_pauseButton        = nullptr;
    HWND                                    m_clearButton        = nullptr;

    HFONT                                   m_uiFont             = nullptr;

    DiskIIEventRing                         m_ring;
    std::atomic<uint32_t>                   m_droppedSinceLastDrain { 0 };
    std::deque<DiskIIEventDisplay>          m_deque;
    std::vector<uint32_t>                   m_filteredIndices;

    FilterState                             m_filter;
    bool                                    m_paused             = false;

    std::array<LogicalColumn, kColumnCount>     m_columns            {};
    std::array<int, kColumnCount>               m_visibleOrdinalToLogicalId {};

    UINT_PTR                                m_drainTimerId          = 1;
    UINT_PTR                                m_filterDebounceTimerId = 2;
    bool                                    m_drainTimerActive      = false;
    bool                                    m_filterDebouncePending = false;

    // Spec-006 bug-fix. The dialog opens with an empty deque so the
    // first RebuildListViewColumns pass can only size each non-Detail
    // column to the width of its header text. As soon as the first
    // batch of real events lands in the deque, the drain tick re-fits
    // every still-untouched column to MAX (header, widest cell). Set
    // to false on every ClearEvents() so a soft-reset re-runs the fit.
    bool                                    m_firstAutoFitDone      = false;

    // Spec-006 bug-fix. InvalidateListView short-circuits when the
    // (count, first-deque-idx, last-deque-idx) triple matches the
    // last publish -- prevents flashing on filter-checkbox toggles
    // that don't actually change the projection.
    int                                     m_lastPublishedCount    = -1;
    uint32_t                                m_lastPublishedFirstIdx = 0;
    uint32_t                                m_lastPublishedLastIdx  = 0;

    std::chrono::steady_clock::time_point   m_uptimeAnchor;
    const uint64_t *                        m_cycleCounter       = nullptr;

    // Spec-006 bug fix. Cached active-drive index used to stamp
    // DiskIIEvent::drive on every controller-side event that doesn't
    // carry its own drive (motor / head / address mark / data mark).
    // Initialized to 0 (controller boots with drive 0 active);
    // updated on OnDriveSelect BEFORE the event is pushed so the
    // stamped value matches the controller's new active drive.
    int                                     m_currentDrive       = 0;
};
