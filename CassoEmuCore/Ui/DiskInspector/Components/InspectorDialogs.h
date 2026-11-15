#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Ui/DiskInspector/Components/IInspectorComponent.h"
#include "Devices/Disk/Inspector/InspectorSearch.h"

class IInspectorComponentHost;
class InspectorSession;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorDialogs
//
//  The inspector's dialogs: Go to (FR-055), Export (FR-058), the decode
//  settings with the change they make to the analysis (FR-019), Repair's
//  confirmation, and the confirmations.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorDialogs : public IInspectorComponent
{
public:
    InspectorDialogs (InspectorSession & session, IInspectorComponentHost & host);

    void  Create     () override {}
    void  GetPrefs   (DiskInspectorPrefs & inOut) const override;
    void  ApplyPrefs (const DiskInspectorPrefs & prefs) override;

    void  OpenGoTo           ();
    void  OpenExport         ();
    void  OpenDecodeSettings ();
    void  OpenRepair         (const std::set<std::wstring> & chosen);
    void  ApplySettings      (const DecodeSettings & settings);

    //  A modal question with a button for the action and one for Cancel;
    //  true for the action. Without Cancel, a report with the one button.
    bool  Confirm (const std::wstring & title, const vector<std::wstring> & lines, const std::wstring & action, bool hasCancel = true);

private:
    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    GoToKind                   m_goToKind = GoToKind::Track;
};