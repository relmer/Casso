#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerElevationPanel
//
//  The explanation and the "Don't show this again" checkbox, stacked.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerElevationPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    void  Init (DxuiLabel & text, DxuiCheckbox & dontShow);

    void  Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse            (const DxuiMouseEvent & ev) override;
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    static constexpr int  kSectionGapDip = 16;

    int  Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place);

    DxuiLabel      * m_text     = nullptr;
    DxuiCheckbox   * m_dontShow = nullptr;
    DxuiDpiScaler    m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerElevationDialog
//
//  Opened before the storage card broker is started, which raises the UAC
//  prompt: why Casso Explorer requires administrator access, and what the
//  broker does with it. OK is the default.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerElevationDialog : public DxuiDialogWindow
{
public:
    //  True for OK. `outDontShow` is the checkbox's state either way.
    static bool  Confirm (HWND owner, const IDxuiTheme * theme, const std::wstring & card, bool & outDontShow);

protected:
    void  OnCreate () override;

private:
    std::wstring                    m_card;
    bool                            m_confirmed = false;
    DxuiLabel                       m_text;
    DxuiCheckbox                    m_dontShow;
    CassoExplorerElevationPanel   * m_body      = nullptr;
};
