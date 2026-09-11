#pragma once

#include "Pch.h"

#include "Core/DxuiCommand.h"
#include "Core/IDxuiControl.h"
#include "Devices/Printer/PrinterStatusModel.h"   // PrinterStatus
#include "Ui/UiCommandTypes.h"                       // InputMappingMode
#include "Widgets/DxuiSlider.h"
#include "Widgets/DxuiToolbar.h"


class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar
//
//  The main window's command toolbar: a chrome strip below the menu bar
//  carrying the most-used commands as icon + label buttons -- Settings, the
//  theme and monitor-color pickers, Printer (with its status LED), the
//  master Volume slider + Mute, the input devices, Fullscreen, Screenshot,
//  Reset, and Power.
//
//  The strip itself is a DxuiToolbar. This class is what is left of the
//  emulator's side of it: the command table, the picker rows, the printer
//  light, the volume flyout's slider and the input cluster, wired into the
//  widget and presented to the shell through the surface the shell already
//  calls. It is a wrapper on its way out; the parts it still carries move
//  into their own files next.
//
//  Input is hand-routed by EmulatorShell: the shell forwards mouse events to
//  OnToolbarMouseMove / OnToolbarLButtonDown / OnToolbarLButtonUp. Volume
//  changes surface through the VolumeFn sink as (volume01, muted).
//
////////////////////////////////////////////////////////////////////////////////

class CommandToolbar : public IDxuiControl
{
public:
    using DispatchFn = std::function<void (WORD)>;
    using VolumeFn   = std::function<void (float, bool)>;
    using InputFn    = std::function<void (InputMappingMode)>;
    using ChoiceFn   = std::function<void (int index)>;

    CommandToolbar  ();
    ~CommandToolbar () override = default;

    // Decides how many entries can still afford their label at this width and
    // returns the band thickness (dp) the strip needs. The shell calls this
    // BEFORE docking the chrome bands.
    int   PlanForWidth     (int clientWidthPx, const DxuiDpiScaler & scaler);
    int   GetBandDp        () const;

    // The hovered entry's tooltip, and the rect to anchor it on. Returns
    // nullptr when nothing should show.
    const wchar_t *  GetTooltipAt (int x, int y, RECT & anchor) const;

    // The DWrite renderer used to measure labels during Layout (the shell's
    // chrome text renderer; must outlive this control).
    void  SetTextRenderer   (IDxuiTextRenderer * text)   { m_textRenderer = text; m_toolbar.SetTextRenderer (text); }

    void  SetDispatch       (DispatchFn fn)              { m_dispatch = std::move (fn); }
    void  SetVolumeSink     (VolumeFn fn)                { m_volumeSink = std::move (fn); }

    // The machine the Reset / Power tooltips talk about ("Apple //e"), and
    // the presentation the fullscreen button offers to leave or enter.
    void  SetMachineDisplayName (const std::wstring & displayName);
    void  SetFullscreen         (bool fullscreen);

    // Theme picker: display names in the order the shell holds their ids,
    // and the row the active theme sits on. Monitor color is the fixed
    // Color / Green / Amber / White set the Settings picture list carries.
    void  SetThemes            (const std::vector<std::wstring> & displayNames, int activeIndex);
    void  SetThemeIndex        (int index);
    void  SetMonitorColorIndex (int index);

    // Both pickers preview while their menu is open and settle when it
    // closes. Preview applies without persisting, because a highlight is
    // not a choice; commit is the user's pick. A dismissed menu replays
    // preview with the row it opened on, which is the snap-back.
    void  SetThemeSinks        (ChoiceFn preview, ChoiceFn commit);
    void  SetMonitorSinks      (ChoiceFn preview, ChoiceFn commit);

    // Routes the picker menus through the host's popup-window pool so they
    // escape the strip and hang over the emulator viewport, and supplies the
    // client rect they are kept inside.
    void  SetPopupHost         (DxuiHwndSource * host)   { m_toolbar.SetPopupHost (host); }
    void  SetHostClientRect    (const RECT & clientRect) { m_toolbar.SetHostClientRect (clientRect); }

    // An open menu owns the keyboard: the shell hands it every keydown so
    // arrowing through the rows previews instead of typing into the guest.
    bool  IsMenuOpen           () const                  { return m_toolbar.IsMenuOpen(); }
    bool  HandleKey            (WPARAM vk)               { return m_toolbar.HandleKey (vk); }

