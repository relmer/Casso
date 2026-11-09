#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerProperties.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/AppleFilePropertiesDialog.h"
#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/WozPropertiesDialog.h"
#include "CassoExplorer/Model/HostAppleFile.h"
#include "CassoExplorer/Model/WozProperties.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties::ExplorerProperties
//
////////////////////////////////////////////////////////////////////////////////

ExplorerProperties::ExplorerProperties (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties::ShowRowProperties
//
//  A host item, a drive among them, has Windows' own Properties sheet. An
//  entry inside an image has none, so its catalog details are shown instead.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerProperties::ShowRowProperties (int row)
{
    Location                           location = m_context.browser.GetLocation();
    std::vector<DxuiListView::Column>  columns  = BrowserCells::GetColumns();
    std::vector<DxuiListView::Cell>    cells;
    std::wstring                       path;
    std::wstring                       text;
    size_t                             i        = 0;
    FileEntry                          entry;
    CassoExplorerBrowser::SearchMatch  match;
    std::wstring                       error;



    //  An entry inside an image or on a linked drive: its Apple properties,
    //  editable.
    if (m_context.browser.IsVolumeLocation() && m_context.browser.TryGetRowEntry (row, entry))
    {
        ShowAppleProperties();
        return;
    }

    if (!m_context.browser.TryGetRowPath (row, path))
    {
        return;
    }

    //  Anything on the disk, a search's result among them, has Windows' sheet.
    if (location.kind == Location::Kind::HostFolder || location.kind == Location::Kind::Root || GetFileAttributesW (path.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        ShowHostProperties (path);
        return;
    }

    //  A search's match inside an image has the sheet it has in its image.
    if (m_context.browser.TryFindSearchMatch (m_context.browser.GetRows()[(size_t) row], match, error))
    {
        CassoExplorerPropertiesDialog::Show (m_context.window.GetHwnd(), m_context.theme, m_context.browser.GetRows()[(size_t) row].name,
                                            CassoExplorerProperties::DescribeEntry (match.entry, match.kind,
                                                BrowserModel::FormatAddress (match.directory.empty() ? Location::MakeDiskImage (match.imagePath)
                                                                                                     : Location::MakeDiskDirectory (match.imagePath, match.directory))));
        return;
    }

    cells = BrowserCells::ToCells (m_context.browser.GetRows()[(size_t) row], location);
    text  = path + L"\n\n";

    for (i = 0; i < columns.size() && i < cells.size(); i++)
    {
        if (!cells[i].text.empty() && !columns[i].title.empty())
        {
            text += columns[i].title + L": " + cells[i].text + L"\n";
        }
    }

    m_context.window.ShowMessage (text, MB_ICONINFORMATION);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties::ShowAppleProperties
//
//  The selected entries of the open disk, in the Apple file properties
//  dialog. Its edits are written as it applies them, and the list is filled
//  again after each.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerProperties::ShowAppleProperties()
{
    std::vector<FileEntry>  entries;
    bool                    readOnly = m_context.browser.IsImageLocation() && m_context.browser.IsWriteProtected();



    m_context.browser.GetSelectedEntries (entries);

    AppleFilePropertiesDialog::Show (m_context.window.GetHwnd(), m_context.theme, entries, m_context.browser.GetVolumeKind(), readOnly,
                                     [this] (const AppleFileProperties::Edits & edits, std::vector<FileEntry> & inOutEntries) -> std::wstring
    {
        CassoExplorerActions::Outcome  outcome = m_context.actions.ApplyProperties (inOutEntries, edits);

        m_context.listPane.FillList();

        //  The dialog's files as the list has them now, so the next Apply
        //  starts from what this one wrote. The list's selection doesn't
        //  survive the refill, so the files go by name.
        for (FileEntry & entry : inOutEntries)
        {
            std::string  name = (edits.nameEdited && inOutEntries.size() == 1) ? TextEncoding::WideToNarrow (edits.name) : entry.name;
            FileEntry    fresh;

            for (int row = 0; row < (int) m_context.browser.GetRows().size(); row++)
            {
                if (m_context.browser.TryGetRowEntry (row, fresh) && _stricmp (fresh.name.c_str(), name.c_str()) == 0)
                {
                    entry = fresh;
                    break;
                }
            }
        }

        return outcome.Succeeded() ? std::wstring() : (outcome.message.empty() ? std::wstring (L"The changes weren't written.") : outcome.message);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties::IsHostAppleFile
//
//  A file on the PC whose name is in the Descriptive or CiderPress form, or
//  an AppleSingle container, known here by its .as name; opening the dialog
//  reads it to be sure.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerProperties::IsHostAppleFile (const std::wstring & path)
{
    std::wstring    leaf   = std::filesystem::path (path).filename().wstring();
    ParsedHostName  parsed;



    if (_wcsicmp (std::filesystem::path (path).extension().wstring().c_str(), L".as") == 0)
    {
        return true;
    }

    return HostFileNaming::Parse (leaf, parsed) && parsed.hasType && parsed.converted == ParsedHostName::ConvertedKind::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties::IsWozImage
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerProperties::IsWozImage (const std::wstring & path)
{
    return _wcsicmp (std::filesystem::path (path).extension().wstring().c_str(), L".woz") == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties::ShowWozProperties
//
//  A WOZ image's INFO and META in Disk properties. Each Apply reads the file
//  again, makes the edits in its bytes and writes it, then previews it again.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerProperties::ShowWozProperties (const std::wstring & path, int tab)
{
    IDiskFileIo             & io    = m_context.browser.GetOperations().GetFileIo();
    std::string               file  = TextEncoding::WideToNarrow (path);
    std::wstring              leaf  = std::filesystem::path (path).filename().wstring();
    std::vector<Byte>         bytes;
    WozLoader::Description    woz;
    HRESULT                   hr    = S_OK;



    hr = io.ReadAllBytes (file, bytes);

    if (SUCCEEDED (hr))
    {
        WozLoader::Describe (bytes, woz);
    }

    if (!woz.isWoz)
    {
        m_context.window.ShowMessage (leaf + L" can't be read as a WOZ image.", MB_ICONINFORMATION);
        return;
    }

    WozPropertiesDialog::Show (m_context.window.GetHwnd(), m_context.theme, leaf, woz, tab, [this, &io, file] (const WozLoader::Edits & edits) -> std::wstring
    {
        std::vector<Byte>  fresh;
        HRESULT            result = io.ReadAllBytes (file, fresh);

        if (SUCCEEDED (result))
        {
            result = WozLoader::ApplyEdits (fresh, edits);
        }

        if (SUCCEEDED (result))
        {
            result = io.WriteAllBytes (file, fresh);
        }

        if (SUCCEEDED (result))
        {
            m_context.browser.RefreshPreview();
            m_context.preview.FillPreview();
        }

        return SUCCEEDED (result) ? std::wstring() : WozProperties::FormatEditError (result);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties::ShowHostAppleProperties
//
//  An Apple file on the PC in the Apple file properties dialog, in ProDOS's
//  vocabulary. Each Apply rewrites an AppleSingle file and renames the file
//  for its form, then reads it again for the next.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerProperties::ShowHostAppleProperties (const std::wstring & path)
{
    IDiskFileIo                             & io      = m_context.browser.GetOperations().GetFileIo();
    std::vector<Byte>                         bytes;
    HostAppleFile                             file;
    AppleFilePropertiesDialog::HostOptions    options;
    std::wstring                              current = path;
    HRESULT                                   hr      = S_OK;



    hr = io.ReadAllBytes (TextEncoding::WideToNarrow (path), bytes);

    if (SUCCEEDED (hr))
    {
        file = HostAppleFile::Read (std::filesystem::path (path).filename().wstring(), bytes);
    }

    if (file.form == HostAppleFile::Form::None)
    {
        m_context.window.ShowMessage (std::filesystem::path (path).filename().wstring() + L" isn't an Apple file: its name doesn't record a type, and it isn't AppleSingle.",
                                      MB_ICONINFORMATION);
        return;
    }

    options.showAccess  = file.form == HostAppleFile::Form::AppleSingle;
    options.showDates   = file.form == HostAppleFile::Form::AppleSingle;
    options.offerRename = file.form == HostAppleFile::Form::AppleSingle;
    options.nameKind    = VolumeKind::Dos33;
    options.leafName    = file.leafName;
    options.isTaken     = [folder = std::filesystem::path (path).parent_path()] (const std::wstring & leaf)
    {
        return GetFileAttributesW ((folder / leaf).c_str()) != INVALID_FILE_ATTRIBUTES;
    };

    AppleFilePropertiesDialog::Show (m_context.window.GetHwnd(), m_context.theme, { file.entry }, VolumeKind::ProDos, false,
                                     [this, &io, &file, &current] (const AppleFileProperties::Edits & edits, std::vector<FileEntry> & inOutEntries) -> std::wstring
    {
        HostAppleFile::Change  change;
        std::wstring           target;
        std::vector<Byte>      fresh;
        HRESULT                result = file.Apply (edits, edits.renameHostFile, change);

        if (result == HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED))
        {
            return L"This AppleSingle file has parts, such as a resource fork, that rewriting it would drop, so it can't be changed here.";
        }

        if (SUCCEEDED (result) && !change.bytes.empty())
        {
            result = io.WriteAllBytes (TextEncoding::WideToNarrow (current), change.bytes);
        }

        target = (std::filesystem::path (current).parent_path() / change.leafName).wstring();

        if (SUCCEEDED (result) && _wcsicmp (target.c_str(), current.c_str()) != 0)
        {
            result = MoveFileExW (current.c_str(), target.c_str(), 0) ? S_OK : HRESULT_FROM_WIN32 (GetLastError());

            if (SUCCEEDED (result))
            {
                current = target;
            }
        }

        m_context.folderSync.RefreshAfterHostChange();

        if (FAILED (result))
        {
            return L"The changes weren't written: " + TextEncoding::NarrowToWide (std::system_category().message (HRESULT_CODE (result)));
        }

        result = io.ReadAllBytes (TextEncoding::WideToNarrow (current), fresh);

        if (SUCCEEDED (result))
        {
            file         = HostAppleFile::Read (std::filesystem::path (current).filename().wstring(), fresh);
            inOutEntries = { file.entry };
        }

        return std::wstring();
    }, &options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties::ShowLocationProperties
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerProperties::ShowLocationProperties (const Location & location)
{
    //  Home and the roots have no host path to show properties for.
    if (location.kind == Location::Kind::None || location.kind == Location::Kind::Root)
    {
        return;
    }

    if (location.kind == Location::Kind::DiskDirectory)
    {
        m_context.window.ShowMessage (BrowserModel::FormatAddress (location), MB_ICONINFORMATION);
        return;
    }

    ShowHostProperties (location.path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties::ShowHostProperties
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerProperties::ShowHostProperties (const std::wstring & path)
{
    BOOL  shown = SHObjectProperties (m_context.window.GetHwnd(), SHOP_FILEPATH, path.c_str(), nullptr);



    IGNORE_RETURN_VALUE (shown, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties::ShowHostProperties
//
//  Open Windows' single Properties sheet for several items, as File Explorer
//  does for a selection.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerProperties::ShowHostProperties (const std::vector<std::wstring> & paths)
{
    HRESULT                            hr = S_OK;
    std::vector<PIDLIST_ABSOLUTE>      ids;
    ComPtr<IShellItemArray>            items;
    ComPtr<IDataObject>                data;



    for (const std::wstring & path : paths)
    {
        PIDLIST_ABSOLUTE  id = nullptr;

        hr = SHParseDisplayName (path.c_str(), nullptr, &id, 0, nullptr);
        CHR (hr);

        ids.push_back (id);
    }

    hr = SHCreateShellItemArrayFromIDLists ((UINT) ids.size(), (PCIDLIST_ABSOLUTE_ARRAY) ids.data(), &items);
    CHR (hr);

    hr = items->BindToHandler (nullptr, BHID_DataObject, IID_PPV_ARGS (&data));
    CHR (hr);

    hr = SHMultiFileProperties (data.Get(), 0);
    CHR (hr);

Error:
    for (PIDLIST_ABSOLUTE id : ids)
    {
        CoTaskMemFree (id);
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}
