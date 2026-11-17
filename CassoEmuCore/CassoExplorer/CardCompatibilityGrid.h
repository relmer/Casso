#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CardLayoutModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CardCompatibilityGrid
//
//  For the chosen layout and adapter: the card's space outside it, then a
//  row for each adapter it concerns, with an icon for its support and a
//  note. The icons are drawn rather than read from a font: a check in a
//  green circle, a warning triangle, a cross in a red circle, and a gray
//  circle for unconfirmed.
//
////////////////////////////////////////////////////////////////////////////////

class CardCompatibilityGrid : public IDxuiControl
{
public:
    explicit CardCompatibilityGrid (const CardLayoutModel & model) : m_model (model) {}

    void  SetChoice (int position, CardLayoutModel::Adapter adapter) { m_position = position; m_adapter = adapter; }

    //  The height at a width, in pixels: as tall as the most rows of any
    //  adapter and layout.
    int   MeasureHeightPx (int widthPx, const DxuiDpiScaler & scaler) const;

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    static constexpr float     kLineDip      = 20.0f;
    static constexpr float     kRowDip       = 36.0f;
    static constexpr float     kGapDip       = 6.0f;
    static constexpr float     kFontDip      = 13.0f;
    static constexpr float     kAdapterShare = 0.42f;
    static constexpr float     kIconDip      = 24.0f;

    static constexpr uint32_t  s_kWorksArgb       = 0xFF3B6D11;
    static constexpr uint32_t  s_kStepArgb        = 0xFFFFB900;   // Windows' warning yellow
    static constexpr uint32_t  s_kWarningMarkArgb = 0xFF000000;

    //  The error color darkened for the white cross, as the erase button's
    //  fill is, so the two reds match.
    static constexpr float     kWhiteContrast     = 4.5f;

    void  PaintIcon (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, CardLayoutModel::Support support, float cx, float cy);

    const CardLayoutModel     & m_model;
    DxuiDpiScaler               m_scaler;
    int                         m_position = 1;
    CardLayoutModel::Adapter    m_adapter  = CardLayoutModel::kDefaultAdapter;
};
