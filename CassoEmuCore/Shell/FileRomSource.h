#pragma once

#include "Pch.h"

#include "Shell/IRomSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileRomSource
//
//  The console tool's machine configurations and ROMs: Machines/<id>/<id>.json
//  and the ROM files it names, found by searching the executable's directory,
//  the working directory and their parents, then the asset directory the
//  emulator downloads ROMs into. The emulator's own startup searches the same
//  places, so a machine that starts in Casso starts here.
//
////////////////////////////////////////////////////////////////////////////////

class FileRomSource : public IRomSource
{
public:
    FileRomSource();

    HRESULT                             GetMachineJson (const std::string & machineId, std::string & jsonText) const override;
    std::filesystem::path               ResolveRom     (const std::filesystem::path & romRelPath) const override;
    std::vector<std::filesystem::path>  GetSearchPaths () const override;

private:
    std::vector<std::filesystem::path>  m_searchPaths;
};
