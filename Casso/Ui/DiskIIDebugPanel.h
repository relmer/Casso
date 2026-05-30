#pragma once

#include "Chrome/ChromedPanelWindow.h"
#include "Chrome/IChromedPanelContent.h"
#include "DiskIIDebugPanelLayout.h"
#include "DxUiPainter.h"
#include "DwriteTextRenderer.h"
#include "Widgets/Button.h"
#include "Widgets/Checkbox.h"
#include "Widgets/Label.h"
#include "Widgets/ListView.h"
#include "Widgets/PopupMenu.h"
#include "Widgets/Radio.h"
#include "Widgets/TextInput.h"
#include "Widgets/Tooltip.h"

#include "../DiskIIDebugDialogState.h"
#include "../DiskIIEventDisplay.h"
#include "../../CassoEmuCore/Devices/IDiskIIEventSink.h"
#include "../../CassoEmuCore/Devices/DiskIIEventRing.h"
#include "../../CassoEmuCore/Audio/IDriveAudioEventSink.h"


struct ChromeTheme;
class TitleBar;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugPanel
//
//  Spec-011 / US7. Themed DX replacement for the legacy Win32
//  DiskIIDebugDialog. Hosts itself inside a ChromedPanelWindow and
//  implements the same two event-sink interfaces (IDiskIIEventSink
//  and IDriveAudioEventSink) so it slots into the existing
//  EmulatorShell event wiring with no contract changes.
//
//  T044 lands this empty -- chrome + state binding only, no controls.
//  T046 brings the layout, T047-T057 the individual control families.
//  Until T046, every sink callback is a no-op so the panel never
//  drops events but also never re-renders.
//
////////////////////////////////////////////////////////////////////////////////

class DiskIIDebugPanel : public IChromedPanelContent,
                         public IDiskIIEventSink,
                         public IDriveAudioEventSink
{
public:
    DiskIIDebugPanel  ();
    ~DiskIIDebugPanel () override;

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
    bool     OnKey              (WPARAM vk)                         override;
    bool     OnChar             (wchar_t ch)                        override;
    void     Accept             ()                                  override;
    void     Cancel             ()                                  override;
    bool     IsContentActive    () const                            override;
    bool     IsNonModal         () const                            override { return true; }
    HCURSOR  OnSetCursor        (int x, int y)                      override;

    // IDiskIIEventSink. Producer thread -- push into the lock-free ring;
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
    void    PublishToRing        (const DiskIIEvent & e);
    DiskIIEvent  MakeStampedEvent (EventCategory cat, DiskIIEventType type) const noexcept;
    void    OnFilterChanged      ();
    void    OnTrackEditChanged   ();
    void    OnSectorEditChanged  ();
    void    UpdatePauseLabel     ();
    void    UpdateTooltip        (int x, int y);
    void    ShowColumnMenu       (int anchorX, int anchorY);
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

    DxUiPainter                          m_painter;
    DwriteTextRenderer                   m_text;

    Label                                m_trackFilterLabel;
    Label                                m_sectorFilterLabel;
    Label                                m_driveFilterLabel;
    Label                                m_trackInvalidLabel;
    Label                                m_sectorInvalidLabel;

    std::array<Checkbox, kEventTypeCheckCount>  m_eventChecks;
    Checkbox                                    m_audioMasterCheck;
    std::array<Checkbox, kAudioSubCheckCount>   m_audioSubChecks;
    Checkbox                                    m_rawQtCheck;
    RadioGroup                                  m_driveRadio;
    TextInput                                   m_trackEdit;
    TextInput                                   m_sectorEdit;
    Button                                      m_pauseButton;
    Button                                      m_clearButton;
    ListView                                    m_eventList;
    Tooltip                                     m_tooltip;
    PopupMenu                                   m_columnMenu;

    FilterState                           m_filter;
    DiskIIEventRing                       m_ring;
    std::deque<DiskIIEventDisplay>        m_events;
    std::vector<size_t>                   m_filteredIndices;
    std::atomic<uint32_t>                 m_droppedSinceLastDrain = 0;
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
};
