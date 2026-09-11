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
    };

    DxuiStatusBar() = default;
    ~DxuiStatusBar() override = default;

    void  SetFields (std::vector<Field> fields);
    void  SetText   (size_t index, std::wstring text);

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
    static constexpr float  kFontDip     = 12.0f;

private:
    std::vector<Field>  m_fields;
    std::vector<RECT>   m_fieldRects;
    DxuiDpiScaler       m_scaler;
};
