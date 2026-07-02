#pragma once

#include "Window/DxuiWindow.h"
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
//  Themed DX replacement for the legacy Win32 Disk2DebugDialog. Derives
//  from DxuiWindow, so it IS its own content-root panel AND owns the OS
//  window (HWND + swap chain + caption + paint pump) through the base
//  class -- the subclass never touches an HWND, a WPARAM, or a host
//  client interface. It still implements the same two event-sink
//  interfaces (IDisk2EventSink and IDriveAudioEventSink) so it slots
//  into the existing EmulatorShell event wiring with no contract
//  changes.
//
//  Content widgets are created as children of this panel in OnCreate
//  (via the inherited Create<T> factory) so the base paint pump walks
//  and paints them; the panel keeps its own focus manager, tooltip, and
//  column menu (the latter two escape the client via the host popup
//  pool exposed through PopupHost()).
//
////////////////////////////////////////////////////////////////////////////////

class Disk2DebugPanel : public DxuiWindow,
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
    void    Destroy ();

    bool    IsOpen () const { return IsCreated(); }

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

    // Framework input entry points. These DxuiPanel overrides own the
    // panel's mouse / key routing directly, dispatching each
    // DxuiMouseEvent / DxuiKeyEvent to the child widgets and the event
    // list; DxuiWindow translates the Win32 messages into these.
    bool    OnMouse (const DxuiMouseEvent & ev)                     override;
    bool    OnKey   (const DxuiKeyEvent   & ev)                     override;

    // DxuiPanel layout hook. DxuiWindow calls this with the client
    // bounds / DPI scaler after the OS window resizes; caches the size
    // and re-runs the panel's absolute layout so the child widgets track
    // the new bounds.
    void    Layout  (const RECT          & boundsDip,
                     const DxuiDpiScaler & scaler)                  override;

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

protected:
    // DxuiWindow hook. Fires inside Create() once the backend + HWND
    // exist; populates the child widgets via the inherited Create<T>
    // factory and wires their state / callbacks.
    void    OnCreate ()                                             override;

private:
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

    PanelLayoutSlots                     m_layout = {};

    const CassoTheme                   * m_theme    = nullptr;
    int                                  m_widthPx  = 0;
    int                                  m_heightPx = 0;
    UINT                                 m_dpi      = 96;

    DxuiLabel                                      * m_trackFilterLabel   = nullptr;
    DxuiLabel                                      * m_sectorFilterLabel  = nullptr;
    DxuiLabel                                      * m_driveFilterLabel   = nullptr;
    DxuiLabel                                      * m_diskEventsLabel    = nullptr;
    DxuiLabel                                      * m_audioEventsLabel   = nullptr;
    DxuiLabel                                      * m_trackInvalidLabel  = nullptr;
    DxuiLabel                                      * m_sectorInvalidLabel = nullptr;

    std::array<DxuiCheckbox*, kEventTypeCheckCount>  m_eventChecks        = {};
    DxuiCheckbox                                   * m_audioMasterCheck   = nullptr;
    std::array<DxuiCheckbox*, kAudioSubCheckCount>   m_audioSubChecks     = {};
    DxuiCheckbox                                   * m_rawQtCheck         = nullptr;
    DxuiRadioGroup                                 * m_driveRadio         = nullptr;
    DxuiTextInput                                  * m_trackEdit          = nullptr;
    DxuiTextInput                                  * m_sectorEdit         = nullptr;
    DxuiButton                                     * m_pauseButton        = nullptr;
    DxuiButton                                     * m_clearButton        = nullptr;
    DxuiListView                                   * m_eventList          = nullptr;
    DxuiTooltip                                      m_tooltip;
    DxuiPopupMenu                                    m_columnMenu;
    DxuiFocusManager                                 m_focusMgr;

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