    // Input-mode entry: one "Input" label over three LED + glyph segments
    // (joystick / paddle / mouse) while it has room, and a single icon with
    // the same three as a checkable menu once it does not. Clicking either
    // reports the mode to toggle; state arrives per frame.
    void  SetInputSink       (InputFn fn)                { m_inputSink = std::move (fn); }
    void  SetInputState      (bool arrowsJoystick, InputMappingMode pointer, bool mouseAvailable);
    void  SetInputSkeuoStyle (bool skeuo)                { m_inputSkeuo = skeuo; }

    // Monoline icon experiment: strokes-and-dots device glyphs matching the
    // Segoe MDL2 language of the bar's other icons, instead of the shaded
    // skeuomorphic drawings. On by default so the bar reads as one icon set;
    // flip off to compare against the peripheral renderings.
    void  SetInputMonoline   (bool monoline)             { m_inputMonoline = monoline; }

    // The volume flyout (vertical slider + readout) opens on hover over the
    // volume button and closes when the pointer leaves button + flyout.
    // Exposed so the shell can keep presenting frames while it is up.
    bool  IsVolumeFlyoutOpen () const                    { return m_toolbar.IsFlyoutOpen (s_kIdVolume); }

    // Seed the volume controls from persisted prefs (no sink callback).
    void  SetVolume         (float volume01, bool muted);
    float GetVolume         () const                     { return m_volume01; }
    bool  IsMuted           () const                     { return m_muted; }

    void  SetPrinterStatus  (PrinterStatus status)       { m_printerStatus = status; }
    void  SetPrinterPresent (bool present)               { m_printerPresent = present; }

    // Shell-forwarded mouse input. Return true when the event was consumed
    // (over a button, or the slider is tracking a drag).
    bool  OnToolbarMouseMove   (int x, int y, bool leftDown);
    void  OnToolbarMouseLeave  ()                        { m_toolbar.OnToolbarMouseLeave(); }
    bool  OnToolbarLButtonDown (int x, int y)            { return m_toolbar.OnToolbarLButtonDown (x, y); }
    bool  OnToolbarLButtonUp   (int x, int y)            { return m_toolbar.OnToolbarLButtonUp (x, y); }

    bool  HitTest           (int x, int y) const         { return m_toolbar.HitTest (x, y); }

    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

private:
    // The entries, in the order they sit on the strip. The order is also the
    // COLLAPSE order read backwards: the last entry gives up its label first.
    enum class Entry
    {
        Settings   = 0,
        Theme,
        Color,
        Printer,
        Volume,
        Input,
        Fullscreen,
        Screenshot,
        Reset,
        Power,
        Count,
    };

    // Ids for the entries that are not commands of the menu's: the pickers
    // and the flyout. Menu command ids start at 40001, so nothing collides.
    static constexpr int  s_kIdTheme  = 1;
    static constexpr int  s_kIdColor  = 2;
    static constexpr int  s_kIdVolume = 3;
    static constexpr int  s_kIdInput  = 4;

    // One input segment: LED + peripheral glyph, no label of its own (the
    // entry's shared label + per-segment tooltips carry the names).
    struct InputSeg
    {
        RECT  rc      = {};
        bool  hovered = false;
        bool  pressed = false;
    };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  CommandToolbar::InputCluster
    //
    //  The input entry as the widget sees it: a shared "Input" label over LED
    //  + glyph segments while expanded, one drawn icon with a light while
    //  collapsed. Everything it does is the toolbar's own state, so it is a
    //  view onto its owner rather than a thing of its own.
    //
    ////////////////////////////////////////////////////////////////////////////

    class InputCluster : public IDxuiToolbarCustomEntry
    {
    public:
        explicit InputCluster (CommandToolbar & owner) : m_owner (owner) {}

        int              GetWidthPx    (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const override;
        void             Layout        (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler) override;
        void             Paint         (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme,
                                        bool hovered, bool pressed, bool labeled) override;
        const wchar_t *  GetTooltipAt  (int x, int y, RECT & anchor) const override;
        bool             OnClick       (int x, int y) override;
        bool             OnMouseMove   (int x, int y) override;
        void             OnMouseLeave  () override;
        bool             OnLButtonDown (int x, int y) override;

    private:
        CommandToolbar &  m_owner;
    };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  CommandToolbar::VolumePanel
    //
    //  The slider as the flyout hosts it. A muted slider is inert, so a press
    //  on it falls through to the bar rather than starting a drag that changes
    //  a value nobody can hear.
    //
    ////////////////////////////////////////////////////////////////////////////

