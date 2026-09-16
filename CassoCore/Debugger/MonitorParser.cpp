#include "Pch.h"

#include "Debugger/MonitorParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kCommandCharacters
//
//  Empty until the parser lands. The ROM sweep asserts against this, so it
//  fails until there is something to assert against; that is the point of
//  running the sweep first.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Byte  s_kCommandCharacters[] = { 0 };





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::GetCommandCharacters
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> MonitorParser::GetCommandCharacters()
{
    return std::span<const Byte> (s_kCommandCharacters, (size_t) 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser::IsCommandCharacter
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorParser::IsCommandCharacter (Byte character)
{
    std::span<const Byte>  characters = GetCommandCharacters();



    return std::find (characters.begin(), characters.end(), character) != characters.end();
}
