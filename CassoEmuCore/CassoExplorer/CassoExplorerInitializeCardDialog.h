#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeCardChoice
//
//  One configuration a storage card can be initialized with.
//
////////////////////////////////////////////////////////////////////////////////

struct InitializeCardChoice
{
    std::wstring  label;
    std::wstring  description;

    //  A count of CFFA volumes, or RawDeviceProtocol::kLayoutFat32.
    uint64_t      layout = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerInitializeCardPanel
//
//  The question, the configurations, the card's partitions and the warning,
//  stacked, each as tall as its text wraps.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerInitializeCardPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    void  Init (DxuiLabel & question, DxuiRadioGroup & choices, DxuiLabel & contents, DxuiLabel & warning);

    void  Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse            (const DxuiMouseEvent & ev) override;
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    static constexpr int  kOptionGapDip  = 6;
    static constexpr int  kSectionGapDip = 12;

    static int  GetLabelHeightPx (const DxuiLabel & label, int widthPx, const DxuiDpiScaler & scaler);

    int  Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place);

    DxuiLabel       * m_question = nullptr;
    DxuiRadioGroup  * m_choices  = nullptr;
    DxuiLabel       * m_contents = nullptr;
    DxuiLabel       * m_warning  = nullptr;
    DxuiDpiScaler     m_scaler;
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
    //  False when canceled or the dialog could not open; otherwise the chosen
    //  configuration's layout.
    static bool  Confirm (HWND                                       owner,
                          const IDxuiTheme                         * theme,
                          const std::wstring                       & question,
                          const std::vector<InitializeCardChoice>  & choices,
                          size_t                                     defaultChoice,
                          const std::wstring                       & contents,
                          uint64_t                                 & outLayout);

protected:
    void  OnCreate() override;

private:
    std::wstring                         m_questionText;
    std::wstring                         m_contentsText;
    std::vector<InitializeCardChoice>    m_choiceList;
    size_t                               m_default   = 0;
    bool                                 m_confirmed = false;
    uint64_t                             m_layout    = 0;
    DxuiLabel                            m_question;
    DxuiRadioGroup                       m_choices;
    DxuiLabel                            m_contents;
    DxuiLabel                            m_warning;
    CassoExplorerInitializeCardPanel   * m_body      = nullptr;
};
