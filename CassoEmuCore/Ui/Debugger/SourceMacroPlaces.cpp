#include "Pch.h"

#include "Ui/Debugger/SourceMacroPlaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceMacroPlaces::GetPlace
//
//  The file and line at a level, -1 being the body line. A state with no
//  places has only its two ends.
//
////////////////////////////////////////////////////////////////////////////////

std::pair<int, int> SourceMacroPlaces::GetPlace (const DebuggerViewSnapshot::SourceState & state, int level)
{
    const std::vector<std::pair<int, int>> & places = state.places;



    if (places.empty())
    {
        return (level != 0 && state.depth > 0) ? std::pair<int, int> { state.bodyFileId, state.bodyLine }
                                               : std::pair<int, int> { state.fileId, state.line };
    }

    if (level < 0 || level >= (int) places.size())
    {
        return places.back();
    }

    return places[(size_t) level];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceMacroPlaces::CanShowBody
//
//  A macro's body can be shown unless it is in the very file that could not
//  be found: then there is nothing to show, and neither the button nor the
//  line it would go to means anything.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceMacroPlaces::CanShowBody (bool hasText, int depth, int bodyFileId, int fileId)
{
    return depth > 0 && (hasText || bodyFileId != fileId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceMacroPlaces::GetNextMacroLevel
//
////////////////////////////////////////////////////////////////////////////////

int SourceMacroPlaces::GetNextMacroLevel (int level, int deepest)
{
    if (deepest <= 0)
    {
        return 0;
    }

    if (level < 0 || level > deepest)
    {
        level = deepest;
    }

    return (level == 0) ? deepest : level - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceMacroPlaces::GetMacroLevel
//
//  Outside every macro there is only level 0. Without the places between,
//  the body line is level 1.
//
////////////////////////////////////////////////////////////////////////////////

int SourceMacroPlaces::GetMacroLevel (const DebuggerViewSnapshot::SourceState & state, int level)
{
    int  deepest = state.places.empty() ? 1 : (int) state.places.size() - 1;



    if (state.depth == 0)
    {
        return 0;
    }

    return (level < 0 || level > deepest) ? deepest : level;
}
