#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingGroup
//
//  A translucent capsule that floats over the platter with controls in it
//  (FR-163). A press, release or move over the capsule goes to its controls
//  and then stops there, so the platter under it takes nothing.
//
////////////////////////////////////////////////////////////////////////////////

class FloatingGroup : public DxuiPanel
{
public:
    FloatingGroup () = default;

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    bool  Contains (POINT pointPx) const;

private:
    DxuiDpiScaler  m_scaler;
};
