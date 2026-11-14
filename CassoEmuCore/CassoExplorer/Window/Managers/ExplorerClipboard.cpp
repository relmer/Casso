#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerClipboard.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerTransferPrompts.h"
#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "Core/AppMessages.h"
#include "Core/TextEncoding.h"
#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerClipboard::ExplorerClipboard
//
////////////////////////////////////////////////////////////////////////////////

ExplorerClipboard::ExplorerClipboard (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerClipboard::GetPasteFolder
//
//  A single folder row takes the paste; otherwise the folder being shown.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerClipboard::GetPasteFolder() const
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
//  ExplorerClipboard::CopyEntriesToClipboard
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerClipboard::CopyEntriesToClipboard (HostFileNaming::Style style, bool raw)
{
    std::wstring  failure;
    HRESULT       hr      = CassoExplorerDragOut::CopyToClipboard (m_context.browser, style, failure, false, nullptr, raw);



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
//  ExplorerClipboard::CutEntriesToClipboard
//
//  What is deleted is read now, while the entries are on display: the
//  paste may come after the window has moved elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerClipboard::CutEntriesToClipboard (HostFileNaming::Style style)
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
//  ExplorerClipboard::FinishCut
//
//  The cut entries deleted, now that a target has pasted them. A drive's
//  deletions go over the link and are reported as they finish.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerClipboard::FinishCut()
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
//  ExplorerClipboard::CopySelectedPaths
//
//  Each path in quotes on its own line, as Explorer's Copy as path gives them.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerClipboard::CopySelectedPaths()
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
//  ExplorerClipboard::PasteHere
//
//  The clipboard's files into the folder, with an undo that takes back what
//  the paste made: a copy goes to the Recycle Bin, a move goes back.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerClipboard::PasteHere (const std::wstring & folder)
{
    IShellItemVerbs::PasteResult  pasted;
    ComPtr<IDataObject>           data;
    HRESULT                       hr     = OleGetClipboard (&data);



    //  An image's or a linked Apple's files are written here rather than by
    //  the folder's drop target, whose name clash prompt has no Keep both and
    //  gives the files a folder they never came from.
    if (SUCCEEDED (hr) && ActionsCopy::IsCassoCopy (data.Get()))
    {
        CassoExplorerActions::Outcome  outcome = m_context.actions.PasteVirtualFiles (data.Get(), folder, m_context.prompts.MakeCollisionPrompt(), pasted.created);

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

    //  A file cut from an A2Pico card is a drive's image there no longer.
    for (size_t i = 0; pasted.moved && i < pasted.sources.size() && i < pasted.created.size(); i++)
    {
        m_context.actions.GetA2retroNet().NoteMove (pasted.sources[i], pasted.created[i]);
    }

    //  An image pasted onto an A2Pico card that A2retroNET doesn't read is
    //  offered as a .po once it is listed.
    m_context.actions.GetA2retroNet().ExpectArrivals (pasted.created);

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
//  ExplorerClipboard::PasteIntoVolume
//
//  The clipboard's files into the disk image or the linked Apple's drive
//  open in the list, by the rules a drop there follows: another image's
//  entries are copied byte for byte, and host files go in by Put's rules.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerClipboard::PasteIntoVolume()
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
                                   : m_context.actions.PutFiles (source.hostPaths, m_context.prompts.MakeAddressPrompt());

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
                               : m_context.actions.PutInto (location.path, targetKind, inner, source.hostPaths, m_context.prompts.MakeAddressPrompt());

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
//  ExplorerClipboard::ReportPasted
//
//  As File Explorer reports a paste of a cut, so the source deletes what it
//  cut: only when every file landed whole.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerClipboard::ReportPasted (IDataObject * data, const CassoExplorerActions::Outcome & outcome)
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
//  ExplorerClipboard::IsCutFrom
//
//  Whether the last cut's entries came from the image directory `inner`.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerClipboard::IsCutFrom (const std::string & inner) const
{
    if (!m_cut.valid || m_cut.onDrive || m_cut.catalogPaths.empty())
    {
        return false;
    }

    const std::string &  first = m_cut.catalogPaths[0];
    size_t               slash = first.find_last_of ('/');

    return ((slash == std::string::npos) ? std::string() : first.substr (0, slash)) == inner;
}
