#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerChips
//
//  What a drive's picker lists, in the command bar beside View: "Showing"
//  and a capsule for each term of the drive's rule, such as 3.5", 400K and
//  800K. Capsules past the right edge of the bounds are left out.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPickerChips : public IDxuiControl
{
public:
    static constexpr float  kFontDip     = 12.0f;
    static constexpr float  kHeightDip   = 22.0f;
    static constexpr float  kPadDip      = 9.0f;
    static constexpr float  kGapDip      = 6.0f;
    static constexpr float  kLabelGapDip = 8.0f;

    void                                SetTerms (std::vector<std::wstring> terms) { m_terms = std::move (terms); }
    const std::vector<std::wstring>  &  GetTerms () const                          { return m_terms; }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        SetBounds (boundsDip);
        m_scaler.SetDpi (scaler.GetDpi());
    }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    std::wstring        GetAccessibleName () const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }

    static constexpr const wchar_t *  kpszLabel = L"Showing";

private:
    float         MeasureWidth (IDxuiTextRenderer & text, const std::wstring & value, float fontPx) const;
    static void   FillCapsule  (IDxuiTextRenderer & text, float x, float y, float width, float height, uint32_t argb);

    std::vector<std::wstring>  m_terms;
    DxuiDpiScaler              m_scaler;
};
