#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorSearch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog
//
//  "Go to" (FR-055): a track, a physical or DOS 3.3 sector, a ProDOS block,
//  a nibble offset or a cell, each entered in the base the inspector shows
//  it in. OK selects it; an entry that finds nothing says why and stays
//  open.
//
////////////////////////////////////////////////////////////////////////////////

class GoToDialog : public DxuiDialogWindow
{
public:
    static constexpr int  kKindCount = 7;

    void  Configure (const IDxuiTheme * theme, const DiskAnalysis & analysis, int quarterTrack, GoToKind kind);

    bool                 IsChosen  () const { return m_isChosen; }
    GoToKind             GetKind   () const { return m_kind; }
    const GoToTarget &   GetTarget () const { return m_target; }

    static constexpr SIZE  kSizeDip = { 420, 400 };

protected:
    void  OnCreate () override;

private:
    void  OnOkClicked ();

    const IDxuiTheme *    m_theme        = nullptr;   // non-owning
    const DiskAnalysis *  m_analysis     = nullptr;   // non-owning
    int                   m_quarterTrack = 0;
    GoToKind              m_kind         = GoToKind::Track;
    GoToTarget            m_target;
    bool                  m_isChosen     = false;

    DxuiRadioGroup        m_kinds;
    DxuiTextInput         m_input;
    DxuiLabel             m_error;
};
