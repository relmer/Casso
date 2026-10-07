#pragma once

#include "Pch.h"

#include "Ui/Chrome/UpdateIndicatorModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorButton
//
//  The title-bar mark that a newer release is available: a download glyph
//  in the accent color, one caption-button column wide, to the left of
//  minimize. Hidden until there is a release to offer.
//
//  It lives in the host's caption, whose children are laid out in DIPs and
//  paint through the caption's scaler, like the system buttons beside it.
//  It reports a client hit so Windows sends ordinary mouse messages for it
//  rather than starting a caption drag; the shell owns hover, press and
//  click, since the caption does not route client input.
//
////////////////////////////////////////////////////////////////////////////////

class UpdateIndicatorButton : public IDxuiControl
{
public:
    // What a pointer move means for the shell: repaint, raise the tooltip,
    // or take it down now.
    struct PointerResult
    {
        bool  repaint = false;
        bool  showTip = false;
        bool  hideTip = false;
    };

    UpdateIndicatorButton();

    PointerResult         OnPointer         (bool isInside, int64_t nowMs);

    bool                  ContainsDip       (POINT pointDip) const;
    bool                  SetHovered        (bool isHovered);
    bool                  SetPressed        (bool isPressed);
    bool                  IsPressed         () const { return m_isPressed; }
    void                  SetToolTipText    (const std::wstring & text) { m_toolTip = text; }
    const std::wstring  & GetToolTipText    () const { return m_toolTip; }
    void                  SetText           (const std::wstring & text) { m_text = text; }
    const std::wstring  & GetText           () const { return m_text; }
    void                  SetShowsText      (bool showsText) { m_showsText = showsText; }
    void                  SetAnimationsEnabled (bool isEnabled) { m_isAnimated = isEnabled; }
    void                  StartShimmerClock (int64_t nowMs) { m_shownAtMs = nowMs; }
    void                  SetRandomSource   (UpdateIndicatorModel::RandomIndexFn randomIndex) { m_random = std::move (randomIndex); }
    const GlintLayout   & GetGlintLayout    () const { return m_glintLayout; }
    bool                  TickShimmer       (int64_t nowMs);
    std::optional<int64_t>  GetMsUntilShimmer (int64_t nowMs) const;

    void                  Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                  Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiHitTestKind       ClassifyHit       (POINT clientDip) const override;
    std::wstring          GetAccessibleName () const override;
    DxuiAccessibleRole    GetAccessibleRole () const override { return DxuiAccessibleRole::Button; }

private:
    DxuiDpiScaler                        m_scaler;
    std::wstring                         m_toolTip;
    bool                                 m_isHovered   = false;
    bool                                 m_isPressed   = false;
    std::wstring                         m_text;
    bool                                 m_showsText   = false;
    bool                                 m_isAnimated  = false;
    int64_t                              m_shownAtMs   = 0;
    std::optional<float>                 m_sweep;
    GlintLayout                          m_glintLayout = UpdateIndicatorModel::MakeEvenGlintLayout();
    UpdateIndicatorModel::RandomIndexFn  m_random;

    void           PaintShimmer (IDxuiTextRenderer & text, float x, float y, float w, float h, float fontPx, float progress) const;
};
