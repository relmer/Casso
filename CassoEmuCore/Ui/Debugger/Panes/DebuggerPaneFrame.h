#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





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
//  false, and gives its height to the rest.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerPaneFrame : public IDxuiControl
{
public:
    using HeightFn = std::function<int (int widthDip, const DxuiDpiScaler & scaler)>;
    using ShownFn  = std::function<bool ()>;

    DebuggerPaneFrame  (std::wstring accessibleName) : m_name (std::move (accessibleName)) {}
    ~DebuggerPaneFrame () override = default;

    void  AddPart (IDxuiControl * control, HeightFn height = nullptr, ShownFn shown = nullptr);

    //  Room left below the last part, so a control drawn with a border of its
    //  own, the console's command box, does not sit on the pane's edge.
    void  SetBottomMarginDip (int dip) { m_bottomMarginDip = dip; }
    //  Lays the parts out again, for a part whose height or shown state
    //  changed while the frame kept its bounds.
    void  Relayout ();

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void                OnVisibilityChanged () override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Panel; }
    std::wstring        GetAccessibleName () const override { return m_name; }

    static constexpr int  kGapDip = 6;

private:
    struct Part
    {
        IDxuiControl  * control = nullptr;
        HeightFn        height;
        ShownFn         shown;
    };

    bool  IsPartShown (const Part & part) const;

    std::wstring       m_name;
    std::vector<Part>  m_parts;
    DxuiDpiScaler      m_scaler;
    int                m_bottomMarginDip = 0;
};
