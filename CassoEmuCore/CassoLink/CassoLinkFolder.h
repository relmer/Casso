#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoExplorer/Model/Location.h"

class IDiskFileIo;
class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder
//
//  The host folder CassoLinkHost serves an ADTPro-compatible server
//  connection from. With none set, each of ADTPro's requests has its unable
//  reply.
//
//  The folder set is the root: ADTPro's CD moves within it, and a request
//  for a file or folder outside its subtree, by "..", a separator, or a
//  junction or symbolic link, fails. Paths are compared once every
//  junction in them is followed.
//
//  The folder is set on the UI thread while the host reads it on the link's
//  thread, so both go through a lock. A write goes through DurableCommit, so
//  an interrupted transfer ends with the old file in place, or with none.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkFolder : public ICassoLinkFolder
{
public:
    CassoLinkFolder (IFileSystem & fileSystem, IDiskFileIo & fileIo);

    static std::wstring  GetFolderFor (const Location & location);

    void         SetLocation     (const Location & location);

    //  The folder served, as the ADTPro setting has it; empty serves none.
    void         SetFolder       (const std::wstring & folder);

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
    bool          IsConfined  (const std::wstring & path) const;

    //  A path as IsConfined compares it: lower case, backslashes, and no
    //  trailing separator.
    static std::wstring  MakeComparable (const std::wstring & path);

    IFileSystem         & m_fileSystem;
    IDiskFileIo         & m_fileIo;
    mutable std::mutex    m_lock;
    std::wstring          m_folder;
    std::wstring          m_root;
    std::atomic<uint64_t> m_writeTag { 1 };
};
