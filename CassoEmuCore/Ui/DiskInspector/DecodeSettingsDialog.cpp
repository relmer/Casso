#include "Pch.h"

#include "Ui/DiskInspector/DecodeSettingsDialog.h"





static constexpr LPCWSTR  s_kpszLabels[] =
{
    L"From track",
    L"To track",
    L"16-sector address prologue",
    L"13-sector address prologue",
    L"Data prologue",
    L"Address epilogue",
    L"Data epilogue",
};

static constexpr int  s_kMaxMarkChars = 8;
static constexpr int  s_kMaxTrackChars = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsBody
//
//  A labeled field per row, the four switches, then the error line, laid out
//  in physical pixels as the other dialog bodies are.
//
////////////////////////////////////////////////////////////////////////////////

class DecodeSettingsBody : public DxuiPanel
{
public:
    void  Init (std::array<DxuiLabel, 7> & labels, std::array<DxuiTextInput, 7> & inputs, std::array<DxuiCheckbox *, 4> checks, DxuiLabel & error)
    {
        size_t  i = 0;



        m_labels = &labels;
        m_inputs = &inputs;
        m_checks = checks;
        m_error  = &error;

        for (i = 0; i < labels.size(); i++)
        {
            Adopt (labels[i]);
            Adopt (inputs[i]);
        }

        for (DxuiCheckbox * check : checks)
        {
            Adopt (*check);
        }

        Adopt (error);
    }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
    {
        int     pad    = scaler.ToPx (kPadDip);
        int     rowH   = scaler.ToPx (kRowDip);
        int     labelW = scaler.ToPx (kLabelDip);
        int     x      = boundsPx.left + pad;
        int     y      = boundsPx.top  + pad;
        size_t  i      = 0;



        SetBounds (boundsPx);

        for (i = 0; i < m_labels->size(); i++, y += rowH)
        {
            (*m_labels)[i].Layout (RECT { x, y, x + labelW, y + rowH - 4 }, scaler);
            (*m_inputs)[i].Layout (RECT { x + labelW, y, boundsPx.right - pad, y + rowH - 4 }, scaler);
        }

        for (DxuiCheckbox * check : m_checks)
        {
            check->Layout (RECT { x, y, boundsPx.right - pad, y + rowH - 4 }, scaler);
            y += rowH;
        }

        m_error->Layout (RECT { x, y, boundsPx.right - pad, y + rowH }, scaler);
    }

    bool  OnMouse (const DxuiMouseEvent & ev) override
    {
        bool  isHandled = false;



        for (DxuiTextInput & input : *m_inputs)
        {
            isHandled = isHandled || input.OnMouse (ev);
        }

        for (DxuiCheckbox * check : m_checks)
        {
            isHandled = isHandled || check->OnMouse (ev);
        }

        return isHandled;
    }

private:
    static constexpr int  kPadDip   = 12;
    static constexpr int  kRowDip   = 32;
    static constexpr int  kLabelDip = 200;

    std::array<DxuiLabel, 7> *      m_labels = nullptr;
    std::array<DxuiTextInput, 7> *  m_inputs = nullptr;
    std::array<DxuiCheckbox *, 4>   m_checks = {};
    DxuiLabel *                     m_error  = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void DecodeSettingsDialog::Configure (const IDxuiTheme * theme, const DecodeSettings & settings, int track)
{
    m_theme    = theme;
    m_settings = settings;
    m_form     = DecodeSettingsForm::MakeFrom (settings, track);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void DecodeSettingsDialog::OnCreate()
{
    DecodeSettingsBody *  body  = nullptr;
    DxuiButton *          ok    = nullptr;
    DxuiButton *          reset = nullptr;
    size_t                i     = 0;



    for (i = 0; i < m_labels.size(); i++)
    {
        m_labels[i].SetTextRole  (DxuiTextRole::Body);
        m_labels[i].SetText      (s_kpszLabels[i]);
        m_labels[i].SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    ConfigureInput (m_inputs[0], m_form.firstTrack);
    ConfigureInput (m_inputs[1], m_form.lastTrack);
    ConfigureInput (m_inputs[2], m_form.address16);
    ConfigureInput (m_inputs[3], m_form.address13);
    ConfigureInput (m_inputs[4], m_form.data);
    ConfigureInput (m_inputs[5], m_form.addressEpilogue);
    ConfigureInput (m_inputs[6], m_form.dataEpilogue);

    m_inputs[0].SetMaxLength (s_kMaxTrackChars);
    m_inputs[1].SetMaxLength (s_kMaxTrackChars);

    m_matchStandard.SetChecked (m_form.matchStandardToo);
    m_checkAddress.SetChecked  (m_form.checkAddress);
    m_checkData.SetChecked     (m_form.checkData);
    m_checkEpilogue.SetChecked (m_form.checkEpilogues);

    m_error.SetTextRole  (DxuiTextRole::Error);
    m_error.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    body = CreateDialogContent<DecodeSettingsBody>();
    body->Init (m_labels, m_inputs, { &m_matchStandard, &m_checkAddress, &m_checkData, &m_checkEpilogue }, m_error);

    reset = AddDialogButton (L"Reset to standard", IDRETRY);
    ok    = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick    ([this] () { OnOkClicked(); });
    reset->SetOnClick ([this] () { OnResetClicked(); });

    SetInitialFocus (&m_inputs[2]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsDialog::OnOkClicked
//
//  The range is added after the existing ones, so it overrides them on the
//  tracks it covers.
//
////////////////////////////////////////////////////////////////////////////////

void DecodeSettingsDialog::OnOkClicked()
{
    DecodeRange   range;
    std::wstring  error;



    m_form.firstTrack       = m_inputs[0].GetText();
    m_form.lastTrack        = m_inputs[1].GetText();
    m_form.address16        = m_inputs[2].GetText();
    m_form.address13        = m_inputs[3].GetText();
    m_form.data             = m_inputs[4].GetText();
    m_form.addressEpilogue  = m_inputs[5].GetText();
    m_form.dataEpilogue     = m_inputs[6].GetText();
    m_form.matchStandardToo = m_matchStandard.IsChecked();
    m_form.checkAddress     = m_checkAddress.IsChecked();
    m_form.checkData        = m_checkData.IsChecked();
    m_form.checkEpilogues   = m_checkEpilogue.IsChecked();

    if (m_form.TryBuildRange (range, error))
    {
        m_settings.AddRange (range);
        m_outcome = Outcome::Applied;
        EndDialog (IDOK);
    }
    else
    {
        m_error.SetText (error);
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsDialog::OnResetClicked
//
////////////////////////////////////////////////////////////////////////////////

void DecodeSettingsDialog::OnResetClicked()
{
    m_settings = DecodeSettings::MakeStandard();
    m_outcome  = Outcome::Reset;
    EndDialog (IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsDialog::ConfigureInput
//
////////////////////////////////////////////////////////////////////////////////

void DecodeSettingsDialog::ConfigureInput (DxuiTextInput & input, const std::wstring & text)
{
    input.SetTheme        (m_theme);
    input.SetHwnd         (GetHwnd());
    input.SetMaxLength    (s_kMaxMarkChars);
    input.SetTextRenderer (GetTextRenderer());
    input.SetText         (text);
}
