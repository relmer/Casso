#include "Pch.h"

#include "Core/NumberFormat.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorState.h"
#include "Ui/DiskInspector/InspectorText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorState::BuildTitle
//
//  "Block inspector - " and the file's name, then a note such as the image
//  it is compared with. `outMarkAt` is where the red dot goes after the
//  name while edits are pending, or npos.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockInspectorState::BuildTitle (const std::wstring & fileName, const std::wstring & note, bool hasEdits, size_t & outMarkAt)
{
    std::wstring  name = kpszTitlePrefix + fileName;



    outMarkAt = hasEdits ? name.size() : std::wstring::npos;

    return name + note;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorState::BuildEditBarState
//
//  The edit bar from the pending edits. A block image has no checksums, and
//  a storage card's partition has no file to copy for Save as.
//
////////////////////////////////////////////////////////////////////////////////

EditBarState BlockInspectorState::BuildEditBarState (const BlockInspectorModel & model, bool isBusy, bool isEditing, bool isOnCard, const std::wstring & note)
{
    bool          isOpen = model.IsOpen() && !isBusy;
    EditBarState  state;



    state.canEdit        = isOpen;
    state.isEditing      = isEditing;
    state.hasSectorEdit  = isOpen && model.GetEdits().HasChanges (model.GetBlock());
    state.hasChecksum    = false;
    state.bytes          = model.IsOpen() ? model.GetEdits().CountChangedBytes() : 0;
    state.sectors        = model.IsOpen() ? (int) model.GetEdits().GetChangedBlocks().size() : 0;
    state.canApply       = isOpen;
    state.canSaveCopy    = !isOnCard;
    state.canUndoApplied = isOpen && model.CanUndoApplied();
    state.canRedoApplied = isOpen && model.CanRedoApplied();
    state.canUseB        = isOpen && model.IsComparing() && model.IsDifferent (model.GetBlock());
    state.note           = note;

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorState::BuildLead
//
//  The status band's left end: the count of unsaved blocks, the
//  comparison's result while comparing, the volume's chips, "Opening" while
//  the file opens, and the selection.
//
////////////////////////////////////////////////////////////////////////////////

vector<InspectorStatusBar::LeadItem> BlockInspectorState::BuildLead (const BlockInspectorModel & model, bool isBusy, const std::wstring & selectionLine)
{
    using LeadKind = InspectorStatusBar::LeadKind;

    vector<InspectorStatusBar::LeadItem>  items;
    size_t                                differ  = 0;
    size_t                                unsaved = 0;



    if (!model.IsOpen())
    {
        items.push_back ({ L"Opening", LeadKind::Note });
        return items;
    }

    unsaved = model.GetEdits().GetChangedBlocks().size();

    if (unsaved > 0)
    {
        items.push_back ({ InspectorText::FormatCount ((int) unsaved, L"unsaved block", L"unsaved blocks"), LeadKind::Result });
    }

    if (isBusy)
    {
        return items;
    }

    if (model.IsComparing())
    {
        differ = model.CountDifferent();
        items.push_back ({ differ == 0 ? std::wstring (L"No blocks differ") : differ == 1 ? std::wstring (L"1 block differs") : std::format (L"{} blocks differ", NumberFormat::FormatInteger (differ)),
                           LeadKind::Result });
    }

    for (const SummaryChip & chip : InspectorText::BuildBlockChips (model))
    {
        items.push_back ({ chip.text, chip.isBad ? LeadKind::BadChip : LeadKind::Chip });
    }

    items.push_back ({ selectionLine, LeadKind::Selection });

    return items;
}
