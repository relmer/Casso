#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemSettings
//
//  The Windows interaction settings a widget has to obey to behave like the
//  rest of the desktop: whether animations are wanted at all, whether access
//  keys are underlined before Alt is pressed, how long a submenu waits before
//  it opens, how far a wheel notch scrolls, how long a notification stays
//  up, how far apart and how close in time two clicks are a double-click,
//  how far the pointer moves before a press is a drag, how tall the cursor
//  is, and how fast the caret blinks.
//
//  Three of these are ACCESSIBILITY settings, not preferences. Animations off
//  is set by people who get motion sick; a long message duration is set by
//  people who cannot read a four-second banner. Ignoring them is not a
//  cosmetic difference to the user who turned them on.
//
//  Every value is read once and cached, since widgets ask on every frame and
//  these are system-parameters calls. `Refresh` re-reads them all; call it
//  from `WM_SETTINGCHANGE`. A query that fails leaves the Windows default,
//  which is what the failed call would have returned.
//
//  Sizes and fonts are NOT here. Menu geometry lives in `DxuiMenuMetrics`,
//  which is per-DPI and so cannot be a process-wide singleton.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiSystemSettings
{
public:
    //  What `SPI_GETWHEELSCROLLLINES` returns when the user picked "one
    //  screen at a time" rather than a line count.
    static constexpr int  kWheelPageScroll = -1;

    //  The system-parameters query, replaceable so a test can supply values
    //  without touching the real settings. Null restores SystemParametersInfoW.
    using ParameterReader = BOOL (WINAPI *) (UINT action, UINT param, PVOID pvParam, UINT winIni);

    //  The system-metrics query and the two time queries, replaceable the
    //  same way. A zero from any of them is the Windows default. Null
    //  restores GetSystemMetrics, GetDoubleClickTime and GetCaretBlinkTime.
    using MetricReader = int  (WINAPI *) (int index);
    using TimeReader   = UINT (WINAPI *) ();

    static DxuiSystemSettings & Instance();

    void  Refresh            ();
    void  SetParameterReader (ParameterReader reader);
    void  SetMetricReaders   (MetricReader metric, TimeReader doubleClickTime, TimeReader caretBlinkTime);

    //  "Animation effects" in Settings > Accessibility > Visual effects.
    //  False means play no animation: jump to the finished state.
    bool  AreAnimationsEnabled    () const { return m_animations; }

    //  Menu reveal, from Performance Options > "Fade or slide menus into
    //  view". Animations are off entirely when the accessibility master
    //  switch is off, so that is folded in here rather than left for every
    //  caller to remember.
    bool  AreMenuAnimationsEnabled () const { return m_animations && m_menuAnimation; }

    //  Which reveal Windows would use. Casso slides regardless; this is kept
    //  so the choice can follow the system instead if that is wanted.
    bool  PrefersMenuFade         () const { return m_menuFade; }

    //  "Underline access keys" -- when true the mnemonic cues show without
    //  waiting for Alt.
    bool  AlwaysShowKeyboardCues  () const { return m_keyboardCues; }

    int   GetMenuShowDelayMs      () const { return m_menuShowDelayMs;   }
    int   GetMessageDurationMs    () const { return m_messageDurationMs; }

    //  Lines per wheel notch, or `kWheelPageScroll` for a screen at a time.
    int   GetWheelLinesPerNotch   () const { return m_wheelLines;  }
    int   GetWheelCharsPerNotch   () const { return m_wheelChars;  }

    //  Two clicks within this time and this rect, in pixels, are a
    //  double-click; a press that moves out of the drag rect is a drag.
    int   GetDoubleClickMs        () const { return m_doubleClickMs; }
    SIZE  GetDoubleClickSizePx    () const { return m_doubleClickPx; }
    SIZE  GetDragSizePx           () const { return m_dragPx;        }

    //  The cursor's height in pixels, which a tip opens below.
    int   GetCursorHeightPx       () const { return m_cursorHeightPx; }

    //  The caret's half period, or INFINITE when it doesn't blink.
    UINT  GetCaretBlinkMs         () const { return m_caretBlinkMs; }

private:
    DxuiSystemSettings();

    static constexpr bool  kDefaultMenuAnimation   = true;
    static constexpr bool  kDefaultMenuFade        = true;
    static constexpr bool  kDefaultAnimations      = true;
    static constexpr bool  kDefaultKeyboardCues    = false;
    static constexpr int   kDefaultMenuShowDelayMs = 400;
    static constexpr int   kDefaultMessageSeconds  = 5;
    static constexpr int   kDefaultWheelLines      = 3;
    static constexpr int   kDefaultWheelChars      = 3;
    static constexpr int   kMsPerSecond            = 1000;
    static constexpr int   kDefaultDoubleClickMs   = 500;
    static constexpr int   kDefaultDoubleClickPx   = 4;
    static constexpr int   kDefaultDragPx          = 4;
    static constexpr int   kDefaultCursorHeightPx  = 32;
    static constexpr UINT  kDefaultCaretBlinkMs    = 530;

    bool  ReadFlag  (UINT action, bool fallback) const;
    int   ReadUint  (UINT action, int fallback) const;
    int   ReadWheel (UINT action, int fallback) const;
    int   ReadMetric (int index, int fallback) const;

    bool  m_animations        = kDefaultAnimations;
    bool  m_menuAnimation     = kDefaultMenuAnimation;
    bool  m_menuFade          = kDefaultMenuFade;
    bool  m_keyboardCues      = kDefaultKeyboardCues;
    int   m_menuShowDelayMs   = kDefaultMenuShowDelayMs;
    int   m_messageDurationMs = kDefaultMessageSeconds * kMsPerSecond;
    int   m_wheelLines        = kDefaultWheelLines;
    int   m_wheelChars        = kDefaultWheelChars;
    int   m_doubleClickMs     = kDefaultDoubleClickMs;
    SIZE  m_doubleClickPx     = { kDefaultDoubleClickPx, kDefaultDoubleClickPx };
    SIZE  m_dragPx            = { kDefaultDragPx, kDefaultDragPx };
    int   m_cursorHeightPx    = kDefaultCursorHeightPx;
    UINT  m_caretBlinkMs      = kDefaultCaretBlinkMs;

    ParameterReader  m_pfnRead             = SystemParametersInfoW;
    MetricReader     m_pfnReadMetric       = GetSystemMetrics;
    TimeReader       m_pfnReadDoubleClick  = GetDoubleClickTime;
    TimeReader       m_pfnReadCaretBlink   = GetCaretBlinkTime;
};
