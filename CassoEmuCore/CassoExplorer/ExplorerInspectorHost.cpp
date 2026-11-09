#include "Pch.h"

#include "CassoExplorer/ExplorerInspectorHost.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"
#include "Devices/Disk/MountDiagnosis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::ExplorerInspectorHost
//
////////////////////////////////////////////////////////////////////////////////

ExplorerInspectorHost::ExplorerInspectorHost (const std::wstring & path) :
    m_path     (path),
    m_utf8Path (TextEncoding::WideToUtf8 (path)),
    m_mediaId  (InspectorImageLoader::MakeMediaId())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::PostInspectorRequest
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ExplorerInspectorHost::PostInspectorRequest (const InspectorRequest & request)
{
    uint64_t  id = m_queue.Post (request);



    m_queue.Drain (*this);

    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::TakeInspectorReplies
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerInspectorHost::TakeInspectorReplies (vector<InspectorReply> & outReplies)
{
    m_queue.TakeReplies (outReplies);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::Reload
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerInspectorHost::Reload()
{
    m_isRead = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::GetImage
//
//  Drive 0 is the file. Every read has the window's media id, so the
//  selected track stays selected after the file is read again (FR-074).
//
////////////////////////////////////////////////////////////////////////////////

const DiskImage * ExplorerInspectorHost::GetImage (int drive, InspectorDiskIdentity & outIdentity)
{
    if (drive != 0)
    {
        return nullptr;
    }

    if (!m_isRead)
    {
        Load();
    }

    outIdentity = m_identity;

    return (m_image != nullptr && m_image->IsLoaded()) ? m_image.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::GetOpenError
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerInspectorHost::GetOpenError (int drive)
{
    return (drive == 0) ? m_openError : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::Load
//
//  The format is chosen by the extension, as for a mount. For a file that
//  cannot be read or loaded, m_image is empty and m_openError has the
//  reason a mount fails with.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerInspectorHost::Load()
{
    HRESULT         hr         = S_OK;
    std::wstring    name       = fs::path (m_path).filename().wstring();
    DWORD           attributes = GetFileAttributesW (m_path.c_str());
    DiskFormat      format     = DiskFormat::Dsk;
    MountDiagnosis  diagnosis;
    vector<Byte>    bytes;



    m_isRead = true;
    m_image  = std::make_unique<DiskImage>();
    m_openError.clear();

    m_identity            = InspectorDiskIdentity();
    m_identity.mediaId    = m_mediaId;
    m_identity.fileName   = m_utf8Path;
    m_identity.isReadOnly = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) != 0;

    hr = DiskImageStore::GetSourceFormatByExtension (m_utf8Path, format);
    CHRF (hr, m_openError = std::format (L"{} is not a disk image Casso opens.", name));

    hr = DiskImageStore::ReadFileBytes (m_utf8Path, bytes);
    CHRF (hr, m_openError = std::format (L"{} could not be read.", name));

    m_identity.fileSize = bytes.size();
    m_image->LoadFromBytes (format, bytes, m_utf8Path);

    if (!m_image->IsLoaded())
    {
        diagnosis   = DiskImageStore::ClassifyLoadFailure (format, bytes);
        m_openError = std::format (L"{} {}", name, TextEncoding::NarrowToWide (diagnosis.Describe()));
    }

Error:
    return;
}
