#include "Pch.h"

#include "Ui/Debugger/SessionActions.h"
#include "Ui/Debugger/DebuggerCommandLines.h"

#include "Debugger/CommandModeNames.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetPanel
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetPanel (const std::string & id, bool open, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (open ? DebugVerb::OpenPanel : DebugVerb::ClosePanel, DebuggerCommandLines::GetPanelLine (id, open), mode);



    action.command.text = id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetTraceToggle
//
//  The command's count is 1 to turn tracing on and 0 to turn it off.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetTraceToggle (bool isOn, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SetHistory, DebuggerCommandLines::GetTraceToggleLine (isOn), mode);



    action.command.count = isOn ? 0 : 1;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetSetMode
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetSetMode (CommandMode target, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SetMode, "MODE " + CommandModeNames::GetUpperName (target), mode);



    action.command.mode = target;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetCallStackMode
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetCallStackMode (const std::string & mechanism, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SetCallStackMode, "CALLS MODE " + mechanism, mode);



    action.command.text = mechanism;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetLoadBasic
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetLoadBasic (const std::string & path, CommandMode mode)
{
    std::string     quoted = std::format ("\"{}\"", path);
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::LoadBasic, "BASICLOAD " + quoted, mode);



    action.command.text = quoted;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetSaveBasic
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetSaveBasic (const std::string & path, CommandMode mode)
{
    std::string     quoted = std::format ("\"{}\"", path);
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SaveBasic, "BASICSAVE " + quoted, mode);



    action.command.text = quoted;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetClearBasic
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetClearBasic (CommandMode mode)
{
    return DebuggerActions::Make (DebugVerb::ClearBasic, "BASICCLEAR", mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetProject
//
//  PROJECT with its subcommand and a file name, as `SAVEAS "file"`, or the
//  subcommand alone when there is no file, as `MERGE`.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetProject (const std::string & subcommand, const std::string & path, CommandMode mode)
{
    std::string     text   = path.empty() ? subcommand : std::format ("{} \"{}\"", subcommand, path);
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::Project, "PROJECT " + text, mode);



    action.command.text = text;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetIoLog
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetIoLog (bool isVideo, const std::string & words, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (isVideo ? DebugVerb::VideoLog : DebugVerb::SoundLog,
                                                    std::format ("{} {}", isVideo ? "VIDEOLOG" : "SOUNDLOG", words), mode);



    action.command.text = words;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetProfile
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetProfile (const std::string & words, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::Profile, "PROFILE " + words, mode);



    action.command.text = words;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions::GetSaveHistory
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction SessionActions::GetSaveHistory (const std::string & path, CommandMode mode)
{
    static constexpr const char * kVerb   = "HISTORY SAVE ";
    std::string                   line    = DebuggerCommandLines::GetLineWithFileName (kVerb, path);
    DebuggerAction                action  = DebuggerActions::Make (DebugVerb::SaveHistory, line, mode);



    //  The file name as the line carries it, quoted when it needs to be, as
    //  the parser leaves it.
    action.command.text = line.substr (strlen (kVerb));
    return action;
}
