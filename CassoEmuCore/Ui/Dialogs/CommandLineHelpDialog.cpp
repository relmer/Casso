#include "Pch.h"

#include "Ui/Dialogs/CommandLineHelpDialog.h"

#include "CommandLineHelp.h"
#include "Core/TextEncoding.h"
#include "Version.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineHelpDialog::OnCreate()
{
    CommandLineHelpPanel  * panel = CreateDialogContent<CommandLineHelpPanel>();
    DxuiButton            * ok    = nullptr;



    panel->Init (m_content);

    ok = AddDialogButton (L"OK", IDOK);

    SetInitialFocus (ok);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpDialog::MakeContent
//
////////////////////////////////////////////////////////////////////////////////

CommandLineHelpPanel::Content CommandLineHelpDialog::MakeContent (char flagPrefix, const std::string & refusal)
{
    CommandLineHelpPanel::Content  content;



    content.title    = L"Casso";
    content.subtitle = L"Apple II family emulator";
    content.version  = L"Version " _CRT_WIDE (VERSION_STRING);
    content.refusal  = TextEncoding::NarrowToWide (refusal);
    content.usage    = TextEncoding::NarrowToWide (CommandLineHelp::kEmulatorUsage);

    for (const CommandLineHelpRow & row : CommandLineHelp::GetEmulatorRows (flagPrefix))
    {
        content.rows.push_back ({ TextEncoding::NarrowToWide (row.option), TextEncoding::NarrowToWide (row.description) });
    }

    return content;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpDialog::Show
//
//  Modal, centered on the screen, since nothing else is on it yet. The height
//  is fitted to the content before the window appears.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CommandLineHelpDialog::Show (HINSTANCE instance, const IDxuiTheme * theme, const CommandLineHelpPanel::Content & content)
{
    HRESULT                   hr = S_OK;
    CommandLineHelpDialog     dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_content = content;

    params.title                    = content.title + L" command-line options";
    params.hInstance                = instance;
    params.initialSizeDip           = { kWidthDip, kHeightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnScreen;

    hr = dialog.Create (params);
    CHR (hr);

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

Error:
    return hr;
}





