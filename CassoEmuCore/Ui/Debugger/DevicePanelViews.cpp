#include "Pch.h"

#include "Ui/Debugger/DevicePanelViews.h"

#include "Debugger/DebugSession.h"
#include "Debugger/IDiagnosticsProvider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelViews::BuildPanels
//
//  Every provider the machine has is listed; only open panels are built, so a
//  device pays for its rows only while someone watches them. A panel whose
//  device is gone closes.
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelViews::BuildPanels (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    std::vector<const IDiagnosticsProvider *>  providers = session.GetTarget().GetDiagnosticsProviders();
    std::set<std::string>                      present;



    for (const IDiagnosticsProvider * provider : providers)
    {
        DebuggerViewSnapshot::PanelInfo  info  { provider->GetDiagnosticsId(), provider->GetDiagnosticsTitle(), false };
        DiagnosticsSnapshot              panel;

        present.insert (info.id);
        info.open = m_openPanels.contains (info.id);

        if (info.open)
        {
            panel.id     = info.id;
            panel.device = info.title;
            provider->GetDiagnostics (panel);
            snapshot.diagnostics.push_back (std::move (panel));
        }

        snapshot.panels.push_back (std::move (info));
    }

    std::erase_if (m_openPanels, [&present] (const std::string & id) { return !present.contains (id); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelViews::RunPanelCommand
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelViews::RunPanelCommand (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::vector<const IDiagnosticsProvider *>  providers = session.GetTarget().GetDiagnosticsProviders();
    constexpr int                              kIdWidth  = 14;
    const IDiagnosticsProvider               * provider  = nullptr;
    MessageData                                message;



    if (command.verb == DebugVerb::ListPanels)
    {
        for (const IDiagnosticsProvider * each : providers)
        {
            std::string  id = each->GetDiagnosticsId();

            message.lines.push_back (std::format ("{:<{}}{}{}", id, kIdWidth, each->GetDiagnosticsTitle(), m_openPanels.contains (id) ? " (open)" : ""));
        }

        if (providers.empty())
        {
            message.lines.push_back ("This machine has no device panels.");
        }

        reply.data = std::move (message);
        return;
    }

    provider = FindProvider (providers, command.text);

    if (provider == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no such panel",
                        std::format ("This machine has no {} panel. Use PANEL LIST to list the ones it has.", command.text));
        return;
    }

    if (command.verb == DebugVerb::ClosePanel)
    {
        ClosePanel (provider->GetDiagnosticsId());
        reply.data = MessageData { { std::format ("The {} panel is closed.", provider->GetDiagnosticsTitle()) } };
    }
    else
    {
        OpenPanel (provider->GetDiagnosticsId());
        reply.data = MessageData { { std::format ("The {} panel is open.", provider->GetDiagnosticsTitle()) } };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelViews::FindProvider
//
//  By id or by title, either case.
//
////////////////////////////////////////////////////////////////////////////////

const IDiagnosticsProvider * DevicePanelViews::FindProvider (const std::vector<const IDiagnosticsProvider *> & providers, const std::string & name)
{
    auto  isSame = [] (const std::string & a, const std::string & b)
    {
        return a.size() == b.size() &&
               std::equal (a.begin(), a.end(), b.begin(), [] (char x, char y) { return tolower ((unsigned char) x) == tolower ((unsigned char) y); });
    };



    for (const IDiagnosticsProvider * provider : providers)
    {
        if (isSame (provider->GetDiagnosticsId(), name) || isSame (provider->GetDiagnosticsTitle(), name))
        {
            return provider;
        }
    }

    return nullptr;
}