    class VolumePanel : public IDxuiControl
    {
    public:
        explicit VolumePanel (CommandToolbar & owner) : m_owner (owner) {}

        void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
        void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
        bool  OnMouse (const DxuiMouseEvent & ev) override;

    private:
        CommandToolbar &  m_owner;
    };


    static bool      IsPointInRect      (const RECT & rc, int x, int y);
    static uint32_t  GetStatusCoreColor (PrinterStatus status);
    static void      PaintStatusLed     (IDxuiPainter & painter, float cx, float cy, UINT dpi, uint32_t core);

    DxuiCommand       &  GetCommand  (Entry entry)       { return m_commands[(size_t) entry]; }
    const DxuiCommand &  GetCommand  (Entry entry) const { return m_commands[(size_t) entry]; }

    void  BuildCommands       ();
    void  BuildEntries        ();
    void  RebuildActionTips   ();
    void  RebuildInputRows    ();
    void  RebuildThemeRows    ();
    void  RebuildColorRows    ();
    void  ToggleMute          ();

    int   MeasureLabelPx      (const wchar_t * text, float fontPx) const;
    int   GetInputWidthPx     (bool labeled, UINT dpi) const;
    void  LayoutInput         (const RECT & rc, bool labeled, UINT dpi);
    void  PaintInputCluster   (IDxuiPainter & painter, IDxuiTextRenderer & text,
                               const struct CassoTheme & theme, const RECT & rc);
    void  PaintInputCollapsed (IDxuiPainter & painter, const struct CassoTheme & theme, const RECT & rc);
    void  PaintPrinterLed     (IDxuiPainter & painter, const DxuiToolbarIconBox & icon);

    // A circle outline as line segments -- the painter has filled circles
    // and lines, but no arcs or outlined circles.
    static void      StrokeCircle      (IDxuiPainter & painter, float cx, float cy,
                                        float r, float stroke, uint32_t ink);

    // The pen every device glyph draws with, for a box `w` pixels wide: the
    // weight that sits level with the MDL2 glyphs beside them.
    static float     GetGlyphStroke    (float w);

    // Joystick and paddle only: the mouse segment draws MDL2's own glyph.
    static void      PaintJoystickMono (IDxuiPainter & painter, const RECT & box, uint32_t ink);
    static void      PaintPaddleMono   (IDxuiPainter & painter, const RECT & box, uint32_t ink);

    int              InputSegCount     () const { return m_mouseAvailable ? 3 : 2; }
    bool             InputSegSelected  (int index) const;
    bool             IsInputExpanded   () const { return m_toolbar.IsLabeled (s_kIdInput); }

    DxuiToolbar                  m_toolbar;
    std::vector<DxuiCommand>     m_commands;       // indexed by Entry, in visual order
    std::vector<DxuiCommand>     m_themeRows;
    std::vector<DxuiCommand>     m_colorRows;
    std::vector<DxuiCommand>     m_inputRows;
    InputCluster                 m_inputCluster;
    VolumePanel                  m_volumePanel;
    DxuiSlider                   m_volumeSlider;   // vertical, lives in the flyout

    InputSeg              m_inputSegs[3];   // joystick, paddle, mouse
    RECT                  m_inputRc        = {};
    RECT                  m_inputLabelRc   = {};
    InputFn               m_inputSink;
    bool                  m_arrowsJoystick = false;
    InputMappingMode      m_pointerMode    = InputMappingMode::Off;
    bool                  m_mouseAvailable = false;
    bool                  m_inputSkeuo     = true;
    bool                  m_inputMonoline  = true;

    std::vector<std::wstring>  m_themeNames;
    int                        m_themeIndex = -1;
    int                        m_colorIndex = 0;

    std::wstring          m_machineName;
    bool                  m_fullscreen     = false;

    IDxuiTextRenderer *   m_textRenderer   = nullptr;
    DispatchFn            m_dispatch;
    VolumeFn              m_volumeSink;

    UINT                  m_dpi            = 96;
    float                 m_volume01       = 1.0f;
    bool                  m_muted          = false;
    PrinterStatus         m_printerStatus  = PrinterStatus::Idle;
    bool                  m_printerPresent = false;
};
