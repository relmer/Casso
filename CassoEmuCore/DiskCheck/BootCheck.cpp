#include "Pch.h"

#include "DiskCheck/BootCheck.h"
#include "Config/Win32FileSystem.h"
#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"
#include "Core/Prng.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootMachine::BootMachine
//
////////////////////////////////////////////////////////////////////////////////

BootMachine::BootMachine() :
    m_builder (*this, m_nothingListening)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootMachine::Build
//
//  The machine's JSON and ROMs from where Casso finds them. The user's
//  changes to the machine, such as a card added to a slot, aren't applied:
//  the boot is of the machine as shipped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BootMachine::Build (const std::string & machineId, std::string & outError)
{
    HRESULT                hr          = S_OK;
    Win32FileSystem        files;
    std::vector<fs::path>  searchPaths = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(), PathResolver::GetWorkingDirectory());
    fs::path               configPath  = PathResolver::FindFile (searchPaths, fs::path ("Machines") / machineId / (machineId + ".json"));
    bool                   hasConfig   = !configPath.empty();
    std::string            jsonText;
    MachineConfig          config;



    outError = hasConfig ? std::string() : std::format ("The machine {} isn't installed.", machineId);

    CBREx (hasConfig, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = files.ReadAllText (configPath.wstring(), jsonText);
    CHR (hr);

    hr = MachineConfigLoader::Load (jsonText, machineId, searchPaths, config, outError);
    CHR (hr);

    SetPrng (std::make_unique<Prng> (s_kSeed));
    SetCurrentMachineName (TextEncoding::NarrowToWide (machineId));

    GetConfig() = config;

    hr = m_builder.Build (config);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootCheck::BuildInstalled
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BootCheck::BuildInstalled (const std::string & machineId, Built & outBuilt, std::string & outError)
{
    HRESULT                       hr      = S_OK;
    std::shared_ptr<BootMachine>  machine = std::make_shared<BootMachine>();



    hr = machine->Build (machineId, outError);
    CHR (hr);

    outBuilt.builder = &machine->GetBuilder();
    outBuilt.host    = machine;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootCheck::GetMachineId
//
////////////////////////////////////////////////////////////////////////////////

std::string BootCheck::GetMachineId (const std::string & model)
{
    std::string  id;



    if (model.starts_with ("Apple //c Plus") || model.starts_with ("Apple IIgs"))
    {
        id.clear();
    }
    else if (model.starts_with ("Apple //c"))
    {
        id = "Apple2c";
    }
    else if (model.starts_with ("Apple //e enhanced"))
    {
        id = kEnhanced;
    }
    else if (model.starts_with ("Apple //e"))
    {
        id = "Apple2e";
    }
    else if (model.starts_with ("Apple ][+"))
    {
        id = kPlus;
    }
    else if (model.starts_with ("Apple ]["))
    {
        id = "Apple2";
    }

    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootCheck::GetMachines
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> BootCheck::GetMachines (const std::string & capturedFrom)
{
    std::vector<std::string>  machines;
    std::string               first = GetMachineId (capturedFrom);



    for (const std::string & id : { first, std::string (kEnhanced), std::string (kPlus) })
    {
        if (!id.empty() && std::find (machines.begin(), machines.end(), id) == machines.end())
        {
            machines.push_back (id);
        }
    }

    return machines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootCheck::IsBetter
//
////////////////////////////////////////////////////////////////////////////////

bool BootCheck::IsBetter (BootVerdict verdict, BootVerdict than)
{
    auto  rank = [] (BootVerdict each)
    {
        switch (each)
        {
            case BootVerdict::Booted:      return 3;
            case BootVerdict::NotBootable: return 2;
            case BootVerdict::Unclear:     return 1;
            default:                       return 0;
        }
    };



    return rank (verdict) > rank (than);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootCheck::Run
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BootCheck::Run (const std::vector<Byte> & image, DiskFormat format, const std::string & capturedFrom, const BootProbe::Limits & limits,
                        const MachineFn & build, BootReport & outReport, std::string & outMachine, std::vector<std::string> & outMissing)
{
    HRESULT  hr     = S_OK;
    bool     hasAny = false;
    bool     isDone = false;



    outReport = BootReport();
    outMachine.clear();
    outMissing.clear();

    for (const std::string & id : GetMachines (capturedFrom))
    {
        Built        built;
        BootReport   report;
        std::string  error;
        HRESULT      hrBuild = isDone ? E_ABORT : build (id, built, error);
        HRESULT      hrProbe = E_FAIL;

        if (isDone)
        {
            continue;
        }

        if (FAILED (hrBuild) || built.host == nullptr || built.builder == nullptr)
        {
            outMissing.push_back (error.empty() ? id : error);
            continue;
        }

        hrProbe = BootProbe::Probe (*built.host, *built.builder, image, format, limits, report);

        if (FAILED (hrProbe))
        {
            continue;
        }

        if (!hasAny || IsBetter (report.verdict, outReport.verdict))
        {
            outReport  = std::move (report);
            outMachine = id;
        }

        hasAny = true;
        isDone = outReport.verdict == BootVerdict::Booted || outReport.verdict == BootVerdict::NotBootable;
    }

    CBREx (hasAny, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

Error:
    return hr;
}
