#include "Pch.h"

#include "Devices/Disk/Archives/WrappedImageFileIo.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DurableCommit.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::WrappedImageFileIo
//
////////////////////////////////////////////////////////////////////////////////

WrappedImageFileIo::WrappedImageFileIo (IDiskFileIo & host, const std::string & path) :
    m_host (host),
    m_path (ParseWrapped (path))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::IsWrapped
//
////////////////////////////////////////////////////////////////////////////////

bool WrappedImageFileIo::IsWrapped (const std::string & path)
{
    ContainerPath  parsed = ParseWrapped (path);



    //  A card's partition, such as \\.\PhysicalDrive2|#2.po, is a path in no
    //  container.
    return std::any_of (parsed.steps.begin(), parsed.steps.end(), [] (const ContainerStep & step)
    {
        return step.kind == ContainerStep::Kind::ZipMember || step.kind == ContainerStep::Kind::Gzip;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::ParseWrapped
//
//  A ContainerPath opens a .gz only when a step follows it, so a path that
//  ends in a gzip file has a gzip step added for the image inside it.
//
////////////////////////////////////////////////////////////////////////////////

ContainerPath WrappedImageFileIo::ParseWrapped (const std::string & path)
{
    ContainerPath  parsed = ContainerPath::Parse (path);
    std::string    last   = parsed.steps.back().name;



    std::transform (last.begin(), last.end(), last.begin(), [] (char c) { return (char) std::tolower ((unsigned char) c); });

    if (parsed.steps.back().kind != ContainerStep::Kind::Gzip && last.ends_with (".gz"))
    {
        parsed.steps.push_back ({ ContainerStep::Kind::Gzip, std::string() });
    }

    return parsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WrappedImageFileIo::Load (std::wstring & outError)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  host;
    OpenedContainer    opened;
    std::string        error;



    hr = m_host.Stat (m_path.GetHost(), m_hostStamp);
    CHRF (hr, outError = L"The file can't be read.");

    hr = m_host.ReadAllBytes (m_path.GetHost(), host);
    CHRF (hr, outError = L"The file can't be read.");

    hr = ContainerPath::OpenBytes (m_path, host, opened, error);
    CHRF (hr, outError = TextEncoding::Utf8ToWide (error.empty() ? std::string ("The image in the file can't be opened.") : error));

    m_bytes      = std::move (opened.bytes);
    m_innerName  = opened.name;
    m_loadedUnix = m_hostStamp.modifiedUnix;
    m_isChanged  = false;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::ReadAllBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WrappedImageFileIo::ReadAllBytes (const std::string & path, std::vector<Byte> & outBytes)
{
    (void) path;

    outBytes = m_bytes;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::WriteAllBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WrappedImageFileIo::WriteAllBytes (const std::string & path, const std::vector<Byte> & bytes)
{
    (void) path;

    m_bytes     = bytes;
    m_isChanged = true;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::WriteAt
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WrappedImageFileIo::WriteAt (const std::string & path, uint64_t offset, std::span<const Byte> bytes)
{
    HRESULT  hr     = S_OK;
    bool     isFit  = offset <= m_bytes.size() && bytes.size() <= m_bytes.size() - offset;



    (void) path;

    CBREx (isFit, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    std::copy (bytes.begin(), bytes.end(), m_bytes.begin() + (ptrdiff_t) offset);
    m_isChanged = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::ReadAt
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WrappedImageFileIo::ReadAt (const std::string & path, uint64_t offset, std::span<Byte> outBytes)
{
    HRESULT  hr    = S_OK;
    bool     isFit = offset <= m_bytes.size() && outBytes.size() <= m_bytes.size() - offset;



    (void) path;

    CBREx (isFit, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    std::copy (m_bytes.begin() + (ptrdiff_t) offset, m_bytes.begin() + (ptrdiff_t) (offset + outBytes.size()), outBytes.begin());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::Stat
//
//  The image's size, and the host file's time at Load while the host file
//  is as this object last read or wrote it, or its time now when something
//  else changed it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WrappedImageFileIo::Stat (const std::string & path, FileStamp & outStamp)
{
    HRESULT    hr      = S_OK;
    FileStamp  current;
    bool       isSame  = false;



    (void) path;

    hr = m_host.Stat (m_path.GetHost(), current);
    CHR (hr);

    isSame = current.sizeBytes == m_hostStamp.sizeBytes && current.modifiedUnix == m_hostStamp.modifiedUnix;

    outStamp.sizeBytes    = m_bytes.size();
    outStamp.modifiedUnix = isSame ? m_loadedUnix : current.modifiedUnix + 1;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::FlushToStorage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WrappedImageFileIo::FlushToStorage (const std::string & path)
{
    HRESULT               hr      = S_OK;
    std::vector<Byte>     rebuilt;
    std::wstring          error;
    CommitPlan::Progress  progress;



    (void) path;

    BAIL_OUT_IF (!m_isChanged, S_OK);

    hr = RebuildHost (m_bytes, rebuilt, error);
    CHR (hr);

    hr = DurableCommit::Commit (m_host, m_path.GetHost(), rebuilt, CommitPlan::NextInvocationTag(), CommitMode::Replace, progress);
    CHR (hr);

    hr = m_host.Stat (m_path.GetHost(), m_hostStamp);
    CHR (hr);

    m_isChanged = false;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo::RebuildHost
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WrappedImageFileIo::RebuildHost (std::span<const Byte> image, std::vector<Byte> & outHostBytes, std::wstring & outError)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  host;
    std::string        error;



    hr = m_host.ReadAllBytes (m_path.GetHost(), host);
    CHRF (hr, outError = L"The file can't be read.");

    hr = ContainerPath::ReplaceBytes (m_path, host, image, outHostBytes, error);
    CHRF (hr, outError = TextEncoding::Utf8ToWide (error.empty() ? std::string ("The file can't be rebuilt around the image.") : error));

Error:
    return hr;
}
