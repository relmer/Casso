#pragma once

#include "Window/DxuiWindow.h"
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
//  Themed DX replacement for the legacy Win32 InputDebugDialog. Derives
//  from DxuiWindow, so it IS its own content-root panel AND owns the OS
//  window (HWND + swap chain + caption + paint pump) through the base
//  class -- the subclass never touches an HWND, a WPARAM, or a host
//  client interface. It still implements the same event-sink interface
//  (IInputEventSink) so it slots into the existing EmulatorShell event
//  wiring with no contract changes.
//
//  Content widgets are created as children of this panel in OnCreate
//  (via the inherited CreateChild<T> factory) so the base paint pump
//  walks and paints them; the panel keeps its own focus manager,
//  tooltip, and column menu (the latter two escape the client via the
//  host popup pool exposed through PopupHost()).
//
////////////////////////////////////////////////////////////////////////////////

class InputDebugPanel : public DxuiWindow,
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
    void    Destroy ();

    bool    IsOpen () const { return IsCreated(); }

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

    // Framework input entry points. These DxuiPanel overrides own all
    // mouse / keyboard routing for the panel: they hit-test and dispatch
    // the DxuiMouseEvent / DxuiKeyEvent straight to the child widgets and
    // the event list, so the host drives the panel purely through the
    // framework.
    bool    OnMouse (const DxuiMouseEvent & ev)                     override;
    bool    OnKey   (const DxuiKeyEvent   & ev)                     override;

    // DxuiPanel cursor hook. The generic panel fan-out hands children
    // client-px, but DxuiListView::CursorForPoint expects list-local
    // coords, so translate before delegating (and hold the resize cursor
    // through an active column drag even if the pointer drifts off the
    // header strip).
    LPCWSTR CursorForPoint (POINT clientPx) const                   override;

    // DxuiPanel layout hook. DxuiWindow calls this with the client
    // bounds / DPI scaler after the OS window resizes; caches the size
    // and re-runs the panel's absolute layout so the child widgets track
    // the new bounds.
    void    Layout  (const RECT          & boundsDip,
                     const DxuiDpiScaler & scaler)                  override;

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

protected:
    // DxuiWindow hook. Fires inside Create() once the backend + HWND
    // exist; populates the child widgets via the inherited CreateChild<T>
    // factory and wires their state / callbacks.
    void    OnCreate ()                                             override;

private:
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

    InputPanelLayoutSlots                 m_layout = {};

    const CassoTheme                   * m_theme    = nullptr;
    int                                   m_widthPx  = 0;
    int                                   m_heightPx = 0;
    UINT                                  m_dpi      = 96;
    DxuiDpiScaler                             m_scaler;

    DxuiLabel                               * m_emuLabel          = nullptr;
    DxuiLabel                               * m_hostLabel         = nullptr;
    std::array<DxuiLabel*, 2>                 m_pairLabel         = {};
    DxuiCheckbox                            * m_allCheck          = nullptr;
    DxuiCheckbox                            * m_emuKeyboardCheck  = nullptr;
    DxuiCheckbox                            * m_joystickCheck     = nullptr;
    DxuiCheckbox                            * m_paddleCheck       = nullptr;
    DxuiCheckbox                            * m_hostKeyboardCheck = nullptr;
    std::array<DxuiDropdown*, 2>              m_pairView          = {};
    DxuiButton                              * m_pauseButton       = nullptr;
    DxuiButton                              * m_clearButton       = nullptr;
    DxuiButton                              * m_copyButton        = nullptr;
    DxuiListView                            * m_eventList         = nullptr;
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
