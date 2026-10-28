#include "Pch.h"

#include "Ui/DiskInspector/Components/SelectionPart.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorTableView.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/TrackStripView.h"
#include "Ui/DiskInspector/Views/DifferencesView.h"
#include "Ui/DiskInspector/Views/FilesView.h"
#include "Ui/DiskInspector/Views/FluxView.h"
#include "Ui/DiskInspector/Views/IssuesView.h"
#include "Ui/DiskInspector/Views/NibblesView.h"
#include "Ui/DiskInspector/Components/InspectorDialogs.h"
#include "Ui/DiskInspector/Components/StripControls.h"
#include "Ui/DiskInspector/Components/FindPart.h"
#include "Devices/Disk/Inspector/InspectorClipboard.h"
#include "Ui/DiskInspector/Components/FindPart.h"
#include "Ui/DiskInspector/Components/StripControls.h"
#include "Ui/DiskInspector/FindBar.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/Views/SectorsView.h"
#include "Ui/DiskInspector/SectorRowView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::SelectionPart
//
////////////////////////////////////////////////////////////////////////////////

SelectionPart::SelectionPart (InspectorSession & session, IInspectorComponentHost & host, InspectorDialogs & dialogs, FindPart & find, StripControls & stripControls) :
    m_session       (session),
    m_host          (host),
    m_dialogs       (dialogs),
    m_findPart      (find),
    m_stripControls (stripControls)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::StepSector
//
//  The previous or next sector in passing order, wrapping at the index, or
//  the first or last.
//
////////////////////////////////////////////////////////////////////////////////

