#pragma once

#include "Pch.h"

#include "Core/DxuiPanel.h"
#include "Widgets/DxuiComboBox.h"
#include "Widgets/DxuiLabel.h"
#include "Window/DxuiDialogWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsPanel
//
//  Each setting a label over its control, down the dialog's content area.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerOptionsPanel : public DxuiPanel
{
public:
    void  Init    (DxuiLabel & namingLabel, DxuiComboBox & naming);
    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kLabelHeightDip = 22;
    static constexpr int  kGapDip         = 6;
    static constexpr int  kFieldHeightDip = 30;

private:
    DxuiLabel     * m_namingLabel = nullptr;
    DxuiComboBox  * m_naming      = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog
//
//  CassoExplorer's settings: today, the form a copy out of an image takes on the
//  host. OK keeps the choices; Cancel, Escape and the close box leave them.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerOptionsDialog : public DxuiDialogWindow
{
public:
    struct Choices
    {
        //  An index into GetNamingLabels.
        int  hostNaming = 0;
    };

    //  The host file name forms, in the order the setting stores them.
    static std::vector<std::wstring>  GetNamingLabels ();

    //  Runs the dialog modally over `owner`. False when it was cancelled or
    //  could not open; `inOutChoices` changes only on OK.
    static bool  Ask (HWND owner, const IDxuiTheme * theme, Choices & inOutChoices);

protected:
    void  OnCreate() override;

private:
    const IDxuiTheme           * m_theme       = nullptr;
    Choices                      m_choices;
    bool                         m_confirmed   = false;
    DxuiLabel                    m_namingLabel;
    DxuiComboBox                 m_naming;
    CassoExplorerOptionsPanel  * m_body        = nullptr;
};
