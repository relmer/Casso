#include "Pch.h"

#include "CassoExplorer/Window/PreviewHexSources.h"
#include "CassoExplorer/Browser/BrowserPreviewer.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewFileBytes::Open
//
//  Shared for reading, writing and deletion, so previewing a file never gets
//  in the way of another program using it.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewFileBytes::Open (const std::wstring & path)
{
    LARGE_INTEGER  size = {};
    HRESULT        hr   = S_OK;



    Close();

    m_file = CreateFileW (path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    m_error = (m_file == INVALID_HANDLE_VALUE) ? HRESULT_FROM_WIN32 (GetLastError()) : S_OK;

    if (m_file != INVALID_HANDLE_VALUE)
    {
        hr      = Win32DiskFileIo::HydrateIfPlaceholder (m_file);
        m_error = SUCCEEDED (hr) && !GetFileSizeEx (m_file, &size) ? HRESULT_FROM_WIN32 (GetLastError()) : hr;
    }

    if (FAILED (m_error))
    {
        Close();
        return false;
    }

    m_path = path;
    m_size = (uint64_t) size.QuadPart;

    if (!GetFileTime (m_file, nullptr, nullptr, &m_written))
    {
        m_written = {};
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewFileBytes::IsCurrent
//
//  By the path, not the open handle: a save that writes a new file and
//  renames it over this one: the handle is then on the old file.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewFileBytes::IsCurrent() const
{
    WIN32_FILE_ATTRIBUTE_DATA  data  = {};
    bool                       found = !m_path.empty() && GetFileAttributesExW (m_path.c_str(), GetFileExInfoStandard, &data);
    uint64_t                   size  = ((uint64_t) data.nFileSizeHigh << 32) | data.nFileSizeLow;



    return found && size == m_size && CompareFileTime (&data.ftLastWriteTime, &m_written) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewFileBytes::Close
//
////////////////////////////////////////////////////////////////////////////////

void PreviewFileBytes::Close()
{
    if (m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle (m_file);
    }

    m_file       = INVALID_HANDLE_VALUE;
    m_size       = 0;
    m_windowBase = 0;

    m_path.clear();
    m_window.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewFileBytes::GetCharset
//
////////////////////////////////////////////////////////////////////////////////

HostCharset PreviewFileBytes::GetCharset() const
{
    std::vector<uint8_t>  sample ((size_t) (std::min) ((uint64_t) kTextSampleBytes, m_size));



    if (sample.empty())
    {
        return HostCharset::None;
    }

    ReadAt (0, sample);

    return BrowserPreviewer::GetHostCharset (sample);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewFileBytes::ReadBytes
//
//  The window starts a little before the bytes asked for, so scrolling back
//  a few rows does not move it again straight away.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewFileBytes::ReadBytes (uint64_t offset, std::span<uint8_t> out) const
{
    uint64_t  end = offset + out.size();



    if (out.size() > kWindowBytes / 2)
    {
        ReadAt (offset, out);
        return;
    }

    if (m_window.empty() || offset < m_windowBase || end > m_windowBase + m_window.size())
    {
        m_windowBase = (offset > kWindowBytes / 4) ? (offset - kWindowBytes / 4) : 0;
        m_window.resize ((size_t) (std::min) ((uint64_t) kWindowBytes, m_size - (std::min) (m_windowBase, m_size)));

        ReadAt (m_windowBase, m_window);
    }

    if (end > m_windowBase + m_window.size())
    {
        ReadAt (offset, out);
        return;
    }

    std::copy_n (m_window.begin() + (ptrdiff_t) (offset - m_windowBase), out.size(), out.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewFileBytes::ReadAt
//
//  Bytes the file no longer has, because it shrank while shown, read as zero.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewFileBytes::ReadAt (uint64_t offset, std::span<uint8_t> out) const
{
    LARGE_INTEGER  position = {};
    DWORD          got      = 0;



    std::fill (out.begin(), out.end(), (uint8_t) 0);

    if (m_file == INVALID_HANDLE_VALUE || out.empty())
    {
        return;
    }

    position.QuadPart = (LONGLONG) offset;

    //  A failed seek or read leaves the zeros already there.
    if (SetFilePointerEx (m_file, position, nullptr, FILE_BEGIN) &&
        !ReadFile (m_file, out.data(), (DWORD) out.size(), &got, nullptr))
    {
        return;
    }
}
