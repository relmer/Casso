#pragma once

#include "Window/DxuiHostWindow.h"
#include "Window/IDxuiHostClient.h"
#include "Disk2DebugPanelLayout.h"
#include "Core/DxuiFocusManager.h"
#include "Core/DxuiPanel.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiPopupMenu.h"
#include "Widgets/DxuiRadio.h"
#include "Widgets/DxuiTextInput.h"
#include "Widgets/DxuiTooltip.h"

#include "../Disk2DebugDialogState.h"
#include "../Disk2EventDisplay.h"
#include "../../CassoEmuCore/Devices/IDisk2EventSink.h"
#include "../../CassoEmuCore/Devices/Disk2EventRing.h"
#include "../../CassoEmuCore/Audio/IDriveAudioEventSink.h"


struct CassoTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanel
//
//  Spec-011 / US7. Themed DX replacement for the legacy Win32
//  Disk2DebugDialog. Owns a full-ownership DxuiHostWindow (borderless
//  chrome + close-only caption + host-owned swap chain / paint pump)
//  and installs itself as the window's IDxuiHostClient, translating the
//  host's Win32 message hooks into the panel's existing DxuiMouseEvent /
//  DxuiKeyEvent routing. It still implements the same two event-sink
//  interfaces (IDisk2EventSink and IDriveAudioEventSink) so it slots
//  into the existing EmulatorShell event wiring with no contract
//  changes.
//
//  The content widgets are adopted into the host's root panel so the
//  host paint pump walks and paints them; the panel keeps its own
//  focus manager, tooltip, and column menu (the latter two escape the
//  client via the host popup pool).
//
////////////////////////////////////////////////////////////////////////////////

class Disk2DebugPanel : public DxuiPanel,
                         public IDxuiHostClient,
                         public IDisk2EventSink,
                         public IDriveAudioEventSink
{
public:
    Disk2DebugPanel  ();
    ~Disk2DebugPanel () override;

    HRESULT Create  (HINSTANCE              hInstance,
                     HWND                   hwndOwner,
                     ID3D11Device         * device,
                     ID3D11DeviceContext  * context,
                     const CassoTheme    * theme);
    void    Show    ();
    void    Hide    ();
    void    Destroy ();

    bool    IsOpen () const { return m_host != nullptr; }
    HWND    Hwnd   () const { return m_host != nullptr ? m_host->Hwnd() : nullptr; }

    HRESULT RenderFrame ();
    void    SetTheme    (const CassoTheme * theme);
    void    SetCycleCounter (const uint64_t * cycleCounter) { m_cycleCounter = cycleCounter; }
    void    SetUptimeAnchor (std::chrono::steady_clock::time_point anchor) { m_uptimeAnchor = anchor; }
    void    SetMultiControllerHint (bool multi)            { m_multiController = multi; }
    void    ClearEvents     ();

    // Thread-safe reset hook. The CPU/reset thread calls this to stage a
    // new Uptime anchor and request an event-list clear; the render
    // thread applies both inside DrainAndProject so the event deque and
    // DxuiListView rows are only ever mutated on one thread.
    void    RequestResetAnchor (std::chrono::steady_clock::time_point anchor) noexcept;

    // IDxuiHostClient. The host window forwards the Win32 messages it
    // does not own end-to-end; each hook translates into the panel's
    // existing DxuiMouseEvent / DxuiKeyEvent routing (OnMouse / OnKey)
    // or the layout / lifecycle helpers below.
    DxuiMessageResult  OnLButtonDown (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnLButtonUp   (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnRButtonDown (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnMouseMove   (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnMouseWheel  (WPARAM wParam, LPARAM lParam, bool horizontal) override;
    DxuiMessageResult  OnKeyDown     (WPARAM vk, LPARAM lParam) override;
    DxuiMessageResult  OnChar        (WPARAM ch, LPARAM lParam) override;
    DxuiMessageResult  OnSetCursor   (WORD hitTest) override;
    DxuiMessageResult  OnSize        (UINT widthPx, UINT heightPx) override;
    DxuiMessageResult  OnGetMinMax   (MINMAXINFO * info) override;
    DxuiMessageResult  OnClose       () override;
    void               OnDestroy     () override;
    void               OnDpiChanged  (UINT newDpi) override;

    // Framework input entry points. These DxuiPanel overrides own the
    // panel's mouse / key routing directly, dispatching each
    // DxuiMouseEvent / DxuiKeyEvent to the child widgets and the event
    // list so the host drives the panel through the framework.
    bool    OnMouse (const DxuiMouseEvent & ev)                     override;
    bool    OnKey   (const DxuiKeyEvent   & ev)                     override;

    // IDisk2EventSink. Producer thread -- push into the lock-free ring;
    // the render thread drains and projects to display rows per frame.
    void OnMotorCommandOn  ()                                       override;
    void OnMotorEngaged    ()                                       override;
    void OnMotorCommandOff ()                                       override;
    void OnMotorDisengaged ()                                       override;
    void OnHeadStep        (int prevQt, int newQt)                  override;
    void OnHeadBump        (int atQt)                               override;
    void OnAddressMark     (int track, int sector, int volume)      override;
    void OnDataMarkRead    (int track, int sector, int volume, int byteCount) override;
    void OnDataMarkWrite   (int track, int sector, int volume, int byteCount) override;
    void OnDriveSelect     (int drive)                              override;
    void OnDiskInserted    (int drive)                              override;
    void OnDiskEjected     (int drive)                              override;

    // IDriveAudioEventSink. Producer thread.
    void OnAudioStarted     (SoundKind kind, int drive)                    override;
    void OnAudioRestarted   (SoundKind kind, int drive)                    override;
    void OnAudioContinued   (SoundKind kind, int drive)                    override;
    void OnAudioSilent      (SoundKind kind, int drive, SilentReason reason) override;
    void OnAudioLoopStarted (SoundKind kind, int drive)                    override;
    void OnAudioLoopStopped (SoundKind kind, int drive)                    override;

private:
    DxuiMessageResult  DispatchClientMouse (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, float wheelDelta);
    DxuiMessageResult  DispatchClientKey   (DxuiKeyEventKind kind, WPARAM code);
    void    RecomputeLayout      ();
    void    LayoutWidgets        ();
    void    ConfigureWidgets     ();
    void    DrainAndProject      ();
    void    RebuildFilteredIndices ();
    void    PushListViewRows     ();
    void    PublishToRing        (const Disk2Event & e);
    Disk2Event  MakeStampedEvent (EventCategory cat, Disk2EventType type) const noexcept;
    void    OnFilterChanged      ();
    void    OnTrackEditChanged   ();
    void    OnSectorEditChanged  ();
    void    UpdatePauseLabel     ();
    void    UpdateTooltip        (int x, int y);
    void    ShowColumnMenu       (int anchorX, int anchorY);
    void    ApplyListSelection   ();
    void    OnListSelectionMoved ();
    bool    ForwardMouseToList   (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, float wheelDelta);
    void    SortByColumn         (int absCol);
    int64_t NowMs                () const;

    std::unique_ptr<DxuiHostWindow>      m_host;
    PanelLayoutSlots                     m_layout = {};

    ID3D11Device                       * m_device  = nullptr;
    ID3D11DeviceContext                * m_context = nullptr;
    const CassoTheme                  * m_theme   = nullptr;
    HWND                                 m_hwnd    = nullptr;
    int                                  m_widthPx  = 0;
    int                                  m_heightPx = 0;
    UINT                                 m_dpi      = 96;

    DxuiLabel                                m_trackFilterLabel;
    DxuiLabel                                m_sectorFilterLabel;
    DxuiLabel                                m_driveFilterLabel;
    DxuiLabel                                m_diskEventsLabel;
    DxuiLabel                                m_audioEventsLabel;
    DxuiLabel                                m_trackInvalidLabel;
    DxuiLabel                                m_sectorInvalidLabel;

    std::array<DxuiCheckbox, kEventTypeCheckCount>  m_eventChecks;
    DxuiCheckbox                                    m_audioMasterCheck;
    std::array<DxuiCheckbox, kAudioSubCheckCount>   m_audioSubChecks;
    DxuiCheckbox                                    m_rawQtCheck;
    DxuiRadioGroup                                  m_driveRadio;
    DxuiTextInput                                   m_trackEdit;
    DxuiTextInput                                   m_sectorEdit;
    DxuiButton                                      m_pauseButton;
    DxuiButton                                      m_clearButton;
    DxuiListView                                    m_eventList;
    DxuiTooltip                                     m_tooltip;
    DxuiPopupMenu                                   m_columnMenu;
    DxuiFocusManager                                m_focusMgr;

    FilterState                           m_filter;
    Disk2EventRing                       m_ring;
    std::deque<Disk2EventDisplay>        m_events;
    std::vector<size_t>                   m_filteredIndices;
    std::atomic<uint32_t>                 m_droppedSinceLastDrain = 0;
    std::atomic<bool>                     m_resetAnchorPending    = false;
    std::atomic<int64_t>                  m_pendingAnchorTicks    = 0;
    const uint64_t                      * m_cycleCounter = nullptr;
    std::chrono::steady_clock::time_point  m_uptimeAnchor;
    bool                                  m_paused          = false;
    bool                                  m_multiController = false;
    int                                   m_currentDrive    = 0;
    int                                   m_sortColumn      = -1;
    bool                                  m_sortDescending  = false;
    bool                                  m_trackEditValid  = true;
    bool                                  m_sectorEditValid = true;

    int                                   m_listSelectedEventIndex = -1;
};
