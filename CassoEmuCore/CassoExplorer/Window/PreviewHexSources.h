#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/PreviewDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewBytes
//
//  The previewed file's bytes, as a source for the hex view. The browser
//  owns the preview content and replaces it on every selection change, so
//  this stores a pointer rather than a copy, updated each time the preview
//  is refilled.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewBytes : public IDxuiHexSource
{
public:
    void  SetBytes (const std::vector<Byte> * bytes) { m_bytes = bytes; }

    uint64_t  GetByteCount() const override
    {
        return (m_bytes != nullptr) ? (uint64_t) m_bytes->size() : 0;
    }

    void  ReadBytes (uint64_t offset, std::span<uint8_t> out) const override
    {
        size_t  start = (size_t) offset;

        for (size_t idx = 0; idx < out.size(); idx++)
        {
            out[idx] = (m_bytes != nullptr && (start + idx) < m_bytes->size())
                     ? (uint8_t) (*m_bytes)[start + idx]
                     : (uint8_t) 0;
        }
    }

private:
    const std::vector<Byte> *  m_bytes = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewFileBytes
//
//  A host file, as a source for the hex view. The view asks only for the
//  rows it draws, and those come from a 1 MB window of the file that moves
//  when a row falls outside it, so a file of any size costs about the
//  same. A read larger than half the window, as a search makes, goes to
//  the file directly.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewFileBytes : public IDxuiHexSource
{
public:
    ~PreviewFileBytes() override { Close(); }

    bool  Open  (const std::wstring & path);
    void  Close ();

    //  Which character set the start of the file is text in, if any.
    HostCharset  GetCharset () const;

    const std::wstring &  GetPath  () const { return m_path; }

    //  Whether the file at the path has the size and write time it had
    //  when opened; false once another program changed or replaced it.
    bool                  IsCurrent () const;

    //  Why the last Open failed, or S_OK.
    HRESULT               GetError () const { return m_error; }

    uint64_t  GetByteCount () const override { return m_size; }
    void      ReadBytes    (uint64_t offset, std::span<uint8_t> out) const override;

    static constexpr size_t  kWindowBytes     = 1024 * 1024;
    static constexpr size_t  kTextSampleBytes = 64 * 1024;

private:
    void  ReadAt (uint64_t offset, std::span<uint8_t> out) const;

    HANDLE                        m_file       = INVALID_HANDLE_VALUE;
    HRESULT                       m_error      = S_OK;
    std::wstring                  m_path;
    uint64_t                      m_size       = 0;
    FILETIME                      m_written    = {};
    mutable std::vector<uint8_t>  m_window;
    mutable uint64_t              m_windowBase = 0;
};
