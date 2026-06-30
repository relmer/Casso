#pragma once

#include "Chrome/ChromedPanelWindow.h"
#include "Chrome/IChromedPanelContent.h"
#include "InputDebugPanelLayout.h"
#include "Core/DxuiPanel.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiPopupMenu.h"
#include "Widgets/DxuiTooltip.h"

#include "../InputDebugDialogState.h"
#include "../InputEventDisplay.h"
#include "../../CassoEmuCore/Devices/IInputEventSink.h"
#include "../../CassoEmuCore/Devices/InputEventRing.h"


struct CassoTheme;
class DxuiHostWindow;




enum class InputFocusStop
{
    AllCheck,
    EmuKeyboardCheck,
    JoystickCheck,
    PaddleCheck,
    HostKeyboardCheck,
    Pair0Dropdown,
    Pair1Dropdown,
    PauseButton,
    ClearButton,
    CopyButton,
    EventList,
};





////////////////////////////////////////////////////////////////////////////////
//
//  InputDebugPanel
//
////////////////////////////////////////////////////////////////////////////////

class InputDebugPanel : public DxuiPanel,
                        public IChromedPanelContent,
                        public IInputEventSink
{
public:
    InputDebugPanel  ();
    ~InputDebugPanel () override;

    HRESULT Create  (HINSTANCE              hInstance,
                     HWND                   hwndOwner,
                     ID3D11Device         * device,
                     ID3D11DeviceContext  * context,
                     const CassoTheme    * theme);
    void    Show    ();
    void    Hide    ();
    void    Destroy ();

    bool    IsOpen () const { return m_window.IsOpen(); }
    HWND    Hwnd   () const { return m_window.Hwnd(); }

    HRESULT RenderFrame ();
    void    SetTheme    (const CassoTheme * theme);
    void    SetCycleCounter (const uint64_t * cycleCounter) { m_cycleCounter = cycleCounter; }
    void    SetUptimeAnchor (std::chrono::steady_clock::time_point anchor) { m_uptimeAnchor = anchor; }
    void    ClearEvents     ();

    // Thread-safe reset hook. The CPU/reset thread calls this to stage a
    // new Uptime anchor and request an event-list clear; the render
    // thread applies both inside DrainAndProject so the event deque and
    // DxuiListView rows are only ever mutated on one thread.
    void    RequestResetAnchor (std::chrono::steady_clock::time_point anchor) noexcept;

    LPCWSTR  GetWindowClassName () const override;
    LPCWSTR  GetWindowTitle     () const override;
    HRESULT  OnHostCreated      (HWND                   hwnd,
                                 ID3D11Device         * device,
                                 ID3D11DeviceContext  * context,
                                 int                    widthPx,
                                 int                    heightPx,
                                 UINT                   dpi,
                                 DxuiHostWindow       * captionHost,
                                 const CassoTheme    * theme) override;
    void     OnHostDestroyed    ()                                  override;
    HRESULT  OnHostResize       (int widthPx, int heightPx, UINT dpi) override;
    void     SetChromeTheme     (DxuiHostWindow * captionHost, const CassoTheme * theme) override;
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

    void OnKbdDataRead    (Word address, Byte value, bool strobeSet)    override;
    void OnKbdStrobe      (Word address, Byte value, bool clearedStrobe) override;
    void OnButtonRead     (Word address, Byte value)                    override;
    void OnPaddleTrigger  (Word address)                                override;
    void OnPaddleRead     (Word address, Byte value)                    override;
    void OnHostAutoRepeat (Byte asciiChar)                              override;
    void OnHostKeyDown    (Byte asciiChar)                              override;
    void OnHostKeyUp      (Byte asciiChar)                              override;
    void OnHostPaddle     (int axis, Byte value)                        override;
    void OnHostButton     (int index, bool down)                        override;

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
    void    AppendNewEventRows   (size_t startIndex);
    void    PublishToRing        (const InputEvent & e);
    InputEvent  MakeStampedEvent (InputEventCategory cat, InputEventType type) const noexcept;
    void    OnFilterChanged      ();
    void    OnPairViewChanged    (int pair, int index);
    void    UpdatePairVisibility ();
    void    SyncAllCheck         ();
    void    ApplyAllToggle       ();
    void    UpdatePauseLabel     ();
    void    CopyEventsToClipboard ();
    void    UpdateTooltip        (int x, int y);
    void    ShowColumnMenu       (int anchorX, int anchorY);
    void    FocusCycle           (int direction);
    void    RebuildFocusOrder    ();
    void    SetFocusToStop       (InputFocusStop stop);
    void    ApplyFocus           ();
    void    ClearAllWidgetFocus  ();
    void    ApplyListSelection   ();
    void    OnListSelectionMoved ();
    void    OnHeaderSortKey      ();
    void    OnDividerResizeKey   (int direction);
    void    SortByColumn         (int absCol);
    void    ApplySort            ();
    int64_t NowMs                () const;

    static void               ArgbToFloat4              (uint32_t argb, float (& outRgba)[4]) noexcept;
    static void               FormatCycleWithSeparators (uint64_t value, wchar_t * out, size_t cap);
    static void               FormatWallNow             (wchar_t * out, size_t cap);
    static void               FormatUptime              (std::chrono::steady_clock::time_point anchor,
                                                         wchar_t * out,
                                                         size_t cap);
    static wchar_t            PrintableChar             (Byte value) noexcept;
    static std::wstring       FormatByteChar            (Byte value);
    static std::wstring       SourceLabel               (InputEventCategory category);
    static LPCWSTR            ButtonAnnotation          (Word address) noexcept;
    static InputGamePortClass ClassifyGamePort          (InputEventType type, Word address) noexcept;
    static void               FormatInputEvent          (const InputEvent & src,
                                                         std::chrono::steady_clock::time_point uptimeAnchor,
                                                         InputEventDisplay & out);
    static void               ProjectOne                (const InputEvent & src,
                                                         std::deque<InputEventDisplay> & deque,
                                                         std::chrono::steady_clock::time_point uptimeAnchor);

    ChromedPanelWindow                    m_window;
    InputPanelLayoutSlots                 m_layout = {};

    ID3D11Device                        * m_device   = nullptr;
    ID3D11DeviceContext                 * m_context  = nullptr;
    const CassoTheme                   * m_theme    = nullptr;
    DxuiHostWindow                      * m_captionHost = nullptr;
    HWND                                  m_hwnd     = nullptr;
    int                                   m_widthPx  = 0;
    int                                   m_heightPx = 0;
    UINT                                  m_dpi      = 96;

    Microsoft::WRL::ComPtr<IDXGISwapChain1>         m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_rtv;

    DxuiPainter                           m_painter;
    DxuiTextRenderer                    m_text;

    DxuiLabel                                 m_emuLabel;
    DxuiLabel                                 m_hostLabel;
    std::array<DxuiLabel, 2>                  m_pairLabel;
    DxuiCheckbox                              m_allCheck;
    DxuiCheckbox                              m_emuKeyboardCheck;
    DxuiCheckbox                              m_joystickCheck;
    DxuiCheckbox                              m_paddleCheck;
    DxuiCheckbox                              m_hostKeyboardCheck;
    std::array<DxuiDropdown, 2>               m_pairView;
    DxuiButton                                m_pauseButton;
    DxuiButton                                m_clearButton;
    DxuiButton                                m_copyButton;
    DxuiListView                              m_eventList;
    DxuiTooltip                               m_tooltip;
    DxuiPopupMenu                             m_columnMenu;

    InputFilterState                      m_filter;
    std::array<InputLogicalColumn, kInputColumnCount>  m_columnsModel = {};
    InputEventRing                        m_ring;
    std::deque<InputEventDisplay>         m_events;
    std::vector<size_t>                   m_filteredIndices;
    std::vector<InputEvent>               m_pendingHostEvents;
    std::atomic<uint32_t>                 m_droppedSinceLastDrain = 0;
    std::atomic<bool>                     m_resetAnchorPending    = false;
    std::atomic<int64_t>                  m_pendingAnchorTicks    = 0;
    const uint64_t                      * m_cycleCounter = nullptr;
    std::chrono::steady_clock::time_point m_uptimeAnchor;
    bool                                  m_paused         = false;
    int                                   m_sortColumn     = -1;
    bool                                  m_sortDescending = false;

    int                                   m_resizeColumn       = -1;
    int                                   m_resizeStartXPx     = 0;
    int                                   m_resizeStartWidthPx = 0;
    std::vector<InputFocusStop>           m_focusStops;
    int                                   m_focusIndex         = -1;
    bool                                  m_joystickVisible    = true;
    bool                                  m_paddleVisible      = false;
    int                                   m_listSelectedEventIndex = -1;
};
