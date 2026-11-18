#include "Pch.h"

#include "DiskCheck/BootCheck.h"
#include "Config/Win32FileSystem.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"
#include "Core/Prng.h"
#include "Core/TextEncoding.h"
#include "Core/JsonValue.h"





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
//  Every card but the Disk II is dropped from the slots: a boot doesn't use one,
//  and a card this build can't make, such as one a newer build of Casso
//  added to the JSON, or one whose ROM isn't installed, would fail the whole
//  machine. A Mockingboard is left out too: a title that finds one can
//  install an interrupt handler in the language card, which the enhanced
//  //e's firmware switches to ROM before it jumps to the handler, as
//  One-on-One does.
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

    hr = KeepDiskSlots (jsonText);
    CHRF (hr, outError = std::format ("The machine {}'s configuration couldn't be read.", machineId));

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
//  BootMachine::KeepDiskSlots
//
//  The machine's JSON again, with only its Disk II cards in its slots.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BootMachine::KeepDiskSlots (std::string & ioJson)
{
    HRESULT                                         hr      = S_OK;
    JsonValue                                       root;
    JsonParseError                                  error;
    std::vector<std::pair<std::string, JsonValue>>  entries;



    hr = JsonParser::Parse (ioJson, root, error);
    CHR (hr);

    for (const auto & [key, value] : root.GetObjectEntries())
    {
        std::vector<JsonValue>  disks;
        std::string             device;

        if (key != "slots" || value.GetType() != JsonType::Array)
        {
            entries.emplace_back (key, value);
            continue;
        }

        for (size_t at = 0; at < value.GetArraySize(); at++)
        {
            if (value.GetArrayElement (at).HasString ("device", device) && device == "disk-ii")
            {
                disks.push_back (value.GetArrayElement (at));
            }
        }

        entries.emplace_back (key, JsonValue (std::move (disks)));
    }

    ioJson = JsonWriter::Write (JsonValue (std::move (entries)));

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
