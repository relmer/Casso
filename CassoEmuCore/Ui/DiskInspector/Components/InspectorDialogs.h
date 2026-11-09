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
//  settings with the change they make to the analysis (FR-019), and the
//  confirmations.
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
    void  ApplySettings      (const DecodeSettings & settings);

    //  A modal question with a button for the action and one for Cancel;
    //  true for the action.
    bool  Confirm (const std::wstring & title, const vector<std::wstring> & lines, const std::wstring & action);

private:
    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    GoToKind                   m_goToKind = GoToKind::Track;
};