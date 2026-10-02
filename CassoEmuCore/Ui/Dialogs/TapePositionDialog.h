#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapePositionDialog
//
//  Asks where to wind the tape: one field taking a time as a counter shows it
//  ("1:30", or plain seconds), starting at the current position, and the
//  tape's length beside it. OK winds there; a time that cannot be read is
//  said so under the field and the dialog stays open. A time past the end
//  winds to the end.
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