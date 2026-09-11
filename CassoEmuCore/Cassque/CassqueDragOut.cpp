#include "Pch.h"

#include "Cassque/CassqueDragOut.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueDragOut::GetEncoding
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Encoding CassqueDragOut::GetEncoding (const DragPayload::Descriptor & descriptor)
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
//  CassqueDragOut::MakeFileGroupDescriptor
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> CassqueDragOut::MakeFileGroupDescriptor (const std::vector<DragPayload::Descriptor> & descriptors)
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
//  CassqueDragOut::MakeHDrop
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> CassqueDragOut::MakeHDrop (const std::vector<std::wstring> & paths)
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
//  CassqueDragOut::BuildFormats
//
//  The render callbacks hold what they need by value -- the image path and
//  the descriptors -- and the browser by reference, since the drag runs to
//  completion inside the window that owns both.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDragDropSource::Format> CassqueDragOut::BuildFormats (CassqueBrowser & browser, HostFileNaming::Style style)
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



    if (browser.IsImageLocation())
    {
        browser.GetSelectedEntries (entries);

        if (entries.empty())
        {
            return formats;
        }

        plan = DragPayload::Build (DragPayload::SourceKind::CatalogEntries, image, browser.GetVolumeKind(), entries, style,
                                   [] (const std::string &, VolumeListing &) { return E_NOTIMPL; }, {});

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

                                 result = browser.GetOperations().Get (image, descriptor.catalogPath, GetEncoding (descriptor), "");

                                 if (result.Succeeded())
                                 {
                                     out.assign (result.payload.begin(), result.payload.end());
                                 }

                                 return result.hr;
                             } });

        formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatA (DragPayload::kPrivateFormatName), 1,
                             [privateData] (int, std::vector<uint8_t> & out) { out.assign (privateData->begin(), privateData->end()); return S_OK; } });

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
