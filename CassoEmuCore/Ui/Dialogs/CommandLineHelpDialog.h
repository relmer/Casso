#pragma once

#include "Pch.h"

#include "Ui/Dialogs/CommandLineHelpPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpDialog
//
//  The emulator's command-line help, for --help and for a command line Casso
//  cannot parse at startup, before the main window exists. One OK button.
//
////////////////////////////////////////////////////////////////////////////////

class CommandLineHelpDialog : public DxuiDialogWindow
{
public:
    //  The dialog's text: the name, the version, the refusal when there is one,
    //  the usage and the option rows in the prefix the reader typed. The logo is
    //  left empty for the caller, which has the module to load it from.
    static CommandLineHelpPanel::Content  MakeContent (char flagPrefix, const std::string & refusal);

    static HRESULT                        Show        (HINSTANCE instance, const IDxuiTheme * theme, const CommandLineHelpPanel::Content & content);

protected:
    void  OnCreate () override;

private:
    static constexpr int  kWidthDip  = 680;
    static constexpr int  kHeightDip = 520;

    CommandLineHelpPanel::Content  m_content;
};
