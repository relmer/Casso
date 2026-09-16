#include "Pch.h"

#include "Shell/HeadlessMachineFactory.h"

#include "Core/MachineConfig.h"
#include "Core/Prng.h"
#include "Machines/MachineDefinitions.h"
#include "Machines/Apple2/Common/VideoTiming.h"
#include "Shell/IRomSource.h"
#include "Shell/MachineBuilder.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessMachineFactory::Build
//
//  Built from a config of its own, and the host keeps a copy of what it was
//  built from. Handing the builder the host's own config makes it walk a list
//  the build is entitled to rewrite as it goes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessMachineFactory::Build (
    MachineHost        & host,
    MachineBuilder     & builder,
    const IRomSource   & source,
    const std::string  & machineId,
    Slots                slots,
    uint64_t             seed,
    std::string        & error)
{
    HRESULT        hr        = S_OK;
    MachineConfig  config;
    std::string    shippedId = GetShippedId (machineId);
    std::wstring   wide (shippedId.begin(), shippedId.end());



    error.clear();

    hr = LoadConfig (source, shippedId, config, error);
    CHR (hr);

    ApplySlots (slots, config);

    //  The emulator gives every machine a video timing model before it is built,
    //  so $C019 and the //c mouse see the frame; a headless machine gets the same.
    host.SetPrng        (std::make_unique<Prng> (seed));
    host.SetVideoTiming (std::make_unique<VideoTiming>());
    host.SetCurrentMachineName (wide);
    host.GetConfig() = config;

    hr = builder.Build (config);
    CHRF (hr, error = std::format ("{} did not build.", shippedId));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessMachineFactory::LoadConfig
//
//  The shipped JSON, through the shipped loader, with ROMs resolved by the
//  source. That the loader takes a resolver at all is the point: where a ROM
//  lives is the host's business, not the configuration's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessMachineFactory::LoadConfig (
    const IRomSource   & source,
    const std::string  & machineId,
    MachineConfig      & config,
    std::string        & error)
{
    HRESULT      hr = S_OK;
    std::string  jsonText;



    hr = source.GetMachineJson (machineId, jsonText);
    CHRF (hr, error = std::format ("{} is not a machine Casso ships.", machineId));

    hr = MachineConfigLoader::Load (jsonText,
                                    machineId,
                                    source.GetSearchPaths(),
                                    [&source] (const std::vector<std::filesystem::path> &,
                                               const std::filesystem::path & romRelPath) -> std::filesystem::path
                                    {
                                        return source.ResolveRom (romRelPath);
                                    },
                                    config,
                                    error);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessMachineFactory::ApplySlots
//
////////////////////////////////////////////////////////////////////////////////

void HeadlessMachineFactory::ApplySlots (Slots slots, MachineConfig & config)
{
    static constexpr int  kDiskSlot = 6;



    if (slots == Slots::Empty)
    {
        config.slots.clear();
    }
    else if (slots == Slots::DiskOnly)
    {
        std::erase_if (config.slots,
                       [] (const SlotConfig & slot) { return slot.slot != kDiskSlot; });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessMachineFactory::GetShippedId
//
//  The shipped id a name matches when case is ignored, or the name unchanged
//  when it matches none. The id selects the machine's built-in definition by
//  exact match, so `apple2e` has to become `Apple2e` before the build: left as
//  typed, it finds no definition and the configuration is read as a
//  user-defined machine, which fails on fields a shipped JSON leaves out.
//
////////////////////////////////////////////////////////////////////////////////

std::string HeadlessMachineFactory::GetShippedId (const std::string & machineId)
{
    for (const std::string & id : MachineDefinitions::GetKnownIds())
    {
        if (_stricmp (id.c_str(), machineId.c_str()) == 0)
        {
            return id;
        }
    }

    return machineId;
}