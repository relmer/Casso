#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar
//
//  A strip of text fields along the bottom of a window.
//
//  Fixed fields keep their width; one stretch field takes what is left, and a
//  field whose text does not fit is elided at its tail rather than overflowing
//  into its neighbor. The band is as tall as the menu bar's strip, so a window
//  with both reads as one set of chrome.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiStatusBar : public IDxuiControl
{
public:
    struct Field
    {
        std::wstring  text;
        int           widthDip = 0;
        bool          stretch  = false;

        //  A width in pixels, used instead of widthDip when not negative, for
        //  a field that has to line up with an edge elsewhere in the window.
        int           widthPx  = -1;

        //  Placed right after the flowing field before it, as wide as its
        //  text, with a short divider after it: File Explorer's item count
        //  and selection. Flowing fields come first; an empty one takes no
        //  room and draws no divider. Its own rectangle is not used.
        bool          flow     = false;
    };

    DxuiStatusBar() = default;
    ~DxuiStatusBar() override = default;

    void  SetFields (std::vector<Field> fields);
    void  SetText   (size_t index, std::wstring text);

    //  The line along the top and the lines between fields; on by default.
    void  SetDividers (bool dividers) { m_dividers = dividers; }

    size_t               GetFieldCount () const             { return m_fields.size(); }
    const Field &        GetField      (size_t index) const { return m_fields[index]; }

    //  The band's height in DIPs.
    static int  GetBandDp();

    //  One field's rectangle after the last layout, in the bounds' pixels.
    RECT  GetFieldRect (size_t index) const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }
    std::wstring        GetAccessibleName () const override;

    static constexpr int    kFieldPadDip = 8;
    static constexpr float  kFlowGapDip  = 10.0f;   // text to divider, divider to the next field
    static constexpr float  kFlowRuleDip = 12.0f;   // the divider's height
    static constexpr float  kFontDip     = 12.0f;

private:
    std::vector<Field>  m_fields;
    std::vector<RECT>   m_fieldRects;
    DxuiDpiScaler       m_scaler;
    bool                m_dividers = true;
};
