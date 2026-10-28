#include "Pch.h"

#include "TestMachine.h"

#include "Core/Prng.h"
#include "Core/TextEncoding.h"
#include "EmbeddedMachineConfigs.h"

#include "EmbeddedMachineJson.h"
#include "FixtureProvider.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::TestMachine
//
////////////////////////////////////////////////////////////////////////////////

TestMachine::TestMachine (const std::string & machineId, Slots slots)
    : m_builder (*this, m_nothingListening)
{
    Construct (machineId, slots, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::TestMachine (customized)
//
//  The builder copies its services when it is constructed, so they are
//  customized up front through a first, throwaway config load.
//
////////////////////////////////////////////////////////////////////////////////

TestMachine::TestMachine (
    const std::string  & machineId,
    Slots                slots,
    const Customize    & customize)
    : m_nothingListening (MakeServices (machineId, customize)),
      m_builder          (*this, m_nothingListening)
{
    Construct (machineId, slots, customize);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::TestMachine (configure)
//
////////////////////////////////////////////////////////////////////////////////

TestMachine::TestMachine (
    const std::string                         & machineId,
    const std::function<void (MachineConfig &)> & configure)
    : m_builder (*this, m_nothingListening)
{
    MachineConfig  config;



    LoadConfig (machineId, config);
    configure  (config);
    BuildFrom  (machineId, config);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::TestMachine (video, configure)
//
////////////////////////////////////////////////////////////////////////////////

TestMachine::TestMachine (
    const std::string                         & machineId,
    Video                                       video,
    const std::function<void (MachineConfig &)> & configure)
    : m_builder (*this, m_nothingListening)
{
    MachineConfig  config;



    if (video == Video::Timed)
    {
        SetVideoTiming (std::make_unique<VideoTiming>());
    }

    LoadConfig (machineId, config);

    if (configure)
    {
        configure (config);
    }

    BuildFrom (machineId, config);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::MakeServices
//
////////////////////////////////////////////////////////////////////////////////

MachineBuildServices TestMachine::MakeServices (
    const std::string  & machineId,
    const Customize    & customize)
{
    MachineConfig         config;
    MachineBuildServices  services;



    if (customize)
    {
        LoadConfig (machineId, config);
        customize (config, services);
    }

    return services;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::Construct
//
////////////////////////////////////////////////////////////////////////////////

void TestMachine::Construct (
    const std::string  & machineId,
    Slots                slots,
    const Customize    & customize)
{
    //  Built FROM a config of its own, and the host keeps a copy of what
    //  it was built from. Handing the builder the host's own config makes
    //  it walk a list the build is entitled to rewrite as it goes.
    MachineConfig         config;
    MachineBuildServices  unused;



    LoadConfig (machineId, config);

    if (slots == Slots::Empty)
    {
        config.slots.clear();
    }
    else if (slots == Slots::DiskOnly)
    {
        std::erase_if (config.slots,
                       [] (const SlotConfig & slot) { return slot.slot != 6; });
    }

    if (customize)
    {
        customize (config, unused);
    }

    //  The emulator gives every machine a video timing model before it is
    //  built, so $C019 and the //c mouse see the frame; so does a test
    //  machine built this way, as HeadlessMachineFactory::Build does.
    SetVideoTiming (std::make_unique<VideoTiming>());

    BuildFrom (machineId, config);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::BuildFrom
//
////////////////////////////////////////////////////////////////////////////////

void TestMachine::BuildFrom (const std::string & machineId, const MachineConfig & config)
{
    std::wstring  wide = TextEncoding::NarrowToWide (GetShippedId (machineId));
    HRESULT       hr   = S_OK;



    SetPrng (std::make_unique<Prng> (kSeed));
    SetCurrentMachineName (wide);
    GetConfig() = config;

    hr = m_builder.Build (config);

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
    std::string      shippedId  = GetShippedId (machineId);
    int              resourceId = 0;
    std::string      jsonText;
    std::string      error;
    HRESULT          hr         = S_OK;



    for (const EmbeddedConfig & cfg : s_kEmbeddedConfigs)
    {
        if (cfg.machineName == shippedId)
        {
            resourceId = cfg.resourceId;
            break;
        }
    }

    Assert::AreNotEqual (0, resourceId,
        std::format (L"'{}' is not a machine Casso ships",
                     TextEncoding::NarrowToWide (machineId)).c_str());

    jsonText = EmbeddedMachineJson::Load (resourceId);

    auto  resolveFlat = [root] (const std::vector<fs::path> &,
                                const fs::path & romRelPath) -> fs::path
    {
        fs::path  candidate = root / romRelPath.filename();

        return (fs::exists (candidate) ? candidate : fs::path());
    };

    hr = MachineConfigLoader::Load (jsonText,
                                    shippedId,
                                    { root },
                                    resolveFlat,
                                    outConfig,
                                    error);

    AssertSucceeded (hr,
        std::format (L"{} config must load: {}",
                     TextEncoding::NarrowToWide (machineId),
                     TextEncoding::NarrowToWide (error)).c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::PlugInSerialLoopbacks
//
////////////////////////////////////////////////////////////////////////////////

void TestMachine::PlugInSerialLoopbacks (MachineConfig & config)
{
    for (PortConfig & port : config.ports)
    {
        if (port.name == kpszSerialPort1Name || port.name == kpszSerialPort2Name)
        {
            port.device = kpszSerialLoopback;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine::GetShippedId
//
//  The shipped id a name matches when case is ignored, or the name unchanged,
//  as HeadlessMachineFactory matches it.
//
////////////////////////////////////////////////////////////////////////////////

std::string TestMachine::GetShippedId (const std::string & machineId)
{
    for (const EmbeddedConfig & cfg : s_kEmbeddedConfigs)
    {
        std::string  name (cfg.machineName);



        if (_stricmp (name.c_str(), machineId.c_str()) == 0)
        {
            return name;
        }
    }

    return machineId;
}
