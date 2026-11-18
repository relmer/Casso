#pragma once

#include "Ui/Debugger/DebuggerViewSnapshot.h"

class DebugSession;
class IDiagnosticsProvider;
struct DebugCommand;





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelViews
//
//  Device panels by provider id. A panel stays open until closed or until
//  its device leaves the machine; only open panels cost the devices
//  anything, since a closed one is never asked for its rows.
//
////////////////////////////////////////////////////////////////////////////////

class DevicePanelViews
{
public:
    void  OpenPanel   (const std::string & id)       { m_openPanels.insert (id); }
    void  ClosePanel  (const std::string & id)       { m_openPanels.erase (id); }
    bool  IsPanelOpen (const std::string & id) const { return m_openPanels.contains (id); }

    void  BuildPanels     (DebugSession & session, DebuggerViewSnapshot & snapshot) const;

    //  PANEL LIST, PANEL OPEN and PANEL CLOSE.
    void  RunPanelCommand (DebugSession & session, const DebugCommand & command, Reply & reply);

private:
    static const IDiagnosticsProvider *  FindProvider (const std::vector<const IDiagnosticsProvider *> & providers, const std::string & name);

    //  Mutable so a build can close the panel of a device that has left.
    mutable std::set<std::string>  m_openPanels;
};
