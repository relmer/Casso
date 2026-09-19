#include "Pch.h"

#include "Debugger/CommandModeNames.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kModes
//
//  Each mode with its name and the output format it sets. The formats share
//  the modes' names, since each is the format of the mode it is named for.
//
////////////////////////////////////////////////////////////////////////////////

struct CommandModeNames::Entry
{
    const char    * name;
    CommandMode     mode;
    OutputFormat    format;
};

static constexpr CommandModeNames::Entry  s_kModes[] =
{
    { "applewin",  CommandMode::AppleWin,  OutputFormat::AppleWin  },
    { "monitor",   CommandMode::Monitor,   OutputFormat::Monitor   },
    { "gssquared", CommandMode::GSSquared, OutputFormat::GSSquared },
};





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeNames::TryParse
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeNames::TryParse (const std::string & name, CommandMode & mode)
{
    for (const Entry & entry : s_kModes)
    {
        if (_stricmp (entry.name, name.c_str()) == 0)
        {
            mode = entry.mode;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeNames::TryParse
//
////////////////////////////////////////////////////////////////////////////////

bool CommandModeNames::TryParse (const std::string & name, OutputFormat & format)
{
    for (const Entry & entry : s_kModes)
    {
        if (_stricmp (entry.name, name.c_str()) == 0)
        {
            format = entry.format;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeNames::GetName
//
////////////////////////////////////////////////////////////////////////////////

const char * CommandModeNames::GetName (CommandMode mode)
{
    for (const Entry & entry : s_kModes)
    {
        if (entry.mode == mode)
        {
            return entry.name;
        }
    }

    return s_kModes[0].name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeNames::GetName
//
////////////////////////////////////////////////////////////////////////////////

const char * CommandModeNames::GetName (OutputFormat format)
{
    for (const Entry & entry : s_kModes)
    {
        if (entry.format == format)
        {
            return entry.name;
        }
    }

    return s_kModes[0].name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeNames::GetUpperName
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandModeNames::GetUpperName (CommandMode mode)
{
    std::string  name = GetName (mode);



    for (char & ch : name)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeNames::GetUpperName
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandModeNames::GetUpperName (OutputFormat format)
{
    std::string  name = GetName (format);



    for (char & ch : name)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeNames::GetOutputFormat
//
//  The format a mode sets when it is selected.
//
////////////////////////////////////////////////////////////////////////////////

OutputFormat CommandModeNames::GetOutputFormat (CommandMode mode)
{
    for (const Entry & entry : s_kModes)
    {
        if (entry.mode == mode)
        {
            return entry.format;
        }
    }

    return OutputFormat::AppleWin;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeNames::GetList
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandModeNames::GetList()
{
    std::string  list;



    for (size_t i = 0; i < std::size (s_kModes); ++i)
    {
        if (i > 0)
        {
            list += (i + 1 == std::size (s_kModes)) ? ", and " : ", ";
        }

        list += s_kModes[i].name;
    }

    return list;
}
