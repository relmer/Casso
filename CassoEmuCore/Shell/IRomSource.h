#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IRomSource
//
//  Where a headless machine's configuration and ROM files come from. Tests
//  read the configuration embedded in Casso.exe and the ROMs in
//  UnitTest/Fixtures; the console tool reads them from the asset directory.
//
////////////////////////////////////////////////////////////////////////////////

class IRomSource
{
public:
    virtual ~IRomSource() = default;

    virtual HRESULT                             GetMachineJson (const std::string & machineId, std::string & jsonText) const = 0;
    virtual std::filesystem::path               ResolveRom     (const std::filesystem::path & romRelPath) const             = 0;
    virtual std::vector<std::filesystem::path>  GetSearchPaths () const                                                     = 0;
};
