#include "Pch.h"

#include "Ui/Debugger/Panes/TracePaneController.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/KeyHintLine.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/TracePane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::TracePaneController
//
////////////////////////////////////////////////////////////////////////////////

TracePaneController::TracePaneController (IDebuggerPaneHost & host) :
    IDebuggerPane (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::~TracePaneController
//
////////////////////////////////////////////////////////////////////////////////

TracePaneController::~TracePaneController() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::GetId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TracePaneController::GetId() const
{
    return DebuggerLayout::kTrace;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::CreateControls
//
//  The rows' model sends the top row the list scrolled to, for the machine
//  to read the entries around it.
//
////////////////////////////////////////////////////////////////////////////////

void TracePaneController::CreateControls()
{
    DxuiPanel  & parent = m_host.GetControlParent();



    m_list = parent.CreateChild<DxuiListView>();
    m_hint = parent.CreateChild<KeyHintLine>();

    m_model = std::make_unique<TracePane> (m_list, [this] (std::optional<uint64_t> first)
    {
        IDebuggerWindowHost  * host = m_host.GetWindowHost();

        if (host != nullptr)
        {
            host->SetDebuggerTraceTop (first);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void TracePaneController::Configure()
{
    m_model->Configure();
    m_hint->SetPairs  (TracePane::GetKeyPairs());
    m_hint->SetOnPick ([this] (int id) { (void) RunTraceAction (id); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::ConfigureLists
//
//  The trace's rows come from a provider, which a precise fit never
//  measures, so its columns are fitted to the text of the rows drawn.
//
////////////////////////////////////////////////////////////////////////////////

void TracePaneController::ConfigureLists()
{
    m_list->SetPreciseAutoFit (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::AddToDock
//
//  The keys over the rows, their text where the pane's title starts.
//
////////////////////////////////////////////////////////////////////////////////

void TracePaneController::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (L"Trace");
    m_frame->AddPart (m_hint, [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (kHintDip); });
    m_frame->AddPart (m_list);
    m_frame->SetPartTextAligned (m_hint);

    dock.AddPane (DebuggerLayout::kTrace, L"Trace", m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * TracePaneController::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> TracePaneController::GetControls() const
{
    return { m_hint, m_list };
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::Apply
//
////////////////////////////////////////////////////////////////////////////////

void TracePaneController::Apply (const DebuggerViewSnapshot & snapshot)
{
    m_model->SetColors  (m_host.GetTextColors());
    m_model->SetPcStyle (m_host.GetPcMarker (nullptr), m_host.GetPcRowArgb());
    m_model->Apply      (snapshot.trace);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::Tick
//
//  Requests the entries around the row the list scrolled to from the
//  machine.
//
////////////////////////////////////////////////////////////////////////////////

bool TracePaneController::Tick (int64_t nowMs)
{
    (void) nowMs;

    m_model->FollowScroll();
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::RouteContentMouse
//
//  The key hints light under the pointer; each is a button, and a press on
//  one moves the focus to the rows.
//
////////////////////////////////////////////////////////////////////////////////

bool TracePaneController::RouteContentMouse (const DxuiMouseEvent & ev)
{
    if (!m_host.IsRoutable (m_hint))
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        if (m_hint->OnMouse (ev))
        {
            m_host.Repaint();
        }

        return false;

    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left && m_hint->IsVisible() && m_hint->OnMouse (ev))
        {
            m_host.FocusControl (m_list);
            m_host.Repaint();
            return true;
        }

        return false;

    case DxuiMouseEventKind::Up:
        if (m_hint->OnMouse (ev))
        {
            m_host.Repaint();
            return true;
        }

        return false;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::RouteEditKey
//
//  The pane's own keys while its rows have the focus. Each step and Run goes
//  the way its key-scheme key does.
//
////////////////////////////////////////////////////////////////////////////////

bool TracePaneController::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    if (ev.kind != DxuiKeyEventKind::Down || focused != m_list)
    {
        return false;
    }

    return RunTraceAction ((int) TracePane::GetKeyAction (ev.vk, ev.ctrl, ev.alt, ev.shift));
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::RunTraceAction
//
////////////////////////////////////////////////////////////////////////////////

bool TracePaneController::RunTraceAction (int action)
{
    using Action = DebuggerKeySchemes::Action;



    switch ((TracePane::KeyAction) action)
    {
    case TracePane::KeyAction::StepInto:    return m_host.RunKeyAction (Action::StepInto);
    case TracePane::KeyAction::StepOver:    return m_host.RunKeyAction (Action::StepOver);
    case TracePane::KeyAction::StepOut:     return m_host.RunKeyAction (Action::StepOut);
    case TracePane::KeyAction::Run:         return m_host.RunKeyAction (Action::Run);
    case TracePane::KeyAction::ToggleTrace: m_host.RunCommandBarEntry (DebuggerCommands::kTrace); return true;
    case TracePane::KeyAction::ToggleBytes: m_model->ToggleBytes(); m_host.Repaint();             return true;
    case TracePane::KeyAction::Save:        Save();                                               return true;
    default:                                                                                      return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::Save
//
//  Backing out of the picker saves nothing.
//
////////////////////////////////////////////////////////////////////////////////

void TracePaneController::Save()
{
    HRESULT                hr      = S_OK;
    IHostDialogs         * dialogs = m_host.GetHostDialogs();
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked  = false;



    BAIL_OUT_IF (dialogs == nullptr, S_OK);

    spec.filters          = { { L"Text files", L"*.txt" }, { L"All files", L"*.*" } };
    spec.defaultExtension = L"txt";
    spec.defaultFileName  = L"trace.txt";

    hr = dialogs->PickFileToSave (m_host.GetOwnerHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    m_host.RunAction (DebuggerActions::GetSaveHistory (TextEncoding::WideToNarrow (chosen.wstring()), m_host.GetMode()));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController::AddMenuItems
//
////////////////////////////////////////////////////////////////////////////////

bool TracePaneController::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    (void) row;

    if (list != m_list)
    {
        return false;
    }

    items.push_back ({ L"Copy", [this] { DxuiClipboard::SetText (m_host.GetOwnerHwnd(), m_list->GetSelectionText()); } });
    return true;
}
