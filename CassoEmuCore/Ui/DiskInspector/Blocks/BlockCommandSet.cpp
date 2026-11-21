#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Ui/DiskInspector/Blocks/BlockCommandSet.h"
#include "Ui/DiskInspector/Blocks/BlockGoToDialog.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/Blocks/BlockNavigator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSet::BlockCommandSet
//
////////////////////////////////////////////////////////////////////////////////

BlockCommandSet::BlockCommandSet (const Parts & parts, InspectorCommands & commands) :
    m_parts    (parts),
    m_commands (commands)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSet::Build
//
//  The commands block mode has, in the slots of the track inspector's menus.
//
////////////////////////////////////////////////////////////////////////////////

void BlockCommandSet::Build()
{
    InspectorCommands    & c          = m_commands;
    BlockInspectorModel  & model      = m_parts.model;
    IBlockJobs           & jobs       = m_parts.jobs;
    IBlockView           & view       = m_parts.view;
    IBlockNavigation     & navigation = m_parts.navigation;
    IBlockDisplay        & display    = m_parts.display;
    IBlockFrame          & frame      = m_parts.frame;
    IBlockEditing        & editing    = m_parts.editing;
    IBlockFileSession    & session    = m_parts.session;
    IBlockRepair         & repair     = m_parts.repair;
    auto                   isOpen     = [&model, &jobs] () { return model.IsOpen() && !jobs.IsBusy(); };
    auto                   hasEdits   = [&model, &jobs] () { return model.IsOpen() && !jobs.IsBusy() && !model.GetEdits().IsEmpty(); };



    auto  make = [] (LPCWSTR label, LPCWSTR accelerator, std::function<void()> dispatch)
    {
        auto  command = std::make_shared<DxuiCommand>();

        command->label       = label;
        command->accelerator = accelerator;
        command->dispatch    = std::move (dispatch);
        return command;
    };

    //  File
    c.save                      = make (L"&Save...", L"Ctrl+S", [&editing] () { editing.Save(); });
    c.save->isEnabled           = hasEdits;
    c.saveEditedCopy            = make (L"Save &as...", L"", [&editing] () { editing.SaveEditedCopy(); });
    c.saveEditedCopy->isEnabled = [this, hasEdits] () { return hasEdits() && !m_parts.file.IsOnCard(); };
    c.close                     = make (L"&Close", L"Alt+F4", [&frame] () { frame.RequestClose(); });

    //  Edit
    c.undoApplied                = make (L"&Undo save", L"Ctrl+Z", [&editing] () { editing.UndoApplied(); });
    c.undoApplied->isEnabled     = [&model, isOpen] () { return isOpen() && model.CanUndoApplied(); };
    c.redoApplied                = make (L"&Redo save", L"Ctrl+Y", [&editing] () { editing.RedoApplied(); });
    c.redoApplied->isEnabled     = [&model, isOpen] () { return isOpen() && model.CanRedoApplied(); };
    c.copySector                 = make (L"Copy &block", L"Ctrl+Shift+C", [&editing] () { editing.CopyBlock(); });
    c.copySector->isEnabled      = isOpen;
    c.copySector->glyph          = kpszGlyphCopy;
    c.copySector->tip            = L"Copy the whole block as a hex dump (Ctrl+Shift+C)";
    c.copySector->isGlyphInMenus = true;
    c.editSector                 = make (L"&Edit block", L"F2", [&view] () { view.SetEditing (!view.IsEditing()); });
    c.editSector->isEnabled      = isOpen;
    c.editSector->isChecked      = [&view] () { return view.IsEditing(); };
    c.editSector->glyph          = kpszGlyphEdit;
    c.discardEdits               = make (L"&Discard edits", L"", [&editing] () { editing.Discard(); });
    c.discardEdits->isEnabled    = hasEdits;

    //  View
    c.zoomIn             = make (L"Zoom &in", L"+", [&display] () { display.GetMap().StepZoom (1); });
    c.zoomIn->isEnabled  = [&model, &display] () { return model.IsOpen() && !display.GetMap().IsAtMost(); };
    c.zoomOut            = make (L"Zoom &out", L"-", [&display] () { display.GetMap().StepZoom (-1); });
    c.zoomOut->isEnabled = [&model, &display] () { return model.IsOpen() && !display.GetMap().IsAtFit(); };
    c.fit                = make (L"&Whole volume", L"0", [&display] () { display.GetMap().Fit(); });
    c.fit->isEnabled     = [&model, &display] () { return model.IsOpen() && !display.GetMap().IsAtFit(); };

    c.overlays.push_back (make (L"&Files", L"", [&display] () { display.SetFilesOn (!display.IsFilesOn()); }));
    c.overlays.back()->isChecked = [&display] () { return display.IsFilesOn(); };
    c.overlays.back()->isEnabled = isOpen;
    c.overlays.push_back (make (L"D&ifferences", L"", [&display] () { display.SetDiffsOn (!display.IsDiffsOn()); }));
    c.overlays.back()->isChecked = [&display] () { return display.IsDiffsOn(); };
    c.overlays.back()->isEnabled = [&model] () { return model.IsOpen() && model.IsComparing(); };

    for (const auto & [label, kind] : { std::pair<LPCWSTR, BlockPlace::Kind> (L"&Card",   BlockPlace::Kind::Card),
                                        std::pair<LPCWSTR, BlockPlace::Kind> (L"&Volume", BlockPlace::Kind::Volume),
                                        std::pair<LPCWSTR, BlockPlace::Kind> (L"&Block",  BlockPlace::Kind::Block) })
    {
        c.views.push_back (make (label, L"", [this, kind] () { ShowLevel (kind); }));
        c.views.back()->radio     = true;
        c.views.back()->isChecked = [&navigation, kind] () { return navigation.GetNavigator().GetPlace().kind == kind; };
        c.views.back()->isEnabled = [&model, &jobs, &navigation, kind] ()
        {
            return model.IsOpen() && !jobs.IsBusy() && (kind == BlockPlace::Kind::Card ? navigation.GetNavigator().GetBrowser().HasCard() : kind == BlockPlace::Kind::Block || model.GetVolumeCount() > 0);
        };
    }

    //  Go
    c.back                          = make (L"&Back", L"Alt+Left", [&navigation] () { navigation.GoBack(); });
    c.back->isEnabled               = [&model, &jobs, &navigation] () { return model.IsOpen() && !jobs.IsBusy() && navigation.GetNavigator().GetBrowser().CanGoBack(); };
    c.forward                       = make (L"&Forward", L"Alt+Right", [&navigation] () { navigation.GoForward(); });
    c.forward->isEnabled            = [&model, &jobs, &navigation] () { return model.IsOpen() && !jobs.IsBusy() && navigation.GetNavigator().GetBrowser().CanGoForward(); };
    c.goTo                          = make (L"&Go to block...", L"Ctrl+G", [this] () { GoToBlock(); });
    c.goTo->isEnabled               = isOpen;
    c.goTo->shortLabel              = L"Go to";
    c.goTo->glyph                   = kpszGlyphGoTo;
    c.goTo->tip                     = L"Go to a block (Ctrl+G)";
    c.goTo->isGlyphInMenus          = true;
    c.previousSector                = make (L"&Previous block", L"Page Up", [&navigation] () { navigation.Step (-1); });
    c.previousSector->isEnabled     = [&model] () { return model.IsOpen() && model.GetBlock() > 0; };
    c.nextSector                    = make (L"&Next block", L"Page Down", [&navigation] () { navigation.Step (1); });
    c.nextSector->isEnabled         = [&model] () { return model.IsOpen() && model.GetBlock() + 1 < model.GetBlockCount(); };
    c.previousDifference            = make (L"Previous &difference", L"Shift+F8", [this] () { StepDifference (false); });
    c.previousDifference->isEnabled = [&model] () { return model.IsOpen() && model.CountDifferent() > 0; };
    c.nextDifference                = make (L"Next d&ifference", L"F8", [this] () { StepDifference (true); });
    c.nextDifference->isEnabled     = [&model] () { return model.IsOpen() && model.CountDifferent() > 0; };

    //  Disk
    c.compareWith              = make (L"&Compare with...", L"", [&session] () { session.CompareWithPicked(); });
    c.compareWith->isEnabled   = isOpen;
    c.endComparison            = make (L"&End comparison", L"", [&session] () { session.EndComparison(); });
    c.endComparison->isEnabled = [&model, &jobs] () { return model.IsOpen() && !jobs.IsBusy() && model.IsComparing(); };
    c.repair                   = make (L"&Repair...", L"", [&repair] () { repair.Repair(); });
    c.repair->isEnabled        = [&repair, isOpen] () { return isOpen() && repair.CanRepair(); };

    //  Window
    c.windowZoomIn  = make (L"Zoom &in", L"Ctrl++", [&frame] () { frame.SetZoom (DxuiZoomControl::StepZoom (frame.GetZoom(), 1)); });
    c.windowZoomOut = make (L"Zoom &out", L"Ctrl+-", [&frame] () { frame.SetZoom (DxuiZoomControl::StepZoom (frame.GetZoom(), -1)); });
    c.actualSize    = make (L"&Actual size", L"Ctrl+0", [&frame] () { frame.SetZoom (1.0); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSet::Run
//
//  True when the command is enabled and ran.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockCommandSet::Run (const InspectorCommands::Command & command)
{
    bool  isRun = command != nullptr && command->dispatch && command->IsEnabled();



    if (isRun)
    {
        command->dispatch();
    }

    return isRun;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSet::TryRunWindowKey
//
//  The keys that act before text being typed takes its keys: Ctrl with +,
//  - and 0 zoom the window, and Alt with the arrows go back and forward.
//  False for any other key; otherwise `outResult` is whether it was handled.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockCommandSet::TryRunWindowKey (const DxuiKeyEvent & ev, bool & outResult)
{
    bool  isZoom = ev.vk == '0' || ev.vk == VK_NUMPAD0 || ev.vk == VK_OEM_PLUS || ev.vk == VK_ADD || ev.vk == VK_OEM_MINUS || ev.vk == VK_SUBTRACT;



    if (ev.ctrl && !ev.alt && isZoom)
    {
        m_parts.frame.SetZoom ((ev.vk == '0' || ev.vk == VK_NUMPAD0) ? 1.0 : DxuiZoomControl::StepZoom (m_parts.frame.GetZoom(), (ev.vk == VK_OEM_PLUS || ev.vk == VK_ADD) ? 1 : -1));
        outResult = true;
        return true;
    }

    if (ev.alt && !ev.ctrl && !ev.shift && (ev.vk == VK_LEFT || ev.vk == VK_RIGHT))
    {
        outResult = Run (ev.vk == VK_LEFT ? m_commands.back : m_commands.forward);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSet::TryRunKey
//
//  The shortcuts after the focused control's typing and editing keys. Escape
//  cancels the running job and goes on to the focused control. False for a
//  key the focused control takes; otherwise `outResult` is whether it was
//  handled.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockCommandSet::TryRunKey (const DxuiKeyEvent & ev, bool & outResult)
{
    InspectorCommands::Command  command;



    if (!ev.ctrl && !ev.alt && ev.vk == VK_F2)
    {
        command = m_commands.editSector;
    }
    else if (ev.ctrl && !ev.alt)
    {
        switch (ev.vk)
        {
            case 'G': command = m_commands.goTo;                                    break;
            case 'C': command = ev.shift ? m_commands.copySector : nullptr;         break;
            case 'S': command = m_commands.save;                                    break;
            case 'Z': command = m_commands.undoApplied;                             break;
            case 'Y': command = m_commands.redoApplied;                             break;
            default:                                                                break;
        }

        if (command == nullptr)
        {
            return false;
        }
    }
    else if (!ev.ctrl && !ev.alt)
    {
        switch (ev.vk)
        {
            case VK_PRIOR:  command = m_commands.previousSector;                                               break;
            case VK_NEXT:   command = m_commands.nextSector;                                                   break;
            case VK_F8:     command = ev.shift ? m_commands.previousDifference : m_commands.nextDifference;    break;
            case VK_ESCAPE: m_parts.jobs.CancelJobs();                                                         break;
            default:                                                                                           break;
        }
    }

    if (command == nullptr)
    {
        return false;
    }

    outResult = Run (command);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSet::TryRunMouseButton
//
//  The mouse's Back and Forward buttons go back and forward.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockCommandSet::TryRunMouseButton (const DxuiMouseEvent & ev)
{
    if (ev.kind != DxuiMouseEventKind::Up || (ev.button != DxuiMouseButton::Back && ev.button != DxuiMouseButton::Forward))
    {
        return false;
    }

    (void) Run (ev.button == DxuiMouseButton::Back ? m_commands.back : m_commands.forward);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSet::ShowLevel
//
//  The View menu's places: the card, the volume of the current place or
//  block, and the current block.
//
////////////////////////////////////////////////////////////////////////////////

void BlockCommandSet::ShowLevel (BlockPlace::Kind kind)
{
    BlockNavigator  & navigator = m_parts.navigation.GetNavigator();
    int               volume    = navigator.GetPlace().volume >= 0 ? navigator.GetPlace().volume : std::max (0, m_parts.model.FindVolume (m_parts.model.GetBlock()));



    if (kind == BlockPlace::Kind::Block)
    {
        m_parts.navigation.SelectBlock (m_parts.model.GetBlock());
        return;
    }

    navigator.GoTo ({ kind, kind == BlockPlace::Kind::Card ? -1 : volume, -1 });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSet::GoToBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockCommandSet::GoToBlock()
{
    uint32_t  block = 0;



    if (BlockGoToDialog::Choose (m_parts.dialogs.GetDialogOwner(), m_parts.dialogs.GetDialogTheme(), m_parts.model.GetBlockCount(), m_parts.model.GetBlock(), block))
    {
        m_parts.navigation.GoToBlock (block);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSet::StepDifference
//
////////////////////////////////////////////////////////////////////////////////

void BlockCommandSet::StepDifference (bool isForward)
{
    uint32_t  block = 0;



    if (m_parts.model.FindDifference (isForward, block))
    {
        m_parts.navigation.SelectBlock (block);
    }
}
