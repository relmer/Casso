#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/LinkImageDropRule.h"
#include "CassoExplorer/Window/Managers/CassoLinkDriveManager.h"
#include "CassoExplorer/Window/Managers/ExplorerTransferPrompts.h"
#include "CassoExplorer/Window/Managers/ExplorerVerbs.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "Core/AppMessages.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::DropOnAppleDrive
//
//  A drop or a paste onto a linked Apple's drive: one disk image is written
//  to the whole disk, or offered both ways on a menu; files, or an image's
//  files, are copied in. Each write goes over the link, and is reported when
//  it is done. False when nothing can go onto the drive.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDragDrop::DropOnAppleDrive (IDataObject * data, DropSource & source, const Location & drive, POINT screen)
{
    LinkImageDrop                  drop    = GetImageDrop (source, drive);
    CassoExplorerActions::Outcome  outcome;
    BOOL                           posted  = FALSE;



    if (drop == LinkImageDrop::None)
    {
        return false;
    }

    if (drop == LinkImageDrop::Write)
    {
        m_context.linkDrives.StartImageWrite (drive, source.hostPaths[0]);
        return true;
    }

    //  The write-or-copy menu opens once this call has returned and a drag
    //  is over, so the window it came from isn't left waiting.
    if (drop == LinkImageDrop::Choose)
    {
        m_pendingDrop             = PendingDrop();
        m_pendingDrop.valid       = true;
        m_pendingDrop.imageChoice = true;
        m_pendingDrop.location    = drive;
        m_pendingDrop.screen      = screen;
        m_pendingDrop.hostPaths   = source.hostPaths;

        posted = PostMessageW (m_context.window.GetHwnd(), (UINT) AppMessage::ExplorerDropMenu, 0, 0);
        IGNORE_RETURN_VALUE (posted, TRUE);
        return true;
    }

    if (m_context.browser.GetLocation().path != drive.path || m_context.browser.GetLocation().innerPath != drive.innerPath)
    {
        m_context.addressBar.GoToLocation (drive);
    }

    RemoveVirtualFiles();

    if (source.isVirtual && !WriteVirtualFiles (data, source))
    {
        return true;
    }

    outcome = source.fromImage ? m_context.actions.CopyEntriesToAppleDrive (source.image, source.kind, source.catalogPaths)
                               : m_context.actions.PutFiles (source.hostPaths, m_context.prompts.MakeAddressPrompt());

    m_context.verbs.ReportOutcome (outcome, L"Put");
    RemoveVirtualFiles();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::TryGetPasteDrive
//
//  The linked drive a paste goes onto: the one shown, or the one drive row
//  selected in its connection's list.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDragDrop::TryGetPasteDrive (Location & outDrive)
{
    const std::vector<int> &  selected = m_context.browser.GetSelectedRows();
    Location                  row;



    if (m_context.browser.IsAppleDriveLocation())
    {
        outDrive = m_context.browser.GetLocation();
        return true;
    }

    if (selected.size() == 1 && BrowserRowPaths::TryGetRowLocation (m_context.browser.GetRowListing(), selected[0], row)
        && row.kind == Location::Kind::AppleDrive && row.innerPath.empty())
    {
        outDrive = row;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop::PasteOnAppleDrive
//
//  The clipboard's files, as a drop on the drive at the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDragDrop::PasteOnAppleDrive (const Location & drive)
{
    ComPtr<IDataObject>  data;
    DropSource           source;
    POINT                screen = {};
    HRESULT              hr     = OleGetClipboard (&data);



    if (FAILED (hr) || !ReadDropSource (data.Get(), source))
    {
        return;
    }

    GetCursorPos (&screen);

    if (!DropOnAppleDrive (data.Get(), source, drive, screen))
    {
        m_context.window.ShowMessage (std::format (L"The disk image is too big for {}.", m_context.browser.GetTreeModel().GetLinkDriveTitle (drive.path)), MB_ICONWARNING);
    }
}
