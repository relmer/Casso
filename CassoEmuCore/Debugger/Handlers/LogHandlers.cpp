#include "Pch.h"

#include "Debugger/Handlers/LogHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/DebugSession.h"
#include "Machines/Apple2/Common/VideoTiming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool LogHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::VideoLog: Execute (session, command, m_video, true,  reply); return true;
    case DebugVerb::SoundLog: Execute (session, command, m_sound, false, reply); return true;
    default:                                                                    return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::Execute
//
//  ON|OFF|CLEAR|LIST [n]|SAVE [file]. A bare command lists. LIST shows the
//  last n entries, 100 unless given; SAVE writes every entry the log holds.
//
////////////////////////////////////////////////////////////////////////////////

void LogHandlers::Execute (DebugSession & session, const DebugCommand & command, IoEventLog & log, bool isVideo, Reply & reply)
{
    std::istringstream        stream (command.text);
    std::string               verb;
    std::string               argument;
    std::string               extra;
    std::string               rest;
    std::vector<std::string>  lines;
    const char              * title  = isVideo ? "Video log" : "Sound log";
    size_t                    count  = kDefaultListCount;
    size_t                    first  = 0;



    stream >> verb;
    std::getline (stream, rest);
    verb  = SymbolTable::ToUpper (verb);
    first = rest.find_first_not_of (' ');
    rest  = (first == std::string::npos) ? std::string() : rest.substr (first, rest.find_last_not_of (' ') - first + 1);

    if (verb == "SAVE")
    {
        Save (session, log, rest.empty() ? (isVideo ? kDefaultVideoFile : kDefaultSoundFile) : rest, reply);
        return;
    }

    stream.clear();
    stream.str (rest);
    stream >> argument >> extra;

    if (verb == "ON" && argument.empty())
    {
        TurnOn (session, log, isVideo);
        reply.data = MessageData { { std::format ("{} on.", title) } };
    }
    else if (verb == "OFF" && argument.empty())
    {
        log.SetOn (false);
        UpdateSink (session);
        reply.data = MessageData { { std::format ("{} off.", title) } };
    }
    else if (verb == "CLEAR" && argument.empty())
    {
        log.Clear();
        reply.data = MessageData { { std::format ("{} cleared.", title) } };
    }
    else if ((verb.empty() || verb == "LIST") && extra.empty() && (argument.empty() || (argument.size() <= kMaxCountDigits && std::all_of (argument.begin(), argument.end(), ::isdigit))))
    {
        count = argument.empty() ? kDefaultListCount : (size_t) std::stoull (argument);
        log.Format (count, lines);
        reply.data = MessageData { lines };
    }
    else
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("{} takes ON, OFF, CLEAR, LIST [n] or SAVE [file].", isVideo ? "VIDEOLOG" : "SOUNDLOG"));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::TurnOn
//
//  The video log starts from the mode the machine is in now, so its first
//  entry is the first change.
//
////////////////////////////////////////////////////////////////////////////////

void LogHandlers::TurnOn (DebugSession & session, IoEventLog & log, bool isVideo)
{
    if (isVideo)
    {
        session.GetTarget().GetSoftSwitches (m_switches);
        m_videoBits = IoEventLog::GetVideoModeBits (m_switches);
    }

    log.SetOn (true);
    UpdateSink (session);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::UpdateSink
//
//  The I/O page while either log is on, and the Mockingboard's page while
//  the sound log is and the machine has one; nothing, and no sink, while
//  both are off.
//
////////////////////////////////////////////////////////////////////////////////

void LogHandlers::UpdateSink (DebugSession & session)
{
    WatchedPages  pages = {};
    Word          base  = 0;



    if (!m_video.IsOn() && !m_sound.IsOn())
    {
        session.GetWatchpoints().SetIoLogSink (nullptr, pages);
        return;
    }

    pages[kIoPage] = true;

    if (m_sound.IsOn() && session.GetTarget().TryGetMockingboardBase (base))
    {
        pages[base >> kPageShift] = true;
    }

    session.GetWatchpoints().SetIoLogSink ([this, &session] (Word address, Byte value, BusAccess access)
    {
        OnIoAccess (session, address, value, access);
    }, pages);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::OnIoAccess
//
//  The machine's count and beam position are those at the start of the
//  instruction making the access. A video switch is judged by the mode it
//  leaves behind, read back from the machine, so an access that changes
//  nothing records nothing.
//
////////////////////////////////////////////////////////////////////////////////

void LogHandlers::OnIoAccess (DebugSession & session, Word address, Byte value, BusAccess access)
{
    IDebugTarget   & target  = session.GetTarget();
    bool             isWrite = access == BusAccess::Write;
    uint64_t         cycle   = target.GetCycleCount();
    VideoPosition    beam;
    uint16_t         after   = 0;
    Word             base    = 0;



    if (m_video.IsOn() && IoEventLog::IsVideoSwitch (address, isWrite))
    {
        target.GetSoftSwitches (m_switches);
        after = IoEventLog::GetVideoModeBits (m_switches);
        beam  = target.GetVideoPosition();

        m_video.RecordVideo (cycle,
                             beam.scanline * VideoTiming::kCyclesPerScanline + beam.cycleInLine,
                             beam.scanline,
                             beam.cycleInLine,
                             address,
                             m_videoBits,
                             after);
        m_videoBits = after;
    }

    if (!m_sound.IsOn())
    {
        return;
    }

    if (IoEventLog::IsSpeaker (address))
    {
        m_sound.RecordSpeaker (cycle, address);
    }
    else if (isWrite && (address >> kPageShift) != kIoPage && target.TryGetMockingboardBase (base) && (address >> kPageShift) == (base >> kPageShift))
    {
        m_sound.RecordMockingboard (cycle, address, value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::Save
//
////////////////////////////////////////////////////////////////////////////////

void LogHandlers::Save (DebugSession & session, const IoEventLog & log, const std::string & name, Reply & reply) const
{
    IFileSystem               * files = session.GetFileSystem();
    std::vector<std::string>    lines;
    std::string                 text;
    HRESULT                     hr    = S_OK;



    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    log.Format (log.GetEntries().size(), lines);

    for (const std::string & line : lines)
    {
        text += line + "\n";
    }

    hr = files->WriteAllText (session.ResolvePath (name), text);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", name));
        return;
    }

    reply.data = MessageData { { std::format ("Saved the log{}.", session.GetPathEcho (name, "to")) } };
}
