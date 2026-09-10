#include "Pch.h"

#include "TestMachine.h"

#include "Core/Prng.h"
#include "EmbeddedMachineConfigs.h"

#include "EmbeddedMachineJson.h"
#include "FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::TestMachine
//
////////////////////////////////////////////////////////////////////////////////

TestMachine::TestMachine (const std::string & machineId)
    : m_builder (m_host, m_nothingListening)
{
    std::wstring  wide (machineId.begin(), machineId.end());
    HRESULT       hr = S_OK;



    LoadConfig (machineId, m_config);

    m_host.SetPrng (std::make_unique<Prng> (kSeed));
    m_host.SetCurrentMachineName (wide);
    m_host.GetConfig() = m_config;

    hr = m_builder.Build (m_config);

    AssertSucceeded (hr, std::format (L"{} must build", wide).c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::LoadConfig
//
//  The shipped JSON, through the shipped loader, with the ROMs resolved out
//  of UnitTest/Fixtures.
//
//  Fixtures hold the ROMs flat while the product keeps each machine's assets
//  in its own directory, so the resolver answers on the file name and
//  ignores the subdirectory. That the loader takes a resolver at all is the
//  point: where a ROM lives is the host's business, not the configuration's.
//
////////////////////////////////////////////////////////////////////////////////

void TestMachine::LoadConfig (const std::string & machineId, MachineConfig & outConfig)
{
    FixtureProvider  fixtures;
    fs::path         root       = fs::path (fixtures.GetRoot());
    int              resourceId = 0;
    std::string      jsonText;
    std::string      error;
    HRESULT          hr         = S_OK;



    for (const EmbeddedConfig & cfg : s_kEmbeddedConfigs)
    {
        if (cfg.machineName == machineId)
        {
            resourceId = cfg.resourceId;
            break;
        }
    }

    Assert::AreNotEqual (0, resourceId,
        std::format (L"'{}' is not a machine Casso ships",
                     std::wstring (machineId.begin(), machineId.end())).c_str());

    jsonText = EmbeddedMachineJson::Load (resourceId);

    auto  resolveFlat = [root] (const std::vector<fs::path> &,
                                const fs::path & romRelPath) -> fs::path
    {
        fs::path  candidate = root / romRelPath.filename();

        return (fs::exists (candidate) ? candidate : fs::path());
    };

    hr = MachineConfigLoader::Load (jsonText,
                                    machineId,
                                    { root },
                                    resolveFlat,
                                    outConfig,
                                    error);

    AssertSucceeded (hr,
        std::format (L"{} config must load: {}",
                     std::wstring (machineId.begin(), machineId.end()),
                     std::wstring (error.begin(), error.end())).c_str());
}
