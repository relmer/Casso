#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneFrame
//
//  One dockable pane made of several controls: the source pane's banner over
//  its text, the console over its command box. The dock site moves, shows and
//  hides a pane as one control, and the frame passes each of those on to its
//  parts, stacked top to bottom.
//
//  THE FRAME PLACES ITS PARTS; IT DOES NOT OWN OR PAINT THEM. They stay
//  children of the window, which paints them and routes their input, as it
//  does for a pane of one control.
//
//  A part with a height function gets that height, and one without takes
//  what is left. A part with a shown function is hidden while it answers
//  false, and its height goes to the rest. A part set beside another is
//  not stacked: it is at the left of that part, as tall as it, and the other
//  part is narrowed by the width it takes.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerPaneFrame : public IDxuiControl
{
public:
    using HeightFn = std::function<int (int widthDip, const DxuiDpiScaler & scaler)>;
    using ShownFn  = std::function<bool ()>;
    using WidthFn  = std::function<int (int heightPx, const DxuiDpiScaler & scaler)>;

    DebuggerPaneFrame  (std::wstring accessibleName) : m_name (std::move (accessibleName)) {}
    ~DebuggerPaneFrame () override = default;

    void  AddPart (IDxuiControl * control, HeightFn height = nullptr, ShownFn shown = nullptr);

    //  Room left below the last part, so a control drawn with a border of its
    //  own, the console's command box, does not sit on the pane's edge.
    void  SetBottomMarginDip (int dip) { m_bottomMarginDip = dip; }

    //  Room kept clear on a part's left, right and top, so a graphic that draws
    //  to its own bounds does not touch the pane's edge.
    void  SetPartInsetDip (IDxuiControl * control, int dip);

    //  A part whose text starts where the pane's title does: its sides are
    //  kept in by the pane's text inset, less the room the part keeps ahead
    //  of its own text. Its top inset and its height are unchanged.
    void  SetPartTextAligned (IDxuiControl * control, int innerPadDip = 0);

    //  A part at the left of `beside`, as tall as it, the width the function
    //  returns for that height.
    void  SetPartBeside (IDxuiControl * control, IDxuiControl * beside, WidthFn width);

    //  Lays the parts out again, for a part whose height or shown state
    //  changed while the frame kept its bounds.
    void  Relayout ();

    //  The least height the parts fit in at a width: the visible fixed
    //  parts, the gaps between them, the bottom margin, and kMinFillDip for a
    //  part that fills.
    int   GetMinimumHeightPx (int widthPx, const DxuiDpiScaler & scaler) const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void                OnVisibilityChanged () override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Panel; }
    std::wstring        GetAccessibleName () const override { return m_name; }

    static constexpr int  kGapDip     = 6;
    static constexpr int  kMinFillDip = 20;

private:
    struct Part
    {
        IDxuiControl  * control     = nullptr;
        HeightFn        height;
        ShownFn         shown;
        int             insetDip    = 0;
        bool            textAligned = false;
        int             innerPadDip = 0;
        IDxuiControl  * beside      = nullptr;
        WidthFn         width;
    };

    bool  IsPartShown (const Part & part) const;

    //  The visible fixed parts' heights at a width, each part's in heights,
    //  with the count of visible parts and whether one of them fills.
    int   MeasureFixedParts (int width, const DxuiDpiScaler & scaler, std::vector<int> & heights, int & shownCount, bool & hasFill) const;

    //  How far a part is kept in from the pane's left and right.
    static int  GetSideInsetPx (const Part & part, const DxuiDpiScaler & scaler);

    std::wstring       m_name;
    std::vector<Part>  m_parts;
    DxuiDpiScaler      m_scaler;
    int                m_bottomMarginDip = 0;
};
