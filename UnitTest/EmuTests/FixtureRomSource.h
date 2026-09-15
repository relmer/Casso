#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "Shell/IRomSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureRomSource
//
//  The configuration JSON a shipped Casso.exe carries, and the ROMs in
//  UnitTest/Fixtures. Fixtures hold the ROMs flat while the product keeps
//  each machine's assets in its own directory, so ROMs are resolved on the
//  file name alone.
//
////////////////////////////////////////////////////////////////////////////////

class FixtureRomSource : public IRomSource
{
public:
    FixtureRomSource();

    HRESULT                             GetMachineJson (const std::string & machineId, std::string & jsonText) const override;
    std::filesystem::path               ResolveRom     (const std::filesystem::path & romRelPath) const override;
    std::vector<std::filesystem::path>  GetSearchPaths () const override;

private:
    std::filesystem::path  m_root;
};
