#include "Pch.h"

#include "Ui/Debugger/Panes/DiagnosticsPane.h"

#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsPane::DiagnosticsPane
//
//  The band for the info button comes first, shown while the graphic has a
//  color key. Each graphic is a part of the frame shown only while the
//  payload is of its kind, and the list takes the height that is left.
//
////////////////////////////////////////////////////////////////////////////////

DiagnosticsPane::DiagnosticsPane (
    std::string      id,
    std::wstring     title,
    DxuiListView   * list,
    MemoryMapBar   * map,
    DiskHeadView   * head,
    MeterBar       * meters) :
    m_id      (std::move (id)),
    m_title   (std::move (title)),
    m_list    (list),
    m_map     (map),
    m_head    (head),
    m_meters  (meters),
    m_frame   (std::make_unique<DebuggerPaneFrame> (m_title)),
    m_keySlot (std::make_unique<DebuggerPaneFrame> (L"Colors"))
{
    m_frame->AddPart (m_keySlot.get(),
                      [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (DxuiToolbar::kCompactBandDp); },
                      [this] { return GetColorKey().has_value(); });
    m_frame->AddPart (m_map,
                      [this] (int width, const DxuiDpiScaler & scaler) { return m_map->GetPreferredHeightPx (width, scaler); },
                      [this] { return m_visual == Visual::MemoryMap; });
    m_frame->AddPart (m_head,
                      [this] (int width, const DxuiDpiScaler & scaler) { return m_head->GetPreferredHeightPx (width, scaler); },
                      [this] { return m_visual == Visual::DiskHead; });
    m_frame->AddPart (m_meters,
                      [this] (int, const DxuiDpiScaler & scaler) { return m_meters->GetPreferredHeightPx (scaler); },
                      [this] { return m_visual == Visual::Meters; });
    m_frame->AddPart (m_list);

    m_frame->SetPartInsetDip (m_map,    kGraphicInsetDip);
    m_frame->SetPartInsetDip (m_head,   kGraphicInsetDip);
    m_frame->SetPartInsetDip (m_meters, kGraphicInsetDip);

    m_map->SetVisible    (false);
    m_head->SetVisible   (false);
    m_meters->SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DiagnosticsPane::GetControls() const
{
    return { m_keySlot.get(), m_map, m_head, m_meters, m_list };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsPane::GetColorKey
//
//  A memory map's sources and a disk head's states are colors; the meters
//  and the list are not.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ColorLegend::Pane> DiagnosticsPane::GetColorKey() const
{
    switch (m_visual)
    {
    case Visual::MemoryMap: return ColorLegend::Pane::MemoryMap;
    case Visual::DiskHead:  return ColorLegend::Pane::DiskHead;
    default:                return std::nullopt;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void DiagnosticsPane::Configure()
{
    m_list->SetColumns ({ { m_title,  0, false, DxuiTextHAlign::Left },
                          { L"Value", 0, false, DxuiTextHAlign::Left },
                          { L"Bits",  0, false, DxuiTextHAlign::Left } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsPane::Apply
//
//  The graphic takes its payload whatever kind it is; only a change of kind
//  moves anything.
//
////////////////////////////////////////////////////////////////////////////////

bool DiagnosticsPane::Apply (const DiagnosticsSnapshot & snapshot)
{
    Visual  visual  = Visual::None;
    bool    resized = false;



    m_list->SetRows (MakeRows (snapshot));

    if (const DiagnosticsMemoryMap * map = std::get_if<DiagnosticsMemoryMap> (&snapshot.visual))
    {
        m_map->SetMap (*map);
        visual = Visual::MemoryMap;
    }
    else if (const DiagnosticsDiskHead * head = std::get_if<DiagnosticsDiskHead> (&snapshot.visual))
    {
        m_head->SetHead (*head);
        visual = Visual::DiskHead;
    }
    else if (const DiagnosticsMeters * meters = std::get_if<DiagnosticsMeters> (&snapshot.visual))
    {
        //  A change in the number of meters changes the height the frame gives
        //  them, so it lays out again as for a change of kind.
        resized = m_meters->GetMeters().levels.size() != meters->levels.size();
        m_meters->SetMeters (*meters);
        visual  = Visual::Meters;
    }

    if (visual == m_visual && !resized)
    {
        return false;
    }

    m_visual = visual;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsPane::MakeRows
//
//  A group's title stands alone on its row; its rows are indented under it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> DiagnosticsPane::MakeRows (const DiagnosticsSnapshot & snapshot)
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;



    for (const DiagnosticsGroup & group : snapshot.groups)
    {
        rows.push_back ({ { TextEncoding::NarrowToWide (group.title) }, { L"" }, { L"" } });

        for (const DiagnosticsRow & row : group.rows)
        {
            rows.push_back ({ { L"  " + TextEncoding::NarrowToWide (row.label) },
                              MakeValueCell (row),
                              MakeBitsCell (row.bits) });
        }
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsPane::MakeValueCell
//
//  A switch that is on reads as ON in the list's full-strength text, and one
//  that is off as off in its muted text, so the set switches stand out.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Cell DiagnosticsPane::MakeValueCell (const DiagnosticsRow & row)
{
    DxuiListView::Cell  cell;
    bool                isOn = row.value == "on";



    if (!row.isSwitch)
    {
        cell.text = TextEncoding::NarrowToWide (row.value);
        return cell;
    }

    cell.text = isOn ? L"ON" : L"off";
    cell.dim  = !isOn;

    return cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsPane::MakeBitsCell
//
//  The state of each bit is its name's color, so the decode reads at a glance
//  and every name keeps its place as bits change.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Cell DiagnosticsPane::MakeBitsCell (const std::vector<DiagnosticsBit> & bits)
{
    DxuiListView::Cell  cell;



    for (const DiagnosticsBit & bit : bits)
    {
        std::wstring  name  = TextEncoding::NarrowToWide (bit.name);
        int           first = 0;

        if (!cell.text.empty())
        {
            cell.text += L' ';
        }

        first      = (int) cell.text.size();
        cell.text += name;

        if (!bit.set)
        {
            cell.dimRanges.emplace_back (first, (int) cell.text.size());
        }
    }

    return cell;
}
