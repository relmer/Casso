#include "Pch.h"

#include "Cli/DebugBatchRunner.h"

#include "Config/IFileSystem.h"
#include "Core/TextEncoding.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugSession.h"
#include "Debugger/ReplyJson.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Shell/HeadlessMachineFactory.h"
#include "Shell/IRomSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner::DebugBatchRunner
//
////////////////////////////////////////////////////////////////////////////////

DebugBatchRunner::DebugBatchRunner (const IRomSource & roms, IFileSystem & files) :
    m_roms  (roms),
    m_files (files)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner::Run
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugBatchRunner::Run (const CommandLineOptions::DebugOptions & options, const std::string & scriptText, DebugBatchResult & result)
{
    HRESULT  hr = S_OK;



    hr = Prepare (options, result);
    CHR (hr);

    Execute (options, scriptText, result);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner::Prepare
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugBatchRunner::Prepare (const CommandLineOptions::DebugOptions & options, DebugBatchResult & result)
{
    HRESULT  hr = S_OK;



    result = DebugBatchResult();

    hr = BuildMachine (options, result);
    CHR (hr);

    RouteDiskIo (options.writeDisks);

    if (!options.disk1.empty())
    {
        hr = MountDisk (options.disk1, 0, result);
        CHR (hr);
    }

    if (!options.disk2.empty())
    {
        hr = MountDisk (options.disk2, 1, result);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner::Execute
//
//  One session over the machine with every command family, the option's
//  budget and its starting mode, then the lines. A command that returns
//  error or unknown sets status 1; a last run that ended on its budget sets
//  3 unless 1 is already set.
//
////////////////////////////////////////////////////////////////////////////////

void DebugBatchRunner::Execute (const CommandLineOptions::DebugOptions & options, const std::string & scriptText, DebugBatchResult & result)
{
    HRESULT                   hr     = S_OK;
    std::vector<std::string>  lines;
    DebugCommand              budget;
    DebugCommand              mode;
    OutputFormat              output = OutputFormat::AppleWin;
    DebugSession              session (*m_target, m_sink, RunState::Paused);



    m_sink.SetJson    (options.json);
    m_sink.SetSession (&session);
    m_handlers.Attach (session);
    session.SetFileSystem (&m_files);

    budget.verb       = DebugVerb::SetBudget;
    budget.sourceName = "BUDGET";
    budget.count      = (uint32_t) std::min<uint64_t> (options.maxCycles, UINT32_MAX);
    session.Execute (budget);

    if (CommandModeNames::TryParse (options.mode, mode.mode) && mode.mode != CommandMode::AppleWin)
    {
        mode.verb       = DebugVerb::SetMode;
        mode.sourceName = "MODE";
        session.Execute (mode);
        m_sink.TakePending();
    }

    if (CommandModeNames::TryParse (options.output, output))
    {
        session.SetOutputFormat (output);
    }

    SplitLines (scriptText, lines);
    lines.insert (lines.end(), options.commands.begin(), options.commands.end());

    RunLines (options, lines, session, result);

    if (options.writeDisks)
    {
        hr = m_host->GetDiskStore().FlushAll();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_sink.SetSession (nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner::BuildMachine
//
//  A machine with a disk card only when a disk is given; without one the
//  autostart ROM reaches BASIC instead of spinning an empty drive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugBatchRunner::BuildMachine (const CommandLineOptions::DebugOptions & options, DebugBatchResult & result)
{
    HRESULT                         hr       = S_OK;
    bool                            hasDisk  = !options.disk1.empty() || !options.disk2.empty();
    HeadlessMachineFactory::Slots   slots    = hasDisk ? HeadlessMachineFactory::Slots::DiskOnly : HeadlessMachineFactory::Slots::Empty;
    std::string                     error;



    m_host    = std::make_unique<MachineHost>();
    m_builder = std::make_unique<MachineBuilder> (*m_host, m_services);

    hr = HeadlessMachineFactory::Build (*m_host, *m_builder, m_roms, options.machine, slots, options.seed, error);

    if (FAILED (hr))
    {
        result.diagnostics += "Error: " + error + "\n";
        result.exitStatus   = kNothingStarted;
    }

    CHR (hr);

    m_host->PowerCycle();

    m_target = std::make_unique<MachineDebugTarget> (*m_host);
    m_driver = std::make_unique<SynchronousRunDriver> (*m_host, m_target->GetRunHook());
    m_target->SetRunDriver (m_driver.get());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner::RouteDiskIo
//
//  Every byte the disk store reads or writes goes through the file system
//  seam, and its identity check is left unrecorded: a batch run holds the
//  file for seconds, and the seam has nothing to stat.
//
////////////////////////////////////////////////////////////////////////////////

void DebugBatchRunner::RouteDiskIo (bool writeThrough)
{
    DiskImageStore & store = m_host->GetDiskStore();



    store.SetImageReader ([this] (const std::string & path, std::vector<Byte> & bytes)
    {
        std::string  content;
        HRESULT      hr = m_files.ReadAllText (TextEncoding::NarrowToWide (path), content);

        bytes.assign (content.begin(), content.end());
        return hr;
    });

    store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });

    if (writeThrough)
    {
        store.SetFlushSink ([this] (const std::string & path, const std::vector<Byte> & bytes)
        {
            return m_files.WriteAllText (TextEncoding::NarrowToWide (path), std::string (bytes.begin(), bytes.end()));
        });
    }
    else
    {
        store.SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner::MountDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugBatchRunner::MountDisk (const std::string & path, int drive, DebugBatchResult & result)
{
    HRESULT         hr = S_OK;
    MountDiagnosis  diagnosis;



    hr = m_host->GetDiskStore().Mount (kDiskSlot, drive, path, diagnosis);

    if (FAILED (hr))
    {
        switch (diagnosis.failure)
        {
        case MountFailure::FileUnreadable:
            result.diagnostics += std::format ("Error: {} could not be read.\n", path);
            break;

        case MountFailure::UnknownExtension:
            result.diagnostics += std::format ("Error: {} is not a disk image Casso reads.\n", path);
            break;

        default:
            result.diagnostics += std::format ("Error: {} could not be mounted in drive {}.\n", path, drive + 1);
            break;
        }

        result.exitStatus = kNothingStarted;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner::RunLines
//
//  Text mode echoes each line behind the mode's prompt, then its reply's
//  text, then any notification the command caused. JSON mode prints the
//  reply record, then the notification records. A blank line reaches the
//  session only while the line assembler is active, where it ends the
//  assembly; a ; line is a comment in both modes.
//
////////////////////////////////////////////////////////////////////////////////

void DebugBatchRunner::RunLines (
    const CommandLineOptions::DebugOptions & options,
    const std::vector<std::string>         & lines,
    DebugSession                           & session,
    DebugBatchResult                       & result)
{
    bool  anyFailed = false;



    for (const std::string & raw : lines)
    {
        std::string  line = Trim (raw);
        Reply        reply;



        if (line.empty() && !session.IsAssembling())
        {
            continue;
        }

        if (line.starts_with (';'))
        {
            continue;
        }

        if (!options.json)
        {
            result.output += (session.GetMode() == CommandMode::Monitor ? "*" : ">") + line + "\n";
        }

        reply = session.ExecuteLine (line);
        session.FormatReply (reply);

        if (options.json)
        {
            result.output += ReplyJson::WriteReply (reply, std::nullopt) + "\n";
        }
        else
        {
            for (const std::string & text : reply.text)
            {
                result.output += text + "\n";
            }
        }

        result.output += m_sink.TakePending();
        anyFailed     |= reply.status == CommandStatus::Error || reply.status == CommandStatus::Unknown;
    }

    if (anyFailed)
    {
        result.exitStatus = kCommandFailed;
    }
    else if (m_sink.GetLastStopReason() == StopReason::Budget)
    {
        result.exitStatus = kBudgetStop;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner::SplitLines
//
////////////////////////////////////////////////////////////////////////////////

void DebugBatchRunner::SplitLines (const std::string & text, std::vector<std::string> & lines)
{
    size_t  start = 0;



    while (start < text.size())
    {
        size_t  end = text.find ('\n', start);



        lines.push_back (text.substr (start, end == std::string::npos ? std::string::npos : end - start));

        if (end == std::string::npos)
        {
            break;
        }

        start = end + 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugBatchRunner::Trim
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugBatchRunner::Trim (const std::string & text)
{
    size_t  first = text.find_first_not_of (" \t\r");
    size_t  last  = text.find_last_not_of  (" \t\r");



    return (first == std::string::npos) ? std::string() : text.substr (first, last - first + 1);
}
