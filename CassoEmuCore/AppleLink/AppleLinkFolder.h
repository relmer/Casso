#pragma once

#include "Pch.h"

#include "AppleLink/AppleLinkHost.h"
#include "CassoExplorer/Model/Location.h"

class IDiskFileIo;
class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkFolder
//
//  The host folder AppleLinkHost serves, following Casso Explorer's current
//  tab: a host folder is served as it is; a disk image, or a directory inside
//  one, serves the folder the image is in; a root such as This PC, the
//  Recycle Bin or a shell folder serves nothing, and each of ADTPro's
//  requests has its unable reply.
//
//  The tab sets the location on the UI thread while the host reads it on the
//  link's thread, so both go through a lock. ADTPro's CD changes the folder
//  the link serves, not the tab. A write goes through DurableCommit, so an
//  interrupted transfer ends with the old file in place, or with none.
//
////////////////////////////////////////////////////////////////////////////////

class AppleLinkFolder : public IAppleLinkFolder
{
public:
    AppleLinkFolder (IFileSystem & fileSystem, IDiskFileIo & fileIo);

    static std::wstring  GetFolderFor (const Location & location);

    void         SetLocation     (const Location & location);

    bool         IsOpen          () const override;
    std::string  GetPath         () const override;
    HRESULT      List            (std::vector<std::string> & outNames) override;
    bool         Exists          (const std::string & name) override;
    HRESULT      Read            (const std::string & name, std::vector<Byte> & outBytes) override;
    HRESULT      Write           (const std::string & name, const std::vector<Byte> & bytes) override;
    HRESULT      ChangeDirectory (const std::string & name) override;

private:
    std::wstring  GetFolder   () const;
    std::string   GetFilePath (const std::string & name) const;

    IFileSystem         & m_fileSystem;
    IDiskFileIo         & m_fileIo;
    mutable std::mutex    m_lock;
    std::wstring          m_folder;
    std::atomic<uint64_t> m_writeTag { 1 };
};
