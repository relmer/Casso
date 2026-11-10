#pragma once

#include "Pch.h"

#include "CassoExplorer/CardLayoutBar.h"
#include "CassoExplorer/Model/CardLayoutModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel
//
//  The question, the card's layout bar, the card's partitions and the
//  warning, stacked, each as tall as its text wraps.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerInitializeCardPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    void  Init (DxuiLabel & question, CardLayoutBar & bar, DxuiLabel & contents, DxuiLabel & warning);

    void  Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse            (const DxuiMouseEvent & ev) override;
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    static constexpr int  kSectionGapDip = 12;

    static int  GetLabelHeightPx (const DxuiLabel & label, int widthPx, const DxuiDpiScaler & scaler);

    int  Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place);

    DxuiLabel      * m_question = nullptr;
    CardLayoutBar  * m_bar      = nullptr;
    DxuiLabel      * m_contents = nullptr;
    DxuiLabel      * m_warning  = nullptr;
    DxuiDpiScaler    m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardDialog
//
//  The confirmation of an erase. Cancel is the default and has the focus, so
//  Enter and Escape cancel; the erase runs only from its button, which is
//  drawn in the theme's error color, when it is clicked or focused with Tab.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerInitializeCardDialog : public DxuiDialogWindow
{
public:
    explicit CassoExplorerInitializeCardDialog (const CardLayoutModel & model) : m_model (model), m_bar (model) {}

    //  False when canceled or the dialog could not open; otherwise the layout
    //  code of the chosen position.
    static bool  Confirm (HWND                       owner,
                          const IDxuiTheme         * theme,
                          const std::wstring       & question,
                          const CardLayoutModel    & model,
                          const std::wstring       & contents,
                          uint64_t                 & outLayout);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    const CardLayoutModel               & m_model;
    std::wstring                          m_questionText;
    std::wstring                          m_contentsText;
    bool                                  m_confirmed = false;
    uint64_t                              m_layout    = 0;
    DxuiLabel                             m_question;
    CardLayoutBar                         m_bar;
    DxuiLabel                             m_contents;
    DxuiLabel                             m_warning;
    CassoExplorerInitializeCardPanel    * m_body      = nullptr;
};
