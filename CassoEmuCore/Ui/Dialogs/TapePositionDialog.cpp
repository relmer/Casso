#include "Pch.h"

#include "Ui/Dialogs/TapePositionDialog.h"
#include "Ui/Chrome/TapeDeckWidget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapePositionBody
//
//  The dialog's content: the prompt, the field beside it, and the error line
//  under both. Lays out in physical pixels, as the create-disk body does.
//
////////////////////////////////////////////////////////////////////////////////

class TapePositionBody : public DxuiPanel
{
public:
    void  Init (DxuiLabel & prompt, DxuiTextInput & input, DxuiLabel & error)
    {
        m_prompt = &prompt;
        m_input  = &input;
        m_error  = &error;

        Adopt (prompt);
        Adopt (input);
        Adopt (error);
    }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
    {
        int   pad     = scaler.ToPx (kPadDip);
        int   rowH    = scaler.ToPx (kRowDip);
        int   labelW  = scaler.ToPx (kLabelDip);
        int   inputW  = scaler.ToPx (kInputDip);
        int   x       = boundsPx.left + pad;
        int   y       = boundsPx.top  + pad;



        SetBounds (boundsPx);

        m_prompt->Layout (RECT { x, y, x + labelW, y + rowH }, scaler);
        m_input->Layout  (RECT { x + labelW, y, x + labelW + inputW, y + rowH }, scaler);
        m_error->Layout  (RECT { x, y + rowH + pad, boundsPx.right - pad, y + 2 * rowH + pad }, scaler);
    }

    bool  OnMouse (const DxuiMouseEvent & ev) override
    {
        return m_input->OnMouse (ev);
    }

private:
    static constexpr int  kPadDip   = 12;
    static constexpr int  kRowDip   = 28;
    static constexpr int  kLabelDip = 150;
    static constexpr int  kInputDip = 110;

    DxuiLabel      * m_prompt = nullptr;
    DxuiTextInput  * m_input  = nullptr;
    DxuiLabel      * m_error  = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapePositionDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void TapePositionDialog::Configure (const IDxuiTheme * theme, double positionSeconds, double lengthSeconds)
{
    m_theme           = theme;
    m_positionSeconds = positionSeconds;
    m_lengthSeconds   = lengthSeconds;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapePositionDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void TapePositionDialog::OnCreate()
{
    TapePositionBody  * body = nullptr;
    DxuiButton        * ok   = nullptr;



    m_prompt.SetTextRole  (DxuiTextRole::Body);
    m_prompt.SetText      (L"Position (of " + TapeDeckWidget::FormatTime (m_lengthSeconds) + L"):");
    m_prompt.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_input.SetTheme        (m_theme);
    m_input.SetHwnd         (GetHwnd());
    m_input.SetMaxLength    (12);
    m_input.SetTextRenderer (GetTextRenderer());
    m_input.SetText         (TapeDeckWidget::FormatTime (m_positionSeconds));

    m_error.SetTextRole  (DxuiTextRole::Body);
    m_error.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    body = CreateDialogContent<TapePositionBody>();
    body->Init (m_prompt, m_input, m_error);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    // A custom click keeps the dialog open when the time cannot be read.
    ok->SetOnClick ([this] () { OnOkClicked(); });

    SetInitialFocus (&m_input);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapePositionDialog::OnOkClicked
//
////////////////////////////////////////////////////////////////////////////////

void TapePositionDialog::OnOkClicked()
{
    double  seconds = 0.0;



    if (!TapeDeckWidget::ParseTime (m_input.GetText(), seconds))
    {
        m_error.SetText (L"Type a time such as 1:30, or a number of seconds.");
        Invalidate();
        return;
    }

    m_result.seconds   = min (seconds, m_lengthSeconds);
    m_result.confirmed = true;
    EndDialog (IDOK);
}