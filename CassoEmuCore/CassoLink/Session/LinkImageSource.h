#pragma once

#include "Pch.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  ILinkImageSource
//
//  The disk image a write sends to the Apple: a file's bytes, or bytes made
//  in memory. The name is the file name, with the extension that indicates
//  the image's format; the path is empty for an image in memory.
//
////////////////////////////////////////////////////////////////////////////////

class ILinkImageSource
{
public:
    virtual ~ILinkImageSource () = default;

    virtual HRESULT      ReadImage (std::vector<Byte> & outBytes) = 0;
    virtual std::string  GetName   () const = 0;
    virtual std::string  GetPath   () const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FileImageSource
//
//  An image file, read when the write starts.
//
////////////////////////////////////////////////////////////////////////////////

class FileImageSource : public ILinkImageSource
{
public:
    FileImageSource (IDiskFileIo & io, std::string path) : m_io (io), m_path (std::move (path)) {}

    HRESULT      ReadImage (std::vector<Byte> & outBytes) override;
    std::string  GetName   () const override;
    std::string  GetPath   () const override { return m_path; }

private:
    IDiskFileIo  & m_io;
    std::string    m_path;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryImageSource
//
//  An image made in memory, such as a boot disk built for the write.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryImageSource : public ILinkImageSource
{
public:
    MemoryImageSource (std::string name, std::vector<Byte> bytes) : m_name (std::move (name)), m_bytes (std::move (bytes)) {}

    HRESULT      ReadImage (std::vector<Byte> & outBytes) override { outBytes = m_bytes; return S_OK; }
    std::string  GetName   () const override                      { return m_name; }
    std::string  GetPath   () const override                      { return std::string(); }

private:
    std::string        m_name;
    std::vector<Byte>  m_bytes;
};
