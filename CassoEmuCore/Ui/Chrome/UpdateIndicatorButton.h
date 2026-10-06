#pragma once

#include "Pch.h"





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
    UpdateIndicatorButton();

    bool                  ContainsDip       (POINT pointDip) const;
    bool                  SetHovered        (bool isHovered);
    bool                  SetPressed        (bool isPressed);
    bool                  IsPressed         () const { return m_isPressed; }
    void                  SetToolTipText    (const std::wstring & text) { m_toolTip = text; }
    const std::wstring  & GetToolTipText    () const { return m_toolTip; }

    void                  Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                  Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiHitTestKind       ClassifyHit       (POINT clientDip) const override;
    std::wstring          GetAccessibleName () const override;
    DxuiAccessibleRole    GetAccessibleRole () const override { return DxuiAccessibleRole::Button; }

private:
    DxuiDpiScaler  m_scaler;
    std::wstring   m_toolTip;
    bool           m_isHovered = false;
    bool           m_isPressed = false;
};
