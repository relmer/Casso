#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/Location.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerProperties
//
//  The Properties dialogs: a disk image's or an Apple file's, an AppleSingle
//  file's on the PC, a WOZ image's, a location's, and the shell's dialog for
//  host files.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerProperties
{
public:
    explicit ExplorerProperties (ExplorerContext & context);

    void         ShowRowProperties       (int row);
    void         ShowAppleProperties     ();
    void         ShowHostAppleProperties (const std::wstring & path);
    void         ShowWozProperties       (const std::wstring & path, int tab);
    void         ShowLocationProperties  (const Location & location);
    void         ShowHostProperties      (const std::wstring & path);
    void         ShowHostProperties      (const std::vector<std::wstring> & paths);
    static bool  IsHostAppleFile         (const std::wstring & path);
    static bool  IsWozImage              (const std::wstring & path);

private:
    ExplorerContext  & m_context;
};