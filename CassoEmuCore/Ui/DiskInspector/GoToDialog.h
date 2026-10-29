#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorSearch.h"

class GoToBody;





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog
//
//  "Go to" (FR-055): a track, a physical or DOS 3.3 sector, a ProDOS block,
//  a nibble offset or a cell, each entered in the base the inspector shows
//  it in. A sector, offset or cell is entered with its track. OK selects
//  it; after an entry that finds nothing, the dialog stays open with the
//  reason under the entry.
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
    void      OnOkClicked     ();
    void      ShowHint        (GoToKind kind);
    GoToKind  GetChosenKind   () const;
    int       GetEnteredTrack () const;

    static LPCWSTR  GetEntryLabel (GoToKind kind);

    const IDxuiTheme *    m_theme        = nullptr;   // non-owning
    const DiskAnalysis *  m_analysis     = nullptr;   // non-owning
    int                   m_quarterTrack = 0;
    GoToKind              m_kind         = GoToKind::Track;
    GoToTarget            m_target;
    bool                  m_isChosen     = false;

    GoToBody *            m_body         = nullptr;   // non-owning
    DxuiRadioGroup        m_kinds;
    DxuiLabel             m_trackLabel;
    DxuiTextInput         m_track;
    DxuiLabel             m_entryLabel;
    DxuiTextInput         m_input;
    DxuiLabel             m_hint;
    DxuiLabel             m_error;
};
