#include "Pch.h"

#include "Ui/Debugger/Panes/CommLogController.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/CommGraph.h"
#include "Ui/Debugger/Panes/CommLogPane.h"
#include "Ui/Debugger/Panes/CommSignalGraph.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





static constexpr int  s_kCommZoomIn  = 20;
static constexpr int  s_kCommZoomOut = 21;
static constexpr int  s_kCommLane    = 30;     // plus the lane





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::CommLogController
//
////////////////////////////////////////////////////////////////////////////////

CommLogController::CommLogController (IDebuggerPaneHost & host, DebugLogKind kind) :
    EventLogController (host,
                        (kind == DebugLogKind::Network) ? DebuggerLayout::kNetworkLog : DebuggerLayout::kSerialLog,
                        (kind == DebugLogKind::Network) ? L"Network log" : L"Serial log",
                        kind)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::~CommLogController
//
////////////////////////////////////////////////////////////////////////////////

CommLogController::~CommLogController() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::CreateControls
//
////////////////////////////////////////////////////////////////////////////////

void CommLogController::CreateControls()
{
    CreateListAndBar();

    m_graph = m_host.GetControlParent().CreateChild<CommSignalGraph>();
    m_model = std::make_unique<CommLogPane> (m_list, [this] (const std::string & request)
    {
        if (GetWindowHost() != nullptr)
        {
            GetWindowHost()->SendDebuggerEventLogRequest (m_kind, request);
        }
    }, IsNetwork());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void CommLogController::Configure()
{
    m_model->Configure();
    (void) LoadOptions();

    ConfigureBar();
    SetBarEntries();
    UpdateGraph();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::AddToDock
//
////////////////////////////////////////////////////////////////////////////////

void CommLogController::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (m_title);
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_graph, [this] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (m_graph->GetHeightDip()); });
    m_frame->AddPart (m_list);

    dock.AddPane (m_id, m_title, m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> CommLogController::GetControls() const
{
    return { m_bar.GetSlot(), m_graph, m_list, m_bar.GetBar() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::SetBarEntries
//
//  A toggle for each lane, then Zoom in and Zoom out as glyphs.
//
////////////////////////////////////////////////////////////////////////////////

void CommLogController::SetBarEntries()
{
    std::vector<DxuiToolbar::Entry>  entries;
    std::shared_ptr<DxuiCommand>     command;
    size_t                           count   = IsNetwork() ? CommGraph::kSocketCount : CommGraph::kSerialLaneCount;
    std::wstring                     label;



    m_commands.clear();

    for (size_t lane = 0; lane < count; lane++)
    {
        label = IsNetwork() ? std::format (L"Socket {}", lane) : std::wstring (CommGraph::GetSerialLaneName (lane));

        command            = AddEntry (entries, s_kCommLane + (int) lane, label.c_str(), (L"Show " + label + L" in the graph").c_str(), DxuiToolbar::Kind::Toggle, 0);
        command->isChecked = [this, lane] { return m_model->IsLaneShown (lane); };
        command->dispatch  = [this, lane] { ShowLane (lane, !m_model->IsLaneShown (lane)); };
    }

    command           = AddEntry (entries, s_kCommZoomIn, L"Zoom in", L"Zoom in: show half as many cycles", DxuiToolbar::Kind::Command, 1);
    command->glyph    = s_kpszMdl2ZoomIn;
    command->dispatch = [this] { Zoom (true); };
    entries.back().iconOnly = true;

    command           = AddEntry (entries, s_kCommZoomOut, L"Zoom out", L"Zoom out: show twice as many cycles", DxuiToolbar::Kind::Command, 1);
    command->glyph    = s_kpszMdl2ZoomOut;
    command->dispatch = [this] { Zoom (false); };
    entries.back().iconOnly = true;

    m_bar.GetBar()->SetEntries (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::Zoom
//
////////////////////////////////////////////////////////////////////////////////

void CommLogController::Zoom (bool isIn)
{
    m_span = std::clamp (isIn ? m_span / 2 : m_span * 2, kMinSpan, kMaxSpan);
    m_graphedCount = SIZE_MAX;

    UpdateGraph();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::ShowLane
//
//  The graph's height follows the lanes shown, so the pane is laid out again.
//
////////////////////////////////////////////////////////////////////////////////

void CommLogController::ShowLane (size_t lane, bool isShown)
{
    m_model->SetLaneShown (lane, isShown);
    m_graphedCount = SIZE_MAX;

    UpdateGraph();
    SaveOptions();
    m_host.RelayoutDock();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::ApplyLogDeltas
//
////////////////////////////////////////////////////////////////////////////////

void CommLogController::ApplyLogDeltas (const DebugLogDeltas & deltas, bool isWindowVisible)
{
    for (const IoLogDelta & delta : deltas.GetIo (m_kind))
    {
        m_model->Apply (delta);
    }

    m_model->SetShown       (isWindowVisible && m_host.IsPaneOpen (m_id));
    m_model->SetShownBefore (m_shownBefore);
    m_model->Refresh();
    m_model->FollowScroll();

    UpdateGraph();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::UpdateGraph
//
//  Built again only when the rows, the keyboard's row or the position the
//  machine was taken back to changed, or the span or the lanes did, since
//  the build reads every row. Behind live it reads only the rows before the
//  position, the ones the list has.
//
////////////////////////////////////////////////////////////////////////////////

void CommLogController::UpdateGraph()
{
    const std::vector<IoLogRow>  & all      = m_model->GetRows().GetRows();
    int                            selected = m_list->GetSelectedRow();
    const std::vector<int>       & view     = m_model->GetView();
    std::vector<IoLogRow>          before;
    std::optional<size_t>          anchor;
    std::vector<CommLane>          lanes;
    uint64_t                       first    = 0;
    uint64_t                       last     = 0;
    Byte                           port     = 0;
    size_t                         shown    = all.size();



    if (selected == m_graphedRow && all.size() == m_graphedCount && m_shownBefore == m_graphedBefore)
    {
        return;
    }

    m_graphedRow    = selected;
    m_graphedCount  = all.size();
    m_graphedBefore = m_shownBefore;

    while (shown > 0 && all[shown - 1].position >= m_shownBefore)
    {
        shown--;
    }

    if (shown < all.size())
    {
        before.assign (all.begin(), all.begin() + (ptrdiff_t) shown);
    }

    const std::vector<IoLogRow>  & rows = (shown < all.size()) ? before : all;


    if (selected >= 0 && (size_t) selected < view.size() && view[(size_t) selected] >= 0)
    {
        anchor = (size_t) view[(size_t) selected];
    }

    for (size_t i = rows.size(); !anchor.has_value() && i > 0; i--)
    {
        anchor = (rows[i - 1].kind == DebugLogRowKind::Event) ? std::optional<size_t> (i - 1) : std::nullopt;
    }

    if (anchor.has_value())
    {
        const IoLogRow  & row = rows[*anchor];

        port  = row.event.chip;
        last  = (selected >= 0) ? row.event.cycle + m_span / 2 : row.event.cycle + m_span / 20;
        first = (last > m_span) ? last - m_span : 0;
    }
    else
    {
        last = m_span;
    }

    if (IsNetwork())
    {
        CommGraph::BuildNetwork (rows, first, last, lanes);
    }
    else
    {
        CommGraph::BuildSerial (rows, port, first, last, lanes);
    }

    m_graph->SetLanes (std::move (lanes), m_model->GetLanesShown(), first, last, IsNetwork());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController::TryGetTip
//
//  The graph's lanes at the pointer, a line each, following the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool CommLogController::TryGetTip (POINT clientPx, PaneTip & tip)
{
    if (m_graph == nullptr || !m_host.IsRoutable (m_graph) || !m_graph->TryGetTipAt (clientPx, tip.text))
    {
        return false;
    }

    tip.kind = PaneTip::Kind::Pointer;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController model access
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CommLogController::GetModelText (bool isSelectedOnly) const  { return m_model->GetText (isSelectedOnly); }
std::string  CommLogController::GetOptionsText() const                    { return m_model->GetOptionsText(); }
void         CommLogController::SetOptionsText (const std::string & text) { m_model->SetOptionsText (text); }
void         CommLogController::SetModelShown (bool isShown)              { m_model->SetShown (isShown); }
size_t       CommLogController::GetTextColumn() const                     { return CommLogPane::kHex; }
