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

    //  A Mockingboard is left out. No title requires one, and a title that
    //  finds one can install an interrupt handler in the language card,
    //  which the enhanced //e's firmware switches to ROM before it jumps to
    //  the handler, as One-on-One does.
    for (SlotConfig & slot : config.slots)
    {
        if (slot.device.starts_with ("mockingboard"))
        {
            slot.enabled = false;
        }
    }

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



    if (model.starts_with ("Apple //c Plus"))
    {
        id = "Apple2cPlus";
    }
    else if (model.starts_with ("Apple IIgs"))
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
//  BootCheck::GetMachineName
//
////////////////////////////////////////////////////////////////////////////////

std::string BootCheck::GetMachineName (const std::string & machineId)
{
    return machineId == "Apple2cPlus" ? "Apple //c Plus"
         : machineId == "Apple2c"     ? "Apple //c"
         : machineId == kEnhanced     ? "enhanced Apple //e"
         : machineId == "Apple2e"     ? "Apple //e"
         : machineId == kPlus         ? "Apple ][ plus"
         : machineId == "Apple2"      ? "Apple ]["
                                      : machineId;
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
    struct Attempt
    {
        std::string        id;
        Built              built;
        BootReport         report;
        HRESULT            hrProbe = E_FAIL;
        std::atomic<bool>  stop    { false };
    };

    HRESULT                   hr     = S_OK;
    bool                      hasAny = false;
    bool                      isDone = false;
    std::deque<Attempt>       attempts;
    std::vector<std::thread>  threads;



    outReport = BootReport();
    outMachine.clear();
    outMissing.clear();

    //  The machines are built on this thread, and each boots on another.
    for (const std::string & id : GetMachines (capturedFrom))
    {
        Built        built;
        std::string  error;
        HRESULT      hrBuild = build (id, built, error);

        if (FAILED (hrBuild) || built.host == nullptr || built.builder == nullptr)
        {
            outMissing.push_back (error.empty() ? id : error);
            continue;
        }

        attempts.emplace_back();
        attempts.back().id    = id;
        attempts.back().built = built;
    }

    for (size_t at = 0; at < attempts.size(); at++)
    {
        threads.emplace_back ([&attempts, &image, &limits, format, at] ()
        {
            Attempt  & attempt = attempts[at];
            bool       isLast  = false;

            SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

            attempt.hrProbe = BootProbe::Probe (*attempt.built.host, *attempt.built.builder, image, format, limits, attempt.report, &attempt.stop);
            isLast          = SUCCEEDED (attempt.hrProbe) && (attempt.report.verdict == BootVerdict::Booted || attempt.report.verdict == BootVerdict::NotBootable);

            for (size_t after = at + 1; isLast && after < attempts.size(); after++)
            {
                attempts[after].stop = true;
            }
        });
    }

    for (std::thread & thread : threads)
    {
        thread.join();
    }

    for (Attempt & attempt : attempts)
    {
        if (isDone || FAILED (attempt.hrProbe))
        {
            continue;
        }

        if (!hasAny || IsBetter (attempt.report.verdict, outReport.verdict))
        {
            outReport  = std::move (attempt.report);
            outMachine = attempt.id;
        }

        hasAny = true;
        isDone = outReport.verdict == BootVerdict::Booted || outReport.verdict == BootVerdict::NotBootable;
    }

    CBREx (hasAny, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

Error:
    return hr;
}
