#pragma once

#include "Pch.h"

#include "Core/DxuiCommand.h"
#include "Ui/UiCommandTypes.h"      // InputMappingMode
#include "Widgets/DxuiToolbar.h"



struct CassoTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry
//
//  The toolbar's input-devices entry. Expanded, it is one shared "Input"
//  label over a row of LED + glyph segments (joystick, paddle, and mouse
//  when the machine has one), each a toggle for its mode. Collapsed, it is
//  a single drawn joystick with a light that says SOMETHING is mapped, and
//  the toolbar opens the same three modes as a checkable menu, built from
//  the three commands this entry owns.
//
//  A segment click reports the mode through the InputFn sink; state arrives
//  per frame through SetInputState.
//
////////////////////////////////////////////////////////////////////////////////

class InputClusterEntry : public IDxuiToolbarCustomEntry
{
public:
    using InputFn = std::function<void (InputMappingMode)>;

    InputClusterEntry ();

    void  SetSink         (InputFn fn)                 { m_sink = std::move (fn); }
    void  SetTextRenderer (IDxuiTextRenderer * text)   { m_textRenderer = text; }

    // Skeuomorphic vs top-down peripheral drawings, used only when the
    // monoline set is off; monoline is on by default so the bar reads as
    // one icon set.
    void  SetSkeuoStyle   (bool skeuo)                 { m_skeuo = skeuo; }
    void  SetMonoline     (bool monoline)              { m_monoline = monoline; }

    // Returns true when the segment COUNT changed, which is when the entry
    // must be laid out again and its picker list handed to the toolbar anew.
    bool  SetInputState   (bool arrowsJoystick, InputMappingMode pointer, bool mouseAvailable);

    bool  IsExpanded      () const                     { return m_labeled; }

    // Only the mouse is shown here now. The joystick and the paddle moved
    // onto the command bar's paddle-source picker, which wears whichever of
    // them is driving on its face, so a pair of icons saying the same thing
    // two entries away was the same answer twice.
    int   GetSegmentCount () const                     { return m_mouseAvailable ? 1 : 0; }

    // Display position -> the mode slot it draws and dispatches. The slots
    // stay as they were (0 joystick, 1 paddle, 2 mouse) because the modes,
    // their labels and IsSegmentOn are all indexed by them.
    int   GetSlotAt       (int index) const;
    bool  IsSegmentOn     (int index) const;

    // The three modes as commands with `isChecked` reading the current
    // state; the first GetSegmentCount() of them, held by pointer.
    std::vector<DxuiPopupMenuItem>  GetPickerItems () const;

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
    // One segment: LED + peripheral glyph, no label of its own (the entry's
    // shared label + per-segment tooltips carry the names).
    struct Segment
    {
        RECT  rc      = {};
        bool  hovered = false;
        bool  pressed = false;
    };

    static bool   IsPointInRect     (const RECT & rc, int x, int y);
    static int    MeasureLabelPx    (IDxuiTextRenderer * text, const wchar_t * label, float fontPx);

    // The pen every device glyph draws with, for a box `w` pixels wide: the
    // weight that sits level with the MDL2 glyphs beside them.
    static float  GetGlyphStroke    (float w);

    // A circle outline as line segments -- the painter has filled circles
    // and lines, but no arcs or outlined circles.
    static void   StrokeCircle      (IDxuiPainter & painter, float cx, float cy, float r, float stroke, uint32_t ink);

    // Joystick and paddle only: the mouse segment draws MDL2's own glyph.
    static void   PaintJoystickMono (IDxuiPainter & painter, const RECT & box, uint32_t ink);
    static void   PaintPaddleMono   (IDxuiPainter & painter, const RECT & box, uint32_t ink);

    void  PaintExpanded  (IDxuiPainter & painter, IDxuiTextRenderer & text, const CassoTheme & theme);
    void  PaintCollapsed (IDxuiPainter & painter, const CassoTheme & theme);


    DxuiCommand          m_modes[3];        // joystick, paddle, mouse
    Segment              m_segments[3];
    RECT                 m_rc             = {};
    RECT                 m_labelRc        = {};
    InputFn              m_sink;
    IDxuiTextRenderer *  m_textRenderer   = nullptr;
    UINT                 m_dpi            = 96;
    bool                 m_labeled        = true;
    bool                 m_arrowsJoystick = false;
    InputMappingMode     m_pointerMode    = InputMappingMode::Off;
    bool                 m_mouseAvailable = false;
    bool                 m_skeuo          = true;
    bool                 m_monoline       = true;
};
