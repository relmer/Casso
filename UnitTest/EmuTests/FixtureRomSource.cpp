#include "Pch.h"

#include "FixtureRomSource.h"

#include "EmbeddedMachineConfigs.h"
#include "EmbeddedMachineJson.h"
#include "FixtureProvider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureRomSource::FixtureRomSource
//
////////////////////////////////////////////////////////////////////////////////

FixtureRomSource::FixtureRomSource() :
    m_root (FixtureProvider().GetRoot())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureRomSource::GetMachineJson
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FixtureRomSource::GetMachineJson (const std::string & machineId, std::string & jsonText) const
{
    HRESULT  hr         = S_OK;
    int      resourceId = 0;



    jsonText.clear();

    for (const EmbeddedConfig & cfg : s_kEmbeddedConfigs)
    {
        if (cfg.machineName == machineId)
        {
            resourceId = cfg.resourceId;
            break;
        }
    }

    CBREx (resourceId != 0, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    jsonText = EmbeddedMachineJson::Load (resourceId);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureRomSource::ResolveRom
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path FixtureRomSource::ResolveRom (const std::filesystem::path & romRelPath) const
{
    std::filesystem::path  candidate = m_root / romRelPath.filename();



    return std::filesystem::exists (candidate) ? candidate : std::filesystem::path();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureRomSource::GetSearchPaths
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::filesystem::path> FixtureRomSource::GetSearchPaths() const
{
    return { m_root };
}
