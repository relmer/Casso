#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorSearch.h"

class GoToBody;





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog
//
//  "Go to" (FR-055): a track, a physical sector or DOS-order sector index, a ProDOS block,
//  a nibble offset or a cell, each entered in the base the inspector shows
//  it in, or a file chosen from a list. A sector, offset or cell is entered with its track. OK selects
//  it; an entry that finds nothing has the reason under it as it is typed,
//  and OK is disabled.
//
////////////////////////////////////////////////////////////////////////////////

class GoToDialog : public DxuiDialogWindow
{
public:
    static constexpr int  kKindCount = 7;
    static constexpr int  kTipCount  = 3;

    void  Configure (const IDxuiTheme * theme, const DiskAnalysis & analysis, int quarterTrack, GoToKind kind);

    bool                 IsChosen  () const { return m_isChosen; }
    GoToKind             GetKind   () const { return m_kind; }
    const GoToTarget &   GetTarget () const { return m_target; }

    static constexpr SIZE  kSizeDip = { 420, 400 };

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    void      OnOkClicked     ();
    void      ShowHint        (GoToKind kind);
    void      FillFiles       ();
    bool      Validate        ();
    IDxuiControl * GetEntryControl (GoToKind kind);
    GoToKind  GetChosenKind   () const;
    int       GetEnteredTrack () const;

    static LPCWSTR                          GetEntryLabel      (GoToKind kind);
    static vector<vector<std::wstring>>     BuildDosOrderTable ();

    const IDxuiTheme *    m_theme        = nullptr;   // non-owning
    const DiskAnalysis *  m_analysis     = nullptr;   // non-owning
    int                   m_quarterTrack = 0;
    GoToKind              m_kind         = GoToKind::Track;
    GoToTarget            m_target;
    bool                  m_isChosen     = false;

    GoToBody *            m_body         = nullptr;   // non-owning
    vector<GoToKind>      m_shownKinds;
    DxuiRadioGroup        m_kinds;
    DxuiInfoTip           m_tips[kTipCount];
    DxuiButton *          m_ok           = nullptr;   // non-owning
    DxuiLabel             m_trackLabel;
    DxuiTextInput         m_track;
    DxuiLabel             m_entryLabel;
    DxuiTextInput         m_input;
    DxuiComboBox          m_files;
    vector<std::wstring>  m_paths;
    DxuiLabel             m_hint;
    DxuiLabel             m_error;
};
