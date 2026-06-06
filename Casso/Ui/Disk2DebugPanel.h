#pragma once

#include "Chrome/ChromedPanelWindow.h"
#include "Chrome/IChromedPanelContent.h"
#include "Disk2DebugPanelLayout.h"
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


struct ChromeTheme;
class TitleBar;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanel
//
//  Spec-011 / US7. Themed DX replacement for the legacy Win32
//  Disk2DebugDialog. Hosts itself inside a ChromedPanelWindow and
//  implements the same two event-sink interfaces (IDisk2EventSink
//  and IDriveAudioEventSink) so it slots into the existing
//  EmulatorShell event wiring with no contract changes.
//
//  T044 lands this empty -- chrome + state binding only, no controls.
//  T046 brings the layout, T047-T057 the individual control families.
//  Until T046, every sink callback is a no-op so the panel never
//  drops events but also never re-renders.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2DebugPanel : public DxuiPanel,
                         public IChromedPanelContent,
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
                     const ChromeTheme    * theme);
    void    Show    ();
    void    Hide    ();
    void    Destroy ();

    bool    IsOpen () const { return m_window.IsOpen(); }
    HWND    Hwnd   () const { return m_window.Hwnd(); }

    HRESULT RenderFrame ();
    void    SetTheme    (const ChromeTheme * theme);
    void    SetCycleCounter (const uint64_t * cycleCounter) { m_cycleCounter = cycleCounter; }
    void    SetUptimeAnchor (std::chrono::steady_clock::time_point anchor) { m_uptimeAnchor = anchor; }
    void    SetMultiControllerHint (bool multi)            { m_multiController = multi; }
    void    ClearEvents     ();

    // Thread-safe reset hook. The CPU/reset thread calls this to stage a
    // new Uptime anchor and request an event-list clear; the render
    // thread applies both inside DrainAndProject so the event deque and
    // DxuiListView rows are only ever mutated on one thread.
    void    RequestResetAnchor (std::chrono::steady_clock::time_point anchor) noexcept;

    // IChromedPanelContent.
    LPCWSTR  GetWindowClassName () const override;
    LPCWSTR  GetWindowTitle     () const override;
    HRESULT  OnHostCreated      (HWND                   hwnd,
                                 ID3D11Device         * device,
                                 ID3D11DeviceContext  * context,
                                 int                    widthPx,
                                 int                    heightPx,
                                 UINT                   dpi,
                                 TitleBar             * titleBar,
                                 const ChromeTheme    * theme) override;
    void     OnHostDestroyed    ()                                  override;
    HRESULT  OnHostResize       (int widthPx, int heightPx, UINT dpi) override;
    void     SetChromeTheme     (TitleBar * titleBar, const ChromeTheme * theme) override;
    SIZE     PreferredClientSize (UINT dpi) const                   override;
    HRESULT  Render             ()                                  override;
    void     OnLButtonDown      (int x, int y)                      override;
    void     OnLButtonUp        (int x, int y)                      override;
    void     OnRButtonDown      (int x, int y)                      override;
    void     OnMouseMove        (int x, int y)                      override;
    void     OnMouseWheel       (int x, int y, int delta)           override;
    bool     OnKey              (WPARAM vk)                         override;
    bool     OnChar             (wchar_t ch)                        override;
    void     Accept             ()                                  override;
    void     Cancel             ()                                  override;
    bool     IsContentActive    () const                            override;
    bool     IsNonModal         () const                            override { return true; }
    HCURSOR  OnSetCursor        (int x, int y)                      override;

    // IDxuiControl pure-virtual overrides supplied by inheriting
    // DxuiPanel. The chrome shell drives this panel directly through
    // OnHostResize / Render and the bespoke IChromedPanelContent input
    // shims above; these adapters exist so an IDxuiControl-tree walk
    // can reach the panel without an explicit downcast. They are
    // intentionally no-ops -- the bespoke pipeline owns layout and
    // paint until the unified Dxui dispatch path absorbs the chrome.
    void    Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void    Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    // Surface the base overloads so virtual dispatch through
    // IDxuiControl* still resolves and direct callers can reach the
    // base overload without name-hiding ambiguity.
    using DxuiPanel::Layout;
    using DxuiPanel::Paint;
    using DxuiPanel::OnKey;

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
    HRESULT EnsureSwapChain      ();
    HRESULT CreateBackBufferRtv  ();
    void    ReleaseRenderTargets ();
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
    void    FocusCycle           (int direction);
    void    SetFocusIndex        (int index);
    void    ClearAllWidgetFocus  ();
    int     DynamicStopCount     () const;
    int     TotalStopCount       () const;
    void    ApplyListSelection   ();
    void    OnListSelectionMoved ();
    void    OnHeaderSortKey      ();
    void    OnDividerResizeKey   (int direction);
    void    SortByColumn         (int absCol);
    int64_t NowMs                () const;

    ChromedPanelWindow                   m_window;
    PanelLayoutSlots                     m_layout = {};

    ID3D11Device                       * m_device  = nullptr;
    ID3D11DeviceContext                * m_context = nullptr;
    const ChromeTheme                  * m_theme   = nullptr;
    TitleBar                           * m_titleBar = nullptr;
    HWND                                 m_hwnd    = nullptr;
    int                                  m_widthPx  = 0;
    int                                  m_heightPx = 0;
    UINT                                 m_dpi      = 96;

    Microsoft::WRL::ComPtr<IDXGISwapChain1>           m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>    m_rtv;

    DxuiPainter                          m_painter;
    DxuiTextRenderer                   m_text;

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

    // Column-resize drag state. When m_resizeColumn >= 0 a drag is
    // in progress: m_resizeStartXPx captures the client-X at drag
    // start and m_resizeStartWidthPx captures the column width at
    // drag start. Delta-applied per mouse-move.
    int                                   m_resizeColumn        = -1;
    int                                   m_resizeStartXPx      = 0;
    int                                   m_resizeStartWidthPx  = 0;

    // Tab-order focus state. m_focusIndex selects which widget owns
    // the keyboard; -1 means no widget is focused. SetFocusIndex
    // mirrors the state to per-widget SetFocused(); FocusCycle wraps
    // forward (+1) or backward (-1) with Tab / Shift+Tab. Indices
    // 0..18 cover the named widget stops in Z-order; 19+ are dynamic
    // per-column stops (header, divider, ..., list) computed from the
    // currently visible columns.
    int                                   m_focusIndex             = -1;
    int                                   m_listSelectedEventIndex = -1;
};
