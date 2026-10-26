#include "Pch.h"

#include "Ui/Debugger/Panes/CallStackController.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/BasicPanes.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackController::CallStackController
//
////////////////////////////////////////////////////////////////////////////////

CallStackController::CallStackController (IDebuggerPaneHost & host) :
    IDebuggerPane (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackController::~CallStackController
//
////////////////////////////////////////////////////////////////////////////////

CallStackController::~CallStackController() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackController::GetId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CallStackController::GetId() const
{
    return DebuggerLayout::kCallStack;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackController::CreateControls
//
////////////////////////////////////////////////////////////////////////////////

void CallStackController::CreateControls()
{
    DxuiPanel  & parent = m_host.GetControlParent();



    m_list   = parent.CreateChild<DxuiListView>();
    m_button = parent.CreateChild<DxuiButton> (L"Hybrid");

    m_model = std::make_unique<CallStackPane> (
        m_list, m_button,
        [this] (const DebuggerActionBuilder & build) { m_host.RunAction (build (m_host.GetMode())); },
        [this] (Word address)                        { m_host.GoToCode (address); },
        [this] (Word line)                           { m_host.ShowBasicLine (line); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void CallStackController::Configure()
{
    m_model->Configure();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackController::AddToDock
//
//  The call stack has no toolbar and no color key: its Found by column has
//  what each dimmed row is, and the row's tip has why.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackController::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (L"Call stack");
    m_button->SetVisible (false);
    m_frame->AddPart (m_list);

    dock.AddPane (DebuggerLayout::kCallStack, L"Call stack", m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackController::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * CallStackController::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackController::Apply
//
////////////////////////////////////////////////////////////////////////////////

void CallStackController::Apply (const DebuggerViewSnapshot & snapshot)
{
    m_model->SetColors (m_host.GetTextColors());
    m_model->Apply     (snapshot.callStack, BasicWatches::IsStoppedInProgram (snapshot.basic) ? snapshot.basic->frames : std::vector<BasicFrame>());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackController::AddMenuItems
//
//  The pane's own rows: the BASIC frames come before the machine's.
//
////////////////////////////////////////////////////////////////////////////////

bool CallStackController::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    CallStackPane::Row  frame;



    if (list != m_list)
    {
        return false;
    }

    if (row < 0 || row >= (int) m_model->GetShownRows().size())
    {
        return true;
    }

    frame = m_model->GetShownRows()[(size_t) row];

    if (!frame.basicLine.has_value())
    {
        items.push_back ({ L"Show call site", [this, at = frame.address] { m_host.GoToCode (at); } });
    }

    if (frame.routineAddress.has_value())
    {
        items.push_back ({ L"Never step into this routine", [this, frame]
        {
            m_host.RunAction (DebuggerActions::GetSkipRoutine (frame.routineSymbol, *frame.routineAddress, m_host.GetMode()));
        } });
    }

    items.push_back ({ L"Copy", [this] { DxuiClipboard::SetText (m_host.GetOwnerHwnd(), m_list->GetSelectionText()); } });
    return true;
}
