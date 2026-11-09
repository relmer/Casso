#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/DecodeSettingsForm.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeSettingsDialog
//
//  "Decode settings" (FR-019): the tracks the settings cover, the five marks
//  with ?? for any byte, "Match standard marks too", and the three checks.
//  OK adds the range to the settings; "Reset to standard" puts the standard
//  settings back. A form that cannot be read says why and stays open.
//
////////////////////////////////////////////////////////////////////////////////

class DecodeSettingsDialog : public DxuiDialogWindow
{
public:
    enum class Outcome
    {
        Cancelled,
        Applied,
        Reset,
    };

    void  Configure (const IDxuiTheme * theme, const DecodeSettings & settings, int track);

    Outcome                 GetOutcome  () const { return m_outcome; }
    const DecodeSettings &  GetSettings () const { return m_settings; }

    static constexpr SIZE  kSizeDip = { 460, 470 };

protected:
    void  OnCreate () override;

private:
    void  OnOkClicked    ();
    void  OnResetClicked ();
    void  ConfigureInput (DxuiTextInput & input, const std::wstring & text);

    const IDxuiTheme *  m_theme   = nullptr;   // non-owning
    DecodeSettings      m_settings;
    DecodeSettingsForm  m_form;
    Outcome             m_outcome = Outcome::Cancelled;

    std::array<DxuiLabel, 7>      m_labels;
    std::array<DxuiTextInput, 7>  m_inputs;
    DxuiCheckbox                  m_matchStandard { L"Match standard marks too" };
    DxuiCheckbox                  m_checkAddress  { L"Check address checksums" };
    DxuiCheckbox                  m_checkData     { L"Check data checksums" };
    DxuiCheckbox                  m_checkEpilogue { L"Check epilogues" };
    DxuiLabel                     m_error;
};
