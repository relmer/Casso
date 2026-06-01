#pragma once

#include "Chrome/ChromedPanelWindow.h"
#include "Chrome/IChromedPanelContent.h"
#include "InputDebugPanelLayout.h"
#include "DxUiPainter.h"
#include "DwriteTextRenderer.h"
#include "Widgets/Button.h"
#include "Widgets/Checkbox.h"
#include "Widgets/Dropdown.h"
#include "Widgets/Label.h"
#include "Widgets/ListView.h"
#include "Widgets/PopupMenu.h"
#include "Widgets/Tooltip.h"

#include "../InputDebugDialogState.h"
#include "../InputEventDisplay.h"
#include "../../CassoEmuCore/Devices/IInputEventSink.h"
#include "../../CassoEmuCore/Devices/InputEventRing.h"


struct ChromeTheme;
class TitleBar;




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
    EventList,
};





////////////////////////////////////////////////////////////////////////////////
//
//  InputDebugPanel
//
////////////////////////////////////////////////////////////////////////////////

class InputDebugPanel : public IChromedPanelContent,
                        public IInputEventSink
{
public:
    InputDebugPanel  ();
    ~InputDebugPanel () override;

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
    void    ClearEvents     ();

    // Thread-safe reset hook. The CPU/reset thread calls this to stage a
    // new Uptime anchor and request an event-list clear; the render
    // thread applies both inside DrainAndProject so the event deque and
    // ListView rows are only ever mutated on one thread.
    void    RequestResetAnchor (std::chrono::steady_clock::time_point anchor) noexcept;

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

    void OnKbdDataRead    (Word address, Byte value, bool strobeSet)    override;
    void OnKbdStrobe      (Word address, Byte value, bool clearedStrobe) override;
    void OnButtonRead     (Word address, Byte value)                    override;
    void OnPaddleTrigger  (Word address)                                override;
    void OnPaddleRead     (Word address, Byte value)                    override;
    void OnHostAutoRepeat (Byte asciiChar)                              override;
    void OnHostKeyDown    (Byte asciiChar)                              override;
    void OnHostKeyUp      (Byte asciiChar)                              override;

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
    void    PublishToRing        (const InputEvent & e);
    InputEvent  MakeStampedEvent (InputEventCategory cat, InputEventType type) const noexcept;
    void    OnFilterChanged      ();
    void    OnPairViewChanged    (int pair, int index);
    void    UpdatePairVisibility ();
    void    SyncAllCheck         ();
    void    ApplyAllToggle       ();
    void    UpdatePauseLabel     ();
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
    int64_t NowMs                () const;

    ChromedPanelWindow                    m_window;
    InputPanelLayoutSlots                 m_layout = {};

    ID3D11Device                        * m_device   = nullptr;
    ID3D11DeviceContext                 * m_context  = nullptr;
    const ChromeTheme                   * m_theme    = nullptr;
    TitleBar                            * m_titleBar = nullptr;
    HWND                                  m_hwnd     = nullptr;
    int                                   m_widthPx  = 0;
    int                                   m_heightPx = 0;
    UINT                                  m_dpi      = 96;

    Microsoft::WRL::ComPtr<IDXGISwapChain1>         m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_rtv;

    DxUiPainter                           m_painter;
    DwriteTextRenderer                    m_text;

    Label                                 m_emuLabel;
    Label                                 m_hostLabel;
    std::array<Label, 2>                  m_pairLabel;
    Checkbox                              m_allCheck;
    Checkbox                              m_emuKeyboardCheck;
    Checkbox                              m_joystickCheck;
    Checkbox                              m_paddleCheck;
    Checkbox                              m_hostKeyboardCheck;
    std::array<Dropdown, 2>               m_pairView;
    Button                                m_pauseButton;
    Button                                m_clearButton;
    ListView                              m_eventList;
    Tooltip                               m_tooltip;
    PopupMenu                             m_columnMenu;

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
