#pragma once

#include "Pch.h"

#include "Core/DxuiPanel.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiTextInput.h"
#include "Window/DxuiDialogWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePromptPanel
//
//  A label over a single-line text field, filling the dialog's content area.
//
////////////////////////////////////////////////////////////////////////////////

class CassquePromptPanel : public DxuiPanel
{
public:
    void  Init (DxuiLabel & label, DxuiTextInput & input);

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kLabelHeightDip = 22;
    static constexpr int  kGapDip         = 6;
    static constexpr int  kInputHeightDip = 30;

private:
    DxuiLabel      * m_label = nullptr;
    DxuiTextInput  * m_input = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePromptDialog
//
//  Asks for one line of text, for a rename or a new name. OK ends the dialog
//  with the text; Cancel, Escape and the close box end it without.
//
////////////////////////////////////////////////////////////////////////////////

class CassquePromptDialog : public DxuiDialogWindow
{
public:
    void  Configure (const std::wstring & label, const std::wstring & initialText, const IDxuiTheme * theme, size_t maxLength);

    bool                  IsConfirmed () const { return m_confirmed; }
    const std::wstring &  GetText     () const { return m_text; }

    //  Creates, shows and runs the dialog modally over `owner`. False when
    //  it was cancelled or could not open.
    static bool  Ask (HWND                  owner,
                      const IDxuiTheme    * theme,
                      const std::wstring  & title,
                      const std::wstring  & label,
                      const std::wstring  & initialText,
                      size_t                maxLength,
                      std::wstring        & outText);

protected:
    void  OnCreate() override;

private:
    const IDxuiTheme     * m_theme     = nullptr;
    std::wstring           m_labelText;
    std::wstring           m_text;
    size_t                 m_maxLength = 0;
    bool                   m_confirmed = false;
    DxuiLabel              m_label;
    DxuiTextInput          m_input;
    CassquePromptPanel   * m_body      = nullptr;
};
