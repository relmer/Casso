#include "Pch.h"

#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::WriteToTempFolder
//
//  From the very formats a drag offers, so a file opened is the file a drag
//  would give: the descriptor names each, and its contents are read in turn.
//  A folder in the selection is left out; Explorer opens those as folders.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerDragOut::WriteToTempFolder (CassoExplorerBrowser & browser, HostFileNaming::Style style, std::vector<std::wstring> & outPaths)
{
    HRESULT                                    hr             = S_OK;
    std::vector<DxuiDragDropSource::Format>    formats        = BuildFormats (browser, style);
    CLIPFORMAT                                 descriptor     = (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW);
    CLIPFORMAT                                 contents       = (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILECONTENTS);
    const DxuiDragDropSource::Format         * names          = nullptr;
    const DxuiDragDropSource::Format         * bodies         = nullptr;
    std::vector<uint8_t>                       group;
    wchar_t                                    temp[MAX_PATH] = {};
    wchar_t                                    unique[64]     = {};
    GUID                                       id             = {};
    std::wstring                               folder;
    UINT                                       count          = 0;
    int                                        made           = 0;
    size_t                                     groupBytes     = 0;
    DWORD                                      tempChars      = 0;
    int                                        created        = 0;



    outPaths.clear();

    for (const DxuiDragDropSource::Format & format : formats)
    {
        names  = (format.format == descriptor) ? &format : names;
        bodies = (format.format == contents)   ? &format : bodies;
    }

    CBR (names != nullptr && bodies != nullptr);

    hr = names->render (0, group);
    CHR (hr);

    groupBytes = group.size();
    CBR (groupBytes >= sizeof (UINT));

    count = reinterpret_cast<const FILEGROUPDESCRIPTORW *> (group.data())->cItems;
    CBR (groupBytes >= sizeof (UINT) + (size_t) count * sizeof (FILEDESCRIPTORW));

    //  A folder of its own each time, so two files of one name never meet.
    hr = CoCreateGuid (&id);
    CHR (hr);

    made      = StringFromGUID2 (id, unique, (int) std::size (unique));
    tempChars = GetTempPathW (MAX_PATH, temp);
    CBR (made > 0 && tempChars > 0);

    folder  = std::wstring (temp) + L"Casso Explorer\\" + unique;
    created = SHCreateDirectoryExW (nullptr, folder.c_str(), nullptr);
    CBR (created == ERROR_SUCCESS);

    for (UINT index = 0; index < count; index++)
    {
        const FILEDESCRIPTORW &  file    = reinterpret_cast<const FILEGROUPDESCRIPTORW *> (group.data())->fgd[index];
        std::vector<uint8_t>     bytes;
        std::wstring             path    = folder + L"\\" + file.cFileName;
        HANDLE                   handle  = INVALID_HANDLE_VALUE;
        DWORD                    written = 0;
        BOOL                     wrote   = FALSE;
        size_t                   length  = 0;

        if ((file.dwFlags & FD_ATTRIBUTES) != 0 && (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            continue;
        }

        hr = bodies->render ((int) index, bytes);
        CHR (hr);

        length = bytes.size();

        handle = CreateFileW (path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        CBREx (handle != INVALID_HANDLE_VALUE, HRESULT_FROM_WIN32 (GetLastError()));

        wrote = WriteFile (handle, bytes.data(), (DWORD) length, &written, nullptr);
        CloseHandle (handle);
        CBREx (wrote && written == length, HRESULT_FROM_WIN32 (GetLastError()));

        outPaths.push_back (path);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::GetEncoding
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Encoding CassoExplorerDragOut::GetEncoding (const DragPayload::Descriptor & descriptor)
{
    ParsedHostName  parsed;
    std::wstring    leaf  = descriptor.relativePath;
    size_t          slash = leaf.find_last_of (L"\\/");



    if (!descriptor.converted)
    {
        return DiskOperations::Encoding::Verbatim;
    }

    if (slash != std::wstring::npos)
    {
        leaf = leaf.substr (slash + 1);
    }

    if (HostFileNaming::Parse (leaf, parsed)
     && (parsed.converted == ParsedHostName::ConvertedKind::ApplesoftListing || parsed.converted == ParsedHostName::ConvertedKind::IntegerListing))
    {
        return DiskOperations::Encoding::Basic;
    }

    return DiskOperations::Encoding::Text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::CopyToClipboard
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerDragOut::CopyToClipboard (CassoExplorerBrowser & browser, HostFileNaming::Style style)
{
    HRESULT                                  hr      = S_OK;
    std::vector<DxuiDragDropSource::Format>  formats = BuildFormats (browser, style);
    std::vector<DxuiDragDropSource::Format>  held;
    DxuiDragDropSource                     * source  = nullptr;
    bool                                     empty   = formats.empty();



    CBR (!empty);

    //  Everything the formats would render on demand, read now.
    for (DxuiDragDropSource::Format & format : formats)
    {
        std::shared_ptr<std::vector<std::vector<uint8_t>>>  bytes = std::make_shared<std::vector<std::vector<uint8_t>>>();

        for (int index = 0; index < (format.count > 0 ? format.count : 1); index++)
        {
            std::vector<uint8_t>  one;

            hr = format.render (index, one);
            CHR (hr);

            bytes->push_back (std::move (one));
        }

        held.push_back (DxuiDragDropSource::Format { format.format, format.count,
                                                     [bytes] (int index, std::vector<uint8_t> & out)
                                                     {
                                                         size_t  at = (index >= 0 && (size_t) index < bytes->size()) ? (size_t) index : 0;

                                                         out = (*bytes)[at];
                                                         return S_OK;
                                                     } });
    }

    hr = DxuiDragDropSource::Create (std::move (held), &source);
    CHR (hr);

    hr = OleSetClipboard (source);
    CHR (hr);

Error:
    if (source != nullptr)
    {
        source->Release();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::DescribeCopyFailure
//
//  The clipboard cannot be opened while another program has it open, and
//  in a session that is locked.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerDragOut::DescribeCopyFailure (HRESULT hr)
{
    std::wstring  text;



    if (hr == CLIPBRD_E_CANT_OPEN || hr == CLIPBRD_E_CANT_EMPTY || hr == CLIPBRD_E_CANT_SET || hr == CLIPBRD_E_CANT_CLOSE)
    {
        text = L"the clipboard could not be opened. Another program may have it open, or Windows may be locked.";
    }
    else if (hr == HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE) || hr == HRESULT_FROM_WIN32 (ERROR_TIMEOUT))
    {
        text = L"the Apple stopped answering while its files were read.";
    }
    else
    {
        text = TextEncoding::NarrowToWide (std::system_category().message ((int) hr));

        while (!text.empty() && (text.back() == L'.' || text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
        {
            text.pop_back();
        }

        if (!text.empty())
        {
            text[0] = (wchar_t) towlower (text[0]);
        }

        text = text.empty() ? std::format (L"error 0x{:08X}.", (uint32_t) hr) : text + L".";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::MakeFileGroupDescriptor
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> CassoExplorerDragOut::MakeFileGroupDescriptor (const std::vector<DragPayload::Descriptor> & descriptors)
{
    size_t                 count = descriptors.size();
    size_t                 bytes = sizeof (FILEGROUPDESCRIPTORW) + (count > 0 ? (count - 1) * sizeof (FILEDESCRIPTORW) : 0);
    std::vector<uint8_t>   out (bytes, 0);
    FILEGROUPDESCRIPTORW * group = (FILEGROUPDESCRIPTORW *) out.data();
    size_t                 i     = 0;



    group->cItems = (UINT) count;

    for (i = 0; i < count; i++)
    {
        FILEDESCRIPTORW &  item = group->fgd[i];

        item.dwFlags          = FD_ATTRIBUTES | FD_PROGRESSUI;
        item.dwFileAttributes = descriptors[i].isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

        wcsncpy_s (item.cFileName, ARRAYSIZE (item.cFileName), descriptors[i].relativePath.c_str(), _TRUNCATE);
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::MakeHDrop
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> CassoExplorerDragOut::MakeHDrop (const std::vector<std::wstring> & paths)
{
    size_t                  chars  = 1;
    std::vector<uint8_t>    out;
    DROPFILES             * header = nullptr;
    wchar_t               * cursor = nullptr;



    for (const std::wstring & path : paths)
    {
        chars += path.size() + 1;
    }

    out.assign (sizeof (DROPFILES) + chars * sizeof (wchar_t), 0);

    header         = (DROPFILES *) out.data();
    header->pFiles = sizeof (DROPFILES);
    header->fWide  = TRUE;

    cursor = (wchar_t *) (out.data() + sizeof (DROPFILES));

    for (const std::wstring & path : paths)
    {
        memcpy (cursor, path.c_str(), path.size() * sizeof (wchar_t));
        cursor += path.size() + 1;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::BuildAppleDriveFormats
//
//  Files on a linked Apple's drive, with the host names of a copy out. A
//  drop requests each file's bytes and cannot complete without them, so each
//  is read over the link then, for up to kDropWaitMs. Folders are not
//  dragged from a drive.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDragDropSource::Format> CassoExplorerDragOut::BuildAppleDriveFormats (CassoExplorerBrowser & browser, HostFileNaming::Style style)
{
    std::vector<DxuiDragDropSource::Format>                  formats;
    std::vector<FileEntry>                                   entries;
    std::vector<FileEntry>                                   files;
    DragPayload::Plan                                        plan;
    std::shared_ptr<std::vector<DragPayload::Descriptor>>    descriptors;
    std::shared_ptr<std::vector<uint8_t>>                    group;
    std::wstring                                             link;
    std::string                                              directory = browser.GetLocation().innerPath;
    int                                                      slot      = 0;
    int                                                      drive     = 0;
    CassoLinkConnections                                   * links     = browser.GetLinks();



    TreeModel::TryParseLinkId (browser.GetLocation().path, link, slot, drive);
    browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        if (!entry.isDirectory)
        {
            files.push_back (entry);
        }
    }

    if (files.empty() || links == nullptr)
    {
        return formats;
    }

    plan = DragPayload::Build (DragPayload::SourceKind::CatalogEntries, std::string(), browser.GetVolumeKind(), files, style,
                               [] (const std::string &, VolumeListing &) { return E_NOTIMPL; }, {});

    descriptors = std::make_shared<std::vector<DragPayload::Descriptor>> (plan.descriptors);
    group       = std::make_shared<std::vector<uint8_t>> (MakeFileGroupDescriptor (plan.descriptors));

    formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW), 1,
                         [group] (int, std::vector<uint8_t> & out) { out = *group; return S_OK; } });

    formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILECONTENTS), (int) descriptors->size(),
                         [links, link = TextEncoding::WideToNarrow (link), drive, directory, descriptors] (int index, std::vector<uint8_t> & out)
                         {
                             const DragPayload::Descriptor &  descriptor = (*descriptors)[(size_t) index];
                             std::string                      path       = directory.empty() ? descriptor.catalogPath : directory + "/" + descriptor.catalogPath;
                             FilePayload                      payload;
                             HRESULT                          hr         = links->WaitForFile (link, drive, path, kDropWaitMs, &CassoExplorerDragOut::PumpWhileWaiting, payload);

                             out.clear();

                             if (SUCCEEDED (hr))
                             {
                                 hr = CassoExplorerActions::ToHostBytes (payload, GetEncoding (descriptor), descriptor.appleSingle, descriptor.single, out);
                             }

                             return hr;
                         } });

    return formats;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::BuildDiskFormats
//
//  The image's bytes are read on demand: the disk is read over the link into
//  a temporary file, and the drop waits for it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDragDropSource::Format> CassoExplorerDragOut::BuildDiskFormats (CassoExplorerBrowser & browser, const std::wstring & driveId)
{
    std::vector<DxuiDragDropSource::Format>  formats;
    CassoLinkConnections                   * links   = browser.GetLinks();
    IDiskFileIo                            * fileIo  = &browser.GetOperations().GetFileIo();
    std::wstring                             link;
    int                                      slot    = 0;
    int                                      drive   = 0;
    VolumeListing                            listing;
    VolumeKind                               kind    = VolumeKind::Unknown;
    HRESULT                                  result  = S_OK;
    std::string                              message;
    DragPayload::Descriptor                  descriptor;
    std::vector<uint8_t>                     id ((driveId.size() + 1) * sizeof (wchar_t), 0);
    std::shared_ptr<std::vector<uint8_t>>    group;



    if (links == nullptr || !TreeModel::TryParseLinkId (driveId, link, slot, drive) || drive == 0)
    {
        return formats;
    }

    if (!links->TryGetListing (TextEncoding::WideToNarrow (link), drive, std::string(), listing, kind, result, message) || FAILED (result))
    {
        listing = VolumeListing();
        kind    = VolumeKind::Unknown;
    }

    descriptor.relativePath = CassoExplorerActions::MakeDriveImageName (listing, kind, slot, drive);
    group                   = std::make_shared<std::vector<uint8_t>> (MakeFileGroupDescriptor ({ descriptor }));

    memcpy (id.data(), driveId.c_str(), driveId.size() * sizeof (wchar_t));

    formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (kDriveFormatName), 1,
                         [id] (int, std::vector<uint8_t> & out) { out = id; return S_OK; } });

    formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW), 1,
                         [group] (int, std::vector<uint8_t> & out) { out = *group; return S_OK; } });

    formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILECONTENTS), 1,
                         [links, fileIo, link = TextEncoding::WideToNarrow (link), drive, name = descriptor.relativePath] (int, std::vector<uint8_t> & out)
                         {
                             HRESULT              hr             = S_OK;
                             wchar_t              temp[MAX_PATH] = {};
                             std::string          path;
                             CassoLinkSession   * session        = links->GetSession (link);
                             auto                 until          = std::chrono::steady_clock::now() + std::chrono::milliseconds (kDiskDropWaitMs);
                             std::vector<Byte>    bytes;

                             out.clear();

                             if (session == nullptr || GetTempPathW (MAX_PATH, temp) == 0)
                             {
                                 return HRESULT_FROM_WIN32 (ERROR_NOT_READY);
                             }

                             path = TextEncoding::WideToNarrow (std::wstring (temp) + L"casso-drive-" + name);

                             auto  operation = session->ReadDisk (drive, path, false);

                             while ((operation->state == LinkOperationState::Queued || operation->state == LinkOperationState::Running)
                                    && std::chrono::steady_clock::now() < until)
                             {
                                 PumpWhileWaiting (CassoLinkConnections::kWaitPollMs);
                             }

                             hr = operation->state == LinkOperationState::Done ? fileIo->ReadAllBytes (path, bytes) : HRESULT_FROM_WIN32 (ERROR_TIMEOUT);

                             if (SUCCEEDED (hr))
                             {
                                 out.assign (bytes.begin(), bytes.end());
                             }

                             if (fileIo->Exists (path))
                             {
                                 fileIo->Remove (path);
                             }

                             return hr;
                         } });

    return formats;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::TryGetDraggedDrive
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerDragOut::TryGetDraggedDrive (IDataObject * data, std::wstring & outDriveId)
{
    FORMATETC  format = { (CLIPFORMAT) RegisterClipboardFormatW (kDriveFormatName), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM  medium = {};
    bool       found  = false;
    HRESULT    hr     = (data != nullptr) ? data->GetData (&format, &medium) : E_POINTER;



    outDriveId.clear();

    if (SUCCEEDED (hr))
    {
        const wchar_t  * text = static_cast<const wchar_t *> (GlobalLock (medium.hGlobal));

        if (text != nullptr)
        {
            outDriveId = text;
            found      = !outDriveId.empty();
            GlobalUnlock (medium.hGlobal);
        }

        ReleaseStgMedium (&medium);
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::BuildFormats
//
//  The render callbacks hold what they need by value -- the image path and
//  the descriptors -- and the browser by reference, since the drag runs to
//  completion inside the window that owns both.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDragDropSource::Format> CassoExplorerDragOut::BuildFormats (CassoExplorerBrowser & browser, HostFileNaming::Style style)
{
    std::vector<DxuiDragDropSource::Format>                  formats;
    std::vector<FileEntry>                                   entries;
    std::vector<std::wstring>                                hostPaths;
    DragPayload::Plan                                        plan;
    std::string                                              image       = TextEncoding::WideToNarrow (browser.GetLocation().path);
    std::shared_ptr<std::vector<DragPayload::Descriptor>>    descriptors;
    std::shared_ptr<std::vector<uint8_t>>                    group;
    std::shared_ptr<std::string>                             privateData;
    std::shared_ptr<std::vector<uint8_t>>                    drop;
    std::vector<CassoExplorerBrowser::SearchMatch>           matches;
    std::shared_ptr<std::vector<std::string>>                images;



    if (browser.IsAppleDriveLocation())
    {
        return BuildAppleDriveFormats (browser, style);
    }

    if (browser.IsImageLocation())
    {
        browser.GetSelectedEntries (entries);

        if (entries.empty())
        {
            return formats;
        }

        //  A folder's files are listed into the plan, so a drop gets them and
        //  not an empty folder, and a move takes away only what was copied.
        plan = DragPayload::Build (DragPayload::SourceKind::CatalogEntries, image, browser.GetVolumeKind(), entries, style,
                                   [&browser, image] (const std::string & directory, VolumeListing & outListing)
                                   {
                                       VolumeKind  listed = VolumeKind::Unknown;

                                       return browser.GetOperations().List (image, directory, outListing, listed).hr;
                                   },
                                   {}, browser.GetLocation().innerPath);

        descriptors = std::make_shared<std::vector<DragPayload::Descriptor>> (plan.descriptors);
        group       = std::make_shared<std::vector<uint8_t>> (MakeFileGroupDescriptor (plan.descriptors));
        privateData = std::make_shared<std::string> (plan.privateBytes);

        formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW), 1,
                             [group] (int, std::vector<uint8_t> & out) { out = *group; return S_OK; } });

        formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILECONTENTS), (int) descriptors->size(),
                             [&browser, image, descriptors] (int index, std::vector<uint8_t> & out)
                             {
                                 const DragPayload::Descriptor &  descriptor = (*descriptors)[(size_t) index];
                                 DiskOperations::Result           result;

                                 out.clear();

                                 if (descriptor.isDirectory)
                                 {
                                     return S_OK;
                                 }

                                 result = browser.GetOperations().Get (image, descriptor.catalogPath, GetEncoding (descriptor), "", descriptor.catalogIndex);

                                 if (result.Succeeded() && descriptor.appleSingle)
                                 {
                                     AppleSingleFile  single = descriptor.single;

                                     single.data = std::move (result.payload);
                                     AppleSingleCodec::Encode (single, out);
                                 }
                                 else if (result.Succeeded())
                                 {
                                     out.assign (result.payload.begin(), result.payload.end());
                                 }

                                 return result.hr;
                             } });

        formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatA (DragPayload::kPrivateFormatName), 1,
                             [privateData] (int, std::vector<uint8_t> & out) { out.assign (privateData->begin(), privateData->end()); return S_OK; } });

        return formats;
    }

    //  A search's matches inside disk images copy as they do from their
    //  images: each image's files planned as that image's, then put together.
    browser.GetSelectedSearchMatches (matches);

    if (!matches.empty())
    {
        descriptors = std::make_shared<std::vector<DragPayload::Descriptor>>();
        images      = std::make_shared<std::vector<std::string>>();

        for (size_t first = 0; first < matches.size(); )
        {
            size_t       last = first;
            std::string  from = TextEncoding::WideToNarrow (matches[first].imagePath);

            entries.clear();

            while (last < matches.size() && matches[last].imagePath == matches[first].imagePath && matches[last].directory == matches[first].directory)
            {
                entries.push_back (matches[last].entry);
                last++;
            }

            plan = DragPayload::Build (DragPayload::SourceKind::CatalogEntries, from, matches[first].kind, entries, style,
                                       [&browser, from] (const std::string & directory, VolumeListing & outListing)
                                       {
                                           VolumeKind  listed = VolumeKind::Unknown;

                                           return browser.GetOperations().List (from, directory, outListing, listed).hr;
                                       },
                                       {}, matches[first].directory);

            for (DragPayload::Descriptor & descriptor : plan.descriptors)
            {
                descriptors->push_back (descriptor);
                images->push_back (from);
            }

            first = last;
        }

        group = std::make_shared<std::vector<uint8_t>> (MakeFileGroupDescriptor (*descriptors));

        formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW), 1,
                             [group] (int, std::vector<uint8_t> & out) { out = *group; return S_OK; } });

        formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILECONTENTS), (int) descriptors->size(),
                             [&browser, images, descriptors] (int index, std::vector<uint8_t> & out)
                             {
                                 const DragPayload::Descriptor &  descriptor = (*descriptors)[(size_t) index];
                                 DiskOperations::Result           result;

                                 out.clear();

                                 if (descriptor.isDirectory)
                                 {
                                     return S_OK;
                                 }

                                 result = browser.GetOperations().Get ((*images)[(size_t) index], descriptor.catalogPath, GetEncoding (descriptor), "", descriptor.catalogIndex);

                                 if (result.Succeeded() && descriptor.appleSingle)
                                 {
                                     AppleSingleFile  single = descriptor.single;

                                     single.data = std::move (result.payload);
                                     AppleSingleCodec::Encode (single, out);
                                 }
                                 else if (result.Succeeded())
                                 {
                                     out.assign (result.payload.begin(), result.payload.end());
                                 }

                                 return result.hr;
                             } });

        return formats;
    }

    browser.GetSelectedHostPaths (hostPaths);

    if (hostPaths.empty())
    {
        return formats;
    }

    drop = std::make_shared<std::vector<uint8_t>> (MakeHDrop (hostPaths));

    formats.push_back ({ CF_HDROP, 1, [drop] (int, std::vector<uint8_t> & out) { out = *drop; return S_OK; } });

    return formats;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDragOut::PumpWhileWaiting
//
//  Painting, sent messages and posted ones, timers among them, are taken;
//  input is not, so nothing the user does starts while the drop runs.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerDragOut::PumpWhileWaiting (DWORD ms)
{
    MSG    msg    = {};
    DWORD  waited = MsgWaitForMultipleObjectsEx (0, nullptr, ms, QS_PAINT | QS_POSTMESSAGE | QS_SENDMESSAGE | QS_TIMER, 0);



    IGNORE_RETURN_VALUE (waited, WAIT_OBJECT_0);

    while (PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE | PM_QS_PAINT | PM_QS_POSTMESSAGE | PM_QS_SENDMESSAGE))
    {
        //  A quit is left for the main loop.
        if (msg.message == WM_QUIT)
        {
            PostQuitMessage ((int) msg.wParam);
            break;
        }

        TranslateMessage (&msg);
        DispatchMessageW (&msg);
    }
}
