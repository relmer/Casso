#pragma once

#include "Window/DxuiHostWindow.h"
#include "Window/IDxuiHostClient.h"
#include "InputDebugPanelLayout.h"
#include "Core/DxuiFocusManager.h"
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





////////////////////////////////////////////////////////////////////////////////
//
//  InputDebugPanel
//
////////////////////////////////////////////////////////////////////////////////

class InputDebugPanel : public DxuiPanel,
                        public IDxuiHostClient,
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

    bool    IsOpen () const { return m_host != nullptr; }
    HWND    Hwnd   () const { return m_host != nullptr ? m_host->Hwnd() : nullptr; }

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

    // Framework input entry points. These DxuiPanel overrides own all
    // mouse / keyboard routing for the panel: they hit-test and dispatch
    // the DxuiMouseEvent / DxuiKeyEvent straight to the child widgets and
    // the event list, so the host drives the panel purely through the
    // framework.
    bool    OnMouse (const DxuiMouseEvent & ev)                     override;
    bool    OnKey   (const DxuiKeyEvent   & ev)                     override;

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
    DxuiMessageResult  DispatchClientMouse (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, float wheelDelta);
    DxuiMessageResult  DispatchClientKey   (DxuiKeyEventKind kind, WPARAM code);
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
    void    ApplyListSelection   ();
    void    OnListSelectionMoved ();
    bool    ForwardMouseToList   (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, float wheelDelta);
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
                                                         const InputFilterState & filter,
                                                         InputEventDisplay & out);
    static void               ProjectOne                (const InputEvent & src,
                                                         std::deque<InputEventDisplay> & deque,
                                                         std::chrono::steady_clock::time_point uptimeAnchor,
                                                         const InputFilterState & filter);

    std::unique_ptr<DxuiHostWindow>       m_host;
    InputPanelLayoutSlots                 m_layout = {};

    ID3D11Device                        * m_device   = nullptr;
    ID3D11DeviceContext                 * m_context  = nullptr;
    const CassoTheme                   * m_theme    = nullptr;
    HWND                                  m_hwnd     = nullptr;
    int                                   m_widthPx  = 0;
    int                                   m_heightPx = 0;
    UINT                                  m_dpi      = 96;

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
    DxuiFocusManager                          m_focusMgr;

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

    bool                                  m_joystickVisible    = true;
    bool                                  m_paddleVisible      = false;
    int                                   m_listSelectedEventIndex = -1;
};
