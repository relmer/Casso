#pragma once

#include "Pch.h"

#include "Debugger/Project/DebugProject.h"

class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectStore
//
//  Debug project files over the file system (FR-168, FR-170). Each scope
//  has an automatic file in the store's folder; a project saved under a
//  name of the user's choosing lives where the user put it, and its scope's
//  automatic file then has only the path to it, so the scope opens it.
//
//  A save replaces the file whole, as IFileSystem::WriteAllText does: after a
//  crash the file has the old project or the new one. A file that is there but
//  is not a project is copied to the same name with ".unreadable-" and the
//  date, and an empty project opens in its place, with a notice.
//
//  The save waits for 2 seconds without a change: each change restarts the
//  wait, and IsSaveDue reports whether a save is due.
//
////////////////////////////////////////////////////////////////////////////////

class DebugProjectStore
{
public:
    static constexpr uint64_t          kSaveDelayMs = 2000;
    static constexpr const char      * kpszRedirect = "redirect";

    //  The date an unreadable file's copy has in its name, such as
    //  "20261015-013000"; the clock's own by default.
    using StampFn = std::function<std::string()>;

    DebugProjectStore (IFileSystem & files, std::wstring folder, StampFn stamp = {});

    std::wstring  GetAutomaticPath (const ProjectScope & scope) const;

    //  The scope's project: its automatic file, or the file that one
    //  points to, or an empty project for the scope.
    HRESULT       Open      (const ProjectScope & scope, DebugProject & project, std::vector<std::string> & notices);

    //  The project in a file the user chose, which becomes a chosen-file project.
    HRESULT       OpenFile  (const std::wstring & path, DebugProject & project, std::vector<std::string> & notices);

    //  Writes the project to its file; a chosen-file project's automatic
    //  file is pointed at it.
    HRESULT       Save      (const DebugProject & project);

    //  Writes a copy to path and makes that file the project's.
    HRESULT       SaveAs    (DebugProject & project, const std::wstring & path);

    void          MarkChanged  (uint64_t nowMs)       { m_changedAt = nowMs; }
    void          ClearChanged ()                     { m_changedAt.reset(); }
    bool          IsChanged    () const               { return m_changedAt.has_value(); }
    bool          IsSaveDue    (uint64_t nowMs) const { return m_changedAt.has_value() && nowMs - *m_changedAt >= kSaveDelayMs; }

private:
    HRESULT       Read          (const std::wstring & path, DebugProject & project, std::vector<std::string> & notices, bool & isFound);
    bool          TryReadRedirect (const std::wstring & path, std::string & redirect);
    std::string   GetStamp      () const;

    static std::wstring  GetFolder (const std::wstring & path);

    IFileSystem              & m_files;
    std::wstring               m_folder;
    StampFn                    m_stamp;
    std::optional<uint64_t>    m_changedAt;
};
