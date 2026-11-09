#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Managers/ExplorerManifestSync.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/AppleFilePropertiesDialog.h"
#include "CassoExplorer/WozPropertiesDialog.h"
#include "CassoExplorer/Model/WozProperties.h"
#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/Model/HostAppleFile.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "CassoExplorer/Model/CassoTargeting.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "AssetBootstrap.h"
#include "Config/UserConfigStore.h"
#include "Core/JsonWriter.h"
#include "Core/MachineScanner.h"
#include "Core/TextEncoding.h"
#include "Core/AppMessages.h"
#include "Core/AppTimerIds.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "Seams/Win32IntentChannel.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::ExplorerFileOps
//
////////////////////////////////////////////////////////////////////////////////

ExplorerFileOps::ExplorerFileOps (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::GetPasteFolder
//
//  A single folder row takes the paste; otherwise the folder being shown.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerFileOps::GetPasteFolder() const
{
    Location  location;



    if (m_context.browser.GetSelectedRows().size() == 1
     && m_context.browser.TryGetRowLocation (m_context.browser.GetSelectedRows()[0], location)
     && location.kind == Location::Kind::HostFolder)
    {
        return location.path;
    }

    return m_context.browser.GetLocation().path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::CopyEntriesToClipboard
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::CopyEntriesToClipboard (HostFileNaming::Style style)
{
    std::wstring  failure;
    HRESULT       hr      = CassoExplorerDragOut::CopyToClipboard (m_context.browser, style, failure);



    //  A file that could not be read has a message with the file's name; for
    //  any other failure, as the clipboard in use, the text comes from
    //  its code.
    if (FAILED (hr))
    {
        m_context.window.ShowMessage ((failure.empty() ? L"The files could not be copied: " + CassoExplorerDragOut::DescribeCopyFailure (hr) : failure + L"."), MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::CutEntriesToClipboard
//
//  What is deleted is read now, while the entries are on display: the
//  paste may come after the window has moved elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::CutEntriesToClipboard (HostFileNaming::Style style)
{
    std::wstring            failure;
    HRESULT                 hr      = S_OK;
    CutSource               cut;
    std::vector<FileEntry>  entries;
    HWND                    hwnd    = m_context.window.GetHwnd();



    if (m_context.browser.IsAppleDriveLocation())
    {
        cut.onDrive = true;

        if (!m_context.actions.TryPlanDriveDeletion (cut.drive))
        {
            return;
        }
    }
    else
    {
        cut.image = TextEncoding::WideToNarrow (m_context.browser.GetLocation().path);
        m_context.browser.GetSelectedEntries (entries);

        for (const FileEntry & entry : entries)
        {
            cut.catalogPaths.push_back (m_context.browser.GetEntryPath (entry));
        }
    }

    hr = CassoExplorerDragOut::CopyToClipboard (m_context.browser, style, failure, true, [hwnd]()
    {
        BOOL  posted = PostMessageW (hwnd, (UINT) AppMessage::ExplorerCutPasted, 0, 0);

        IGNORE_RETURN_VALUE (posted, TRUE);
    });

    if (FAILED (hr))
    {
        m_context.window.ShowMessage ((failure.empty() ? L"The files could not be cut: " + CassoExplorerDragOut::DescribeCopyFailure (hr) : failure + L"."), MB_ICONWARNING);
        return;
    }

    cut.valid = true;
    m_cut     = std::move (cut);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::FinishCut
//
//  The cut entries deleted, now that a target has pasted them. A drive's
//  deletions go over the link and are reported as they finish.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::FinishCut()
{
    CassoExplorerActions::Outcome  outcome;
    CutSource                      cut     = std::move (m_cut);



    m_cut = CutSource();

    if (!cut.valid)
    {
        return;
    }

    outcome = cut.onDrive ? m_context.actions.RunDriveDeletion (cut.drive)
                          : m_context.actions.DeleteEntries (cut.image, cut.catalogPaths);

    m_context.commandManager.ReportOutcome (outcome, L"Move");
    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::CopySelectedPaths
//
//  Each path in quotes on its own line, as Explorer's Copy as path gives them.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::CopySelectedPaths()
{
    std::wstring  text;
    std::wstring  path;



    for (int row : m_context.browser.GetSelectedRows())
    {
        if (!m_context.browser.TryGetRowPath (row, path))
        {
            continue;
        }

        if (!text.empty())
        {
            text += L"\r\n";
        }

        text += L"\"" + path + L"\"";
    }

    if (!text.empty())
    {
        DxuiClipboard::SetText (m_context.window.GetHwnd(), text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::PasteHere
//
//  The clipboard's files into the folder, with an undo that takes back what
//  the paste made: a copy goes to the Recycle Bin, a move goes back.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::PasteHere (const std::wstring & folder)
{
    IShellItemVerbs::PasteResult  pasted;
    ComPtr<IDataObject>           data;
    HRESULT                       hr     = OleGetClipboard (&data);



    //  An image's or a linked Apple's files are written here rather than by
    //  the folder's drop target, whose name clash prompt has no Keep both and
    //  gives the files a folder they never came from.
    if (SUCCEEDED (hr) && ActionsCopy::IsCassoCopy (data.Get()))
    {
        CassoExplorerActions::Outcome  outcome = m_context.actions.PasteVirtualFiles (data.Get(), folder, MakeCollisionPrompt(), pasted.created);

        m_context.commandManager.ReportOutcome (outcome, L"Copy");
        pasted.sources.assign (pasted.created.size(), std::wstring());

        //  A cut's files are deleted where they were, now they are here.
        ReportPasted (data.Get(), outcome);
    }
    else
    {
        hr = m_context.shellVerbs.PasteInto (m_context.window.GetHwnd(), folder, pasted);
    }

    //  After a paste that failed or was canceled, the cut files remain cut,
    //  as in File Explorer. Any error is in the shell's dialog.
    if (SUCCEEDED (hr))
    {
        m_context.listPane.ClearCutPaths();
    }

    if (!pasted.created.empty())
    {
        ExplorerUndoStep  step;

        step.kind    = pasted.moved ? ExplorerUndoStep::Kind::HostMove : ExplorerUndoStep::Kind::HostCopy;
        step.paths   = pasted.created;
        step.sources = pasted.sources;

        m_context.undo.Push (std::move (step));
    }

    //  What was pasted is selected once it is listed, as in File Explorer.
    if (!pasted.created.empty())
    {
        std::vector<std::wstring>  names;

        for (const std::wstring & path : pasted.created)
        {
            names.push_back (std::filesystem::path (path).filename().wstring());
        }

        m_context.listPane.SelectWhenListed (folder, std::move (names));
    }

    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::PasteIntoVolume
//
//  The clipboard's files into the disk image or the linked Apple's drive
//  open in the list, by the rules a drop there follows: another image's
//  entries are copied byte for byte, and host files go in by Put's rules.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::PasteIntoVolume()
{
    HRESULT                        hr         = S_OK;
    ComPtr<IDataObject>            data;
    ExplorerDragDrop::DropSource   source;
    Location                       location   = m_context.browser.GetLocation();
    VolumeKind                     targetKind = VolumeKind::Unknown;
    std::string                    inner;
    VolumeListing                  listing;
    CassoExplorerActions::Outcome  outcome;



    hr = OleGetClipboard (&data);

    if (FAILED (hr) || !m_context.dragDrop.ReadDropSource (data.Get(), source))
    {
        return;
    }

    if (m_context.browser.IsAppleDriveLocation())
    {
        outcome = source.fromImage ? m_context.actions.CopyEntriesToAppleDrive (source.image, source.kind, source.catalogPaths)
                                   : m_context.actions.PutFiles (source.hostPaths, MakeAddressPrompt());

        m_context.commandManager.ReportOutcome (outcome, L"Put");
        ReportPasted (data.Get(), outcome);
        return;
    }

    inner = (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string();

    if (!m_context.browser.GetOperations().List (TextEncoding::WideToNarrow (location.path), inner, listing, targetKind).Succeeded())
    {
        m_context.window.ShowMessage (L"The disk image could not be read.", MB_ICONWARNING);
        return;
    }

    outcome = source.fromImage ? m_context.actions.CopyEntriesInto (source.image, source.kind, source.catalogPaths, location.path, targetKind, inner)
                               : m_context.actions.PutInto (location.path, targetKind, inner, source.hostPaths, MakeAddressPrompt());

    m_context.undo.PushPut (outcome, location.path, inner);
    m_context.commandManager.ReportOutcome (outcome, L"Put");

    //  Pasted back where it was cut from, nothing moved.
    if (!(source.fromImage && _wcsicmp (TextEncoding::NarrowToWide (source.image).c_str(), location.path.c_str()) == 0 && IsCutFrom (inner)))
    {
        ReportPasted (data.Get(), outcome);
    }

    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::ReportPasted
//
//  As File Explorer reports a paste of a cut, so the source deletes what it
//  cut: only when every file landed whole.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerFileOps::ReportPasted (IDataObject * data, const CassoExplorerActions::Outcome & outcome)
{
    HRESULT    hr        = S_OK;
    FORMATETC  preferred = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_PREFERREDDROPEFFECT), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    FORMATETC  pasted    = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_PASTESUCCEEDED), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM  medium    = {};
    DWORD      effect    = DROPEFFECT_NONE;
    DWORD    * locked    = nullptr;



    if (data == nullptr || !outcome.IsWhole() || outcome.written == 0)
    {
        return;
    }

    hr = data->GetData (&preferred, &medium);

    if (SUCCEEDED (hr))
    {
        locked = (DWORD *) GlobalLock (medium.hGlobal);

        if (locked != nullptr)
        {
            effect = *locked;
            GlobalUnlock (medium.hGlobal);
        }

        ReleaseStgMedium (&medium);
    }

    if (effect != DROPEFFECT_MOVE)
    {
        return;
    }

    medium         = {};
    medium.tymed   = TYMED_HGLOBAL;
    medium.hGlobal = GlobalAlloc (GMEM_MOVEABLE, sizeof (DWORD));

    if (medium.hGlobal == nullptr)
    {
        return;
    }

    locked = (DWORD *) GlobalLock (medium.hGlobal);

    if (locked != nullptr)
    {
        *locked = DROPEFFECT_MOVE;
        GlobalUnlock (medium.hGlobal);
    }

    hr = data->SetData (&pasted, &medium, TRUE);

    if (FAILED (hr))
    {
        ReleaseStgMedium (&medium);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::IsCutFrom
//
//  Whether the last cut's entries came from the image directory `inner`.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerFileOps::IsCutFrom (const std::string & inner) const
{
    if (!m_cut.valid || m_cut.onDrive || m_cut.catalogPaths.empty())
    {
        return false;
    }

    const std::string &  first = m_cut.catalogPaths[0];
    size_t               slash = first.find_last_of ('/');

    return ((slash == std::string::npos) ? std::string() : first.substr (0, slash)) == inner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::MakeAddressPrompt
//
//  Asks for a binary's load address, prefilled with the content's
//  suggestion, and asks again after an address that does not parse.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::AddressFn ExplorerFileOps::MakeAddressPrompt()
{
    return [this] (const std::wstring & hostName, Word suggested, Word & outAddress)
    {
        std::wstring  text = std::format (L"${:04X}", suggested);

        for (;;)
        {
            if (!CassoExplorerPromptDialog::Ask (m_context.window.GetHwnd(), m_context.theme, L"Load address",
                                           hostName + L" is a binary. Load address:", text, 8, text))
            {
                return false;
            }

            if (ActionsGoTo::TryParseAddress (text, outAddress))
            {
                return true;
            }

            m_context.window.ShowMessage (L"Type an address from $0000 to $FFFF.", MB_ICONWARNING);
        }
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps::MakeCollisionPrompt
//
//  File Explorer's question for a file of the same name in the folder a
//  copy goes into. Keep both is the default, so Enter or Escape writes over
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::CollisionFn ExplorerFileOps::MakeCollisionPrompt()
{
    return [this] (const std::wstring & hostPath, int index, int count)
    {
        std::filesystem::path  path    (hostPath);
        std::wstring           ofCount = (count > 1) ? std::format (L" ({} of {})", index, count) : std::wstring();
        std::wstring           text    = std::format (L"{} already has a file called \"{}\".{}\n\nReplace it with the copy, skip this file, or keep both?",
                                                      path.parent_path().filename().wstring(), path.filename().wstring(), ofCount);
        int                    choice  = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, text.c_str(), L"Replace or skip files", MB_YESNOCANCEL | MB_DEFBUTTON3,
                                                         { L"Replace", L"Skip", L"Keep both" });

        return (choice == IDYES) ? CassoExplorerActions::Collision::Replace
             : (choice == IDNO)  ? CassoExplorerActions::Collision::Skip
                                 : CassoExplorerActions::Collision::KeepBoth;
    };
}
