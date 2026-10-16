#pragma once

#include "Ui/Debugger/FieldDialogPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ByteDialog
//
//  One byte typed in hex, for a command that takes a value, such as the
//  Stack pane's Push byte...: OK takes it only once it is one or two hex
//  digits, with or without a dollar sign.
//
////////////////////////////////////////////////////////////////////////////////

class ByteDialog : public DxuiDialogWindow
{
public:
    static bool  TryParseByte (const std::wstring & text, Byte & value);

    //  Runs the dialog modally over `owner`; the byte when OK.
    static std::optional<Byte>  RunModal (HWND owner, const IDxuiTheme * theme, const std::wstring & title, const std::wstring & label);

protected:
    void  OnCreate() override;

private:
    const IDxuiTheme     * m_theme = nullptr;
    std::wstring           m_label;
    std::optional<Byte>    m_result;
    DxuiLabel              m_valueLabel;
    DxuiTextInput          m_value;
    DxuiLabel              m_error;
    FieldDialogPanel     * m_body  = nullptr;
};
