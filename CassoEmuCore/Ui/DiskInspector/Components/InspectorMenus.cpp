#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorMenus.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::InspectorMenus
//
////////////////////////////////////////////////////////////////////////////////

InspectorMenus::InspectorMenus (InspectorSession & session, IInspectorComponentHost & host, const InspectorCommands & commands) :
    m_session  (session),
    m_host     (host),
    m_commands (commands)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::Create
//
////////////////////////////////////////////////////////////////////////////////

void InspectorMenus::Create()
{
    m_bar.Create (m_host.GetControlParent(), m_host.GetPopupHost(), m_host.GetTextRenderer(), m_commands);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::Layout
//
////////////////////////////////////////////////////////////////////////////////

void InspectorMenus::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    m_bar.Layout (area, m_host.GetControlParent().GetBounds(), scaler);
}
