#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapePositionDialog
//
//  Prompts for a tape position: one field that takes a time in the counter's
//  format ("1:30", or plain seconds), starting at the current position, with
//  the tape's length beside it. OK moves the tape there; for a time that
//  cannot be parsed, an error shows under the field and the dialog stays
//  open. A time past the end moves the tape to the end.
//
////////////////////////////////////////////////////////////////////////////////

class TapePositionDialog : public DxuiDialogWindow
{
public:
    struct Result
    {
        double  seconds   = 0.0;
        bool    confirmed = false;
    };

    void  Configure (const IDxuiTheme * theme, double positionSeconds, double lengthSeconds);

    const Result &  GetOutcome () const { return m_result; }

protected:
    void  OnCreate () override;

private:
    void  OnOkClicked ();

    const IDxuiTheme  * m_theme           = nullptr;   // non-owning
    double              m_positionSeconds = 0.0;
    double              m_lengthSeconds   = 0.0;
    Result              m_result;

    DxuiLabel           m_prompt;
    DxuiTextInput       m_input;
    DxuiLabel           m_error;
};