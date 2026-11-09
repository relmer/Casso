#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerManifestSync.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerManifestSync::ExplorerManifestSync
//
////////////////////////////////////////////////////////////////////////////////

ExplorerManifestSync::ExplorerManifestSync (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerManifestSync::NoteRename
//
//  An image renamed in a Full Send folder is renamed in its manifest as
//  well. A manifest that can't be updated doesn't undo the rename.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerManifestSync::NoteRename (const std::wstring & oldPath, const std::wstring & newName)
{
    std::filesystem::path  old (oldPath);
    HRESULT                hr  = FullSendManifestFile::NoteRename (m_context.browser.GetOperations().GetFileIo(),
                                                                   TextEncoding::WideToNarrow (old.parent_path().wstring()),
                                                                   TextEncoding::WideToNarrow (old.filename().wstring()),
                                                                   TextEncoding::WideToNarrow (newName));



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerManifestSync::NoteDeletes
//
//  A path whose file exists was not deleted, as when the user canceled
//  Windows' confirmation.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerManifestSync::NoteDeletes (const std::vector<std::wstring> & paths)
{
    IDiskFileIo  & io = m_context.browser.GetOperations().GetFileIo();



    for (const std::wstring & path : paths)
    {
        std::filesystem::path  file (path);
        RemovedRows            removed;
        std::wstring           key  = path;
        HRESULT                hr   = S_OK;

        if (io.Exists (TextEncoding::WideToNarrow (path)))
        {
            continue;
        }

        removed.folder = TextEncoding::WideToNarrow (file.parent_path().wstring());

        hr = FullSendManifestFile::NoteDelete (io, removed.folder, TextEncoding::WideToNarrow (file.filename().wstring()), removed.removal);
        IGNORE_RETURN_VALUE (hr, S_OK);

        std::transform (key.begin(), key.end(), key.begin(), towlower);

        if (!removed.removal.IsEmpty())
        {
            m_removedRows[key] = std::move (removed);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerManifestSync::NoteRestores
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerManifestSync::NoteRestores (const std::vector<std::wstring> & paths)
{
    IDiskFileIo  & io = m_context.browser.GetOperations().GetFileIo();



    for (const std::wstring & path : paths)
    {
        std::wstring  key   = path;
        HRESULT       hr    = S_OK;

        std::transform (key.begin(), key.end(), key.begin(), towlower);

        auto  found = m_removedRows.find (key);

        if (found == m_removedRows.end())
        {
            continue;
        }

        hr = FullSendManifestFile::NoteRestore (io, found->second.folder, found->second.removal);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_removedRows.erase (found);
    }
}
