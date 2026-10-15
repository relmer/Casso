#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectScopeKind
//
//  The signals a debug project is chosen by, most specific first (FR-167).
//
////////////////////////////////////////////////////////////////////////////////

enum class ProjectScopeKind
{
    DebugFile,
    Binary,
    MachineBoot,
    Machine,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectScope
//
//  The program a debug project is for: a debug or symbol file by its
//  content's SHA-1, so a moved file finds its project; a binary BLOAD loaded,
//  by its content's SHA-1 and address; the machine with the disk in drive 1
//  at power-on, by the machine and the disk's path, since a disk changes as
//  the guest writes it; or the machine alone. The label is how PROJECT
//  writes the scope.
//
////////////////////////////////////////////////////////////////////////////////

struct ProjectScope
{
    ProjectScopeKind  kind  = ProjectScopeKind::Machine;
    std::string       key;
    std::string       label;

    //  The most specific signal there is: a debug file's key, else a
    //  binary's, else the machine with its boot disk, else the machine.
    static ProjectScope  Choose (const std::string  & debugFileKey,
                                 const std::string  & debugFileName,
                                 const std::string  & binaryKey,
                                 const std::string  & binaryName,
                                 const std::string  & machine,
                                 const std::wstring & bootDisk);

    //  True when this scope's signal ranks above the other's.
    bool                 IsMoreSpecificThan (const ProjectScope & other) const { return kind < other.kind; }

    //  The automatic project's file name: the kind, then the first sixteen
    //  hex digits of the SHA-1 of the key, a name valid on any file system.
    std::wstring         GetFileName () const;

    static const char  * GetKindName (ProjectScopeKind kind);
    static bool          TryParseKind (const std::string & name, ProjectScopeKind & kind);

    bool operator== (const ProjectScope & other) const { return kind == other.kind && key == other.key; }
};
