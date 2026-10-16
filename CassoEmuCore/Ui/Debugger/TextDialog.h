#pragma once

#include "Ui/Debugger/FieldDialogPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TextDialog
//
//  One line of text, for a command that takes a value the user types, such
//  as a Memory pane's Fill selection... or Compare with...: OK takes it once
//  the check given returns true, and puts the reason under the field when
//  it returns false.
//
////////////////////////////////////////////////////////////////////////////////

class TextDialog : public DxuiDialogWindow
{
public:
    using CheckFn = std::function<bool (const std::wstring & text, std::wstring & error)>;

    //  Hex bytes apart at spaces, as "A9 00 8D", each one or two digits with
    //  or without a dollar sign; and one address of one to four.
    static bool  TryParseBytes   (const std::wstring & text, std::vector<Byte> & bytes);
    static bool  TryParseAddress (const std::wstring & text, Word & address);

    //  Runs the dialog modally over `owner`; the text when OK.
    static std::optional<std::wstring>  RunModal (HWND owner, const IDxuiTheme * theme, const std::wstring & title, const std::wstring & label,
                                                  const std::wstring & placeholder, CheckFn check);

protected:
    void  OnCreate() override;

private:
    const IDxuiTheme             * m_theme = nullptr;
    std::wstring                   m_label;
    std::wstring                   m_placeholder;
    CheckFn                        m_check;
    std::optional<std::wstring>    m_result;
    DxuiLabel                      m_valueLabel;
    DxuiTextInput                  m_value;
    DxuiLabel                      m_error;
    FieldDialogPanel             * m_body  = nullptr;
};
