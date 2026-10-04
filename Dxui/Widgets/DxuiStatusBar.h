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
//  A field can also hold a meter at its right end, or be a meter itself,
//  filled from its left with its text over the fill, and run an action when
//  it is pressed, such as opening a popup above it.
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

        //  A meter along the field's right end, meterWidthDip wide, filled
        //  to meter (0 to 1) in meterArgb; the text takes the rest. A
        //  negative meter draws none.
        float         meter         = -1.0f;
        uint32_t      meterArgb     = 0;
        int           meterWidthDip = 0;

        //  The whole field as a meter: its background filled from the left to
        //  fill (0 to 1), blending from fillFromArgb at the left edge to
        //  fillToArgb at the fill's end, with the text drawn over it in
        //  white with a shadow so it reads over the fill and the band alike.
        //  A negative fill draws none, and the text as usual.
        float         fill          = -1.0f;
        uint32_t      fillFromArgb  = 0;
        uint32_t      fillToArgb    = 0;

        //  Called with the field's rectangle, in the bounds' pixels, when the
        //  field is pressed; a field without one ignores the mouse.
        std::function<void (const RECT &)>  onClick;
    };

    DxuiStatusBar() = default;
    ~DxuiStatusBar() override = default;

    void  SetFields (std::vector<Field> fields);
    void  SetText   (size_t index, std::wstring text);
    void  SetMeter  (size_t index, float fraction, uint32_t argb);
    void  SetFill   (size_t index, float fraction, uint32_t fromArgb, uint32_t toArgb);

    //  The field under a point in the bounds' pixels, or -1.
    int   FindFieldAt (POINT point) const;

    size_t               GetFieldCount () const             { return m_fields.size(); }
    const Field &        GetField      (size_t index) const { return m_fields[index]; }

    //  The band's height in DIPs.
    static int  GetBandDp();

    //  One field's rectangle after the last layout, in the bounds' pixels.
    RECT  GetFieldRect (size_t index) const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR             GetCursorForPoint (POINT clientPx) const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }
    std::wstring        GetAccessibleName () const override;

    static constexpr int       kFieldPadDip    = 8;
    static constexpr float     kFontDip        = 12.0f;
    static constexpr int       kMeterHeightDip = 8;
    static constexpr int       kShadowReachDip = 3;
    static constexpr uint32_t  kFillTextArgb   = 0xFFFFFFFFu;

private:
    void  PaintMeter (IDxuiPainter & painter, const Field & field, const RECT & fieldRect, const IDxuiTheme & theme) const;

    std::vector<Field>  m_fields;
    std::vector<RECT>   m_fieldRects;
    DxuiDpiScaler       m_scaler;
};