void SelectionPart::StepSector (int delta, bool isToEnd)
{
    const TrackAnalysis *  track = m_session.model.GetTrack();
    vector<int>            order = (track != nullptr) ? SectorRowView::GetPassingOrder (*track) : vector<int>();
    int                    count = static_cast<int> (order.size());
    int                    at    = 0;



    if (count > 0)
    {
        at = static_cast<int> (std::find (order.begin(), order.end(), m_session.model.GetSectorIndex()) - order.begin());
        at = isToEnd ? (delta < 0 ? 0 : count - 1) : (at + delta + count) % count;

        m_session.model.SelectSector (m_session.model.GetQuarterTrack(), order[at]);
        m_host.OnSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::StepHistory
//
//  Back or Forward through the user's selections (FR-164). The selection
//  stepped to is not added to the history again.
//
////////////////////////////////////////////////////////////////////////////////

void SelectionPart::StepHistory (bool isForward)
{
    SelectionHistory &  history   = m_session.model.GetHistory();
    InspectorSelection  selection;
    bool                isMoved   = isForward ? history.GoForward (selection) : history.GoBack (selection);



    if (isMoved)
    {
        m_session.model.RestoreSelection (selection);
        m_session.isSteppingHistory = true;
m_host.OnSelection();
        m_session.isSteppingHistory = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::RecordSelection
//
//  The user's selection is added to the history for Back and Forward
//  (FR-164), unless Back, Forward or Follow head made it.
//
////////////////////////////////////////////////////////////////////////////////

void SelectionPart::RecordSelection (bool isHeadSelecting)
{
    if (!m_session.isSteppingHistory && !isHeadSelecting && (m_session.model.GetSector() != nullptr || m_session.model.GetFirstNibble() >= 0))
    {
        m_session.model.GetHistory().Record (m_session.model.GetSelection());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::OnHistoryButton
//
//  The mouse's back and forward buttons step through the selections
//  (FR-164).
//
////////////////////////////////////////////////////////////////////////////////

bool SelectionPart::OnHistoryButton (const DxuiMouseEvent & ev)
{
    bool  isHistory = ev.button == DxuiMouseButton::Back || ev.button == DxuiMouseButton::Forward;



    if (isHistory && ev.kind == DxuiMouseEventKind::Up && m_session.context.hasDisk)
    {
        StepHistory (ev.button == DxuiMouseButton::Forward);
    }

    return isHistory;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::IsStripKey
//
////////////////////////////////////////////////////////////////////////////////

bool SelectionPart::IsStripKey (WPARAM vk)
{
    return vk == VK_OEM_PLUS || vk == VK_ADD || vk == VK_OEM_MINUS || vk == VK_SUBTRACT || vk == VK_LEFT || vk == VK_RIGHT || vk == '0' || vk == VK_NUMPAD0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::GetKeyTarget
//
//  The view under a press, for the keys that act on the view pressed last.
//
////////////////////////////////////////////////////////////////////////////////

KeyTarget SelectionPart::GetKeyTarget (POINT pointPx) const
{
    KeyTarget  target = KeyTarget::Platter;



    if (m_findPart.IsBarAt (pointPx))
    {
        target = KeyTarget::FindBar;
    }
    else if (IsInside (m_session.views.strip->GetBounds(), pointPx) || (m_session.views.stripB->IsVisible() && IsInside (m_session.views.stripB->GetBounds(), pointPx)) || m_stripControls.Contains (pointPx))
    {
        target = KeyTarget::Strip;
    }
    else if (IsInside (m_session.views.sectorRow->GetBounds(), pointPx))
    {
        target = KeyTarget::SectorRow;
    }
    else if (m_session.views.sectors->IsVisible() && IsInside (m_session.views.sectors->GetBounds(), pointPx))
    {
        target = KeyTarget::SectorData;
    }
    else if (m_session.views.nibbles->IsVisible() && IsInside (m_session.views.nibbles->GetBounds(), pointPx))
    {
        target = KeyTarget::Nibbles;
    }
    else if (m_session.views.flux->IsVisible() && IsInside (m_session.views.flux->GetBounds(), pointPx))
    {
        target = KeyTarget::FluxTiming;
    }
    else if (m_session.views.tracks->IsVisible() && IsInside (m_session.views.tracks->GetBounds(), pointPx))
    {
        target = KeyTarget::Tracks;
    }
    else if (m_session.views.issues->IsVisible() && IsInside (m_session.views.issues->GetBounds(), pointPx))
    {
        target = KeyTarget::Findings;
    }
    else if (m_session.views.image->IsVisible() && IsInside (m_session.views.image->GetBounds(), pointPx))
    {
        target = KeyTarget::Image;
    }
    else if (m_session.views.files->IsListAt (pointPx))
    {
        target = KeyTarget::FileList;
    }
    else if (m_session.views.diffs->IsVisible() && IsInside (m_session.views.diffs->GetBounds(), pointPx))
    {
        target = KeyTarget::Differences;
    }
    else if (m_session.views.findResults->IsVisible() && IsInside (m_session.views.findResults->GetBounds(), pointPx))
    {
        target = KeyTarget::FindResults;
    }

    return target;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::SelectAll
//
//  The sector's bytes in the Sector data tab, the track's nibbles in the
//  Nibbles tab and the strip (FR-043).
//
////////////////////////////////////////////////////////////////////////////////

void SelectionPart::SelectAll()
{
    if (m_session.keyTarget == KeyTarget::SectorData)
    {
        m_session.model.SelectAllBytes();
    }
    else if (m_session.keyTarget == KeyTarget::Nibbles || m_session.keyTarget == KeyTarget::Strip)
    {
        m_session.model.SelectAllNibbles();
        m_host.OnSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::ExtendSelection
//
//  Shift with an arrow moves the end of the selection away from where it
//  started: a byte or a nibble across, a row up or down.
//
////////////////////////////////////////////////////////////////////////////////

void SelectionPart::ExtendSelection (WPARAM vk)
{
    const TrackAnalysis *  track  = m_session.model.GetTrack();
    bool                   isByte = m_session.keyTarget == KeyTarget::SectorData;
    int                    row    = isByte ? SectorsView::kBytesPerRow : std::max (m_session.views.nibbles->GetPerRow(), 1);
    int                    first  = isByte ? m_session.model.GetFirstByte()  : m_session.model.GetFirstNibble();
    int                    count  = isByte ? m_session.model.GetByteCount()  : m_session.model.GetNibbleCount();
    int                    anchor = isByte ? m_session.model.GetByteAnchor() : m_session.model.GetNibbleAnchor();
    int                    last   = isByte ? DiskFieldFormat::kSectorBytes - 1 : (track != nullptr ? static_cast<int> (track->framed.nibbles.size()) - 1 : -1);
    int                    end    = (first == anchor) ? first + count - 1 : first;
    int                    step   = (vk == VK_LEFT) ? -1 : (vk == VK_RIGHT) ? 1 : (vk == VK_UP) ? -row : row;



    if (first >= 0 && count > 0 && last >= 0)
    {
        if (isByte)
        {
            m_session.model.ExtendBytes (std::clamp (end + step, 0, last));
        }
        else
        {
            m_session.model.ExtendNibbles (std::clamp (end + step, 0, last));
            m_host.OnSelection();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::Copy
//
//  Whatever the view pressed last has selected, as the window contract
//  gives it (FR-057): bytes as hex or text, nibbles as hex, a table's row,
//  the histogram; the sector as a hex dump when nothing smaller is chosen.
//
////////////////////////////////////////////////////////////////////////////////

void SelectionPart::Copy()
{
    const TrackAnalysis *   track  = m_session.model.GetTrack();
    const AnalyzedSector *  sector = m_session.model.GetSector();
    std::wstring            text;



    switch (m_session.keyTarget)
    {
        case KeyTarget::Tracks:     text = m_session.views.tracks->GetSelectedText();   break;
        case KeyTarget::Findings:   text = m_session.views.issues->GetSelectedText(); break;
        case KeyTarget::Image:      text = m_session.views.image->GetSelectedText();    break;
        case KeyTarget::FileList:   text = m_session.views.files->GetFileList().GetSelectedText(); break;
        case KeyTarget::Differences: text = m_session.views.diffs->GetSelectedText();   break;
        case KeyTarget::FindResults: text = m_session.views.findResults->GetSelectedText(); break;
        case KeyTarget::FluxTiming: text = m_session.views.flux->GetHistogramText();    break;

        case KeyTarget::Strip:
        case KeyTarget::Nibbles:
            //  A section header last clicked copies as its line (FR-057).
            text = (m_session.keyTarget == KeyTarget::Nibbles) ? m_session.views.nibbles->GetSelectedHeaderText() : std::wstring();
            text = !text.empty() ? text : (track != nullptr && m_session.model.GetNibbleCount() > 0) ? InspectorClipboard::FormatNibbles (*track, m_session.model.GetFirstNibble(), m_session.model.GetNibbleCount())
                                                                      : std::wstring();
            break;

        case KeyTarget::SectorData:
            if (track != nullptr && sector != nullptr && sector->dataField >= 0 && m_session.model.GetByteCount() > 0)
            {
                std::span<const Byte>  bytes = std::span<const Byte> (track->fields[sector->dataField].data.bytes).subspan (m_session.model.GetFirstByte(), m_session.model.GetByteCount());

                text = m_session.model.IsTextColumn() ? InspectorClipboard::FormatText (bytes) : InspectorClipboard::FormatHex (bytes);
            }

            break;

        default:
            break;
    }

    if (text.empty())
    {
        CopySector();
    }
    else
    {
        (void) m_session.clipboard.SetText (m_host.GetOwnerHwnd(), text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::CopySector
//
//  "Copy sector": the whole sector as a hex dump (FR-057).
//
////////////////////////////////////////////////////////////////////////////////

void SelectionPart::CopySector()
{
    const TrackAnalysis *   track  = m_session.model.GetTrack();
    const AnalyzedSector *  sector = m_session.model.GetSector();



    if (track != nullptr && sector != nullptr && sector->dataField >= 0)
    {
        (void) m_session.clipboard.SetText (m_host.GetOwnerHwnd(), InspectorClipboard::FormatHexDump (track->fields[sector->dataField].data.bytes));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::ShowContextMenu
//
//  The Sector data, Nibbles and Tracks tabs' right-click menu: Copy, "Copy
//  sector" in the Sector data tab, and "Export..." (FR-057, FR-058).
//
////////////////////////////////////////////////////////////////////////////////

void SelectionPart::ShowContextMenu (POINT pointPx)
{
    DxuiHwndSource                  * host   = m_host.GetPopupHost();
    std::vector<DxuiPopupMenuItem>    items;
    std::shared_ptr<DxuiCommand>      copy   = std::make_shared<DxuiCommand>();
    std::shared_ptr<DxuiCommand>      sector = std::make_shared<DxuiCommand>();
    std::shared_ptr<DxuiCommand>      save   = std::make_shared<DxuiCommand>();



    copy->label          = L"Copy";
    copy->accelerator    = L"Ctrl+C";
    copy->dispatch       = [this] () { Copy(); };
    sector->label        = L"Copy sector";
    sector->accelerator  = L"Ctrl+Shift+C";
    sector->dispatch     = [this] () { CopySector(); };
    sector->isEnabled    = [this] () { return m_session.model.GetSector() != nullptr && m_session.model.GetSector()->dataField >= 0; };
    save->label          = L"Export...";
    save->dispatch       = [this] () { m_dialogs.OpenExport(); };

    items.push_back (DxuiPopupMenuItem::ForCommand (copy));

    if (m_session.keyTarget == KeyTarget::SectorData)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (sector));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (save));

    m_menuCommands = { copy, sector, save };

    if (host != nullptr)
    {
        DxuiContextMenu::Show (*host, pointPx.x, pointPx.y, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::StepKeyTarget
//
//  F6 and Shift+F6: the keys move to the next or previous view shown.
//
////////////////////////////////////////////////////////////////////////////////

void SelectionPart::StepKeyTarget (int step)
{
    const std::array<std::pair<KeyTarget, const IDxuiControl *>, 10>  order =
    {{
        { KeyTarget::Platter,    m_session.views.platter },
        { KeyTarget::Strip,      m_session.views.strip },
        { KeyTarget::SectorRow,  m_session.views.sectorRow },
        { KeyTarget::SectorData, m_session.views.sectors },
        { KeyTarget::Nibbles,    m_session.views.nibbles },
        { KeyTarget::FluxTiming, m_session.views.flux },
        { KeyTarget::Tracks,     m_session.views.tracks },
        { KeyTarget::Findings,   m_session.views.issues },
        { KeyTarget::Differences, m_session.views.diffs },
        { KeyTarget::FindResults, m_session.views.findResults },
    }};



    int  count = static_cast<int> (order.size());
    int  at    = 0;
    int  k     = 0;



    for (k = 0; k < count; k++)
    {
        at = (order[k].first == m_session.keyTarget) ? k : at;
    }

    for (k = 1; k < count; k++)
    {
        size_t        index = static_cast<size_t> (at + step * k + count * k) % order.size();
        const auto &  next  = (index < order.size()) ? order[index] : order[0];

        if (next.second != nullptr && next.second->IsVisible())
        {
            m_session.keyTarget = next.first;
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::GetKeyTargetBounds
//
////////////////////////////////////////////////////////////////////////////////

RECT SelectionPart::GetKeyTargetBounds() const
{
    RECT  bounds = m_session.views.platter->GetBounds();



    switch (m_session.keyTarget)
    {
        case KeyTarget::Strip:      bounds = m_session.views.strip->GetBounds();   break;
        case KeyTarget::SectorRow:  bounds = m_session.views.sectorRow->GetBounds();   break;
        case KeyTarget::SectorData: bounds = m_session.views.sectors->GetBounds();    break;
        case KeyTarget::Nibbles:    bounds = m_session.views.nibbles->GetBounds();  break;
        case KeyTarget::FluxTiming: bounds = m_session.views.flux->GetBounds();     break;
        case KeyTarget::Tracks:     bounds = m_session.views.tracks->GetBounds();   break;
        case KeyTarget::Findings:   bounds = m_session.views.issues->GetBounds(); break;
        case KeyTarget::Differences: bounds = m_session.views.diffs->GetBounds();   break;
        case KeyTarget::FindResults: bounds = m_session.views.findResults->GetBounds(); break;
        default:                                                         break;
    }

    return bounds;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionPart::IsInside
//
////////////////////////////////////////////////////////////////////////////////

bool SelectionPart::IsInside (const RECT & rect, POINT pointPx)
{
    return PtInRect (&rect, pointPx) != FALSE;
}
