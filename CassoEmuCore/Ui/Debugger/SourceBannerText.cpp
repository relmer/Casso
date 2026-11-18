#include "Pch.h"

#include "Ui/Debugger/SourceBannerText.h"

#include "Debugger/Source/SourcePathList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceBannerText::GetPlace
//
//  The file and line at a level, -1 being the body line. A state with no
//  places has only its two ends.
//
////////////////////////////////////////////////////////////////////////////////

std::pair<int, int> SourceBannerText::GetPlace (const DebuggerViewSnapshot::SourceState & state, int level)
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
//  SourceBannerText::GetStaleText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourceBannerText::GetStaleText (const std::wstring & debugFilePath)
{
    return std::format (L"The program in memory doesn't match {}, so this file's lines aren't mapped to it.", fs::path (debugFilePath).filename().wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceBannerText::GetBannerText
//
//  What the file needs said, then where a macro is, each on a line of its own
//  so the two do not read as one sentence.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourceBannerText::GetBannerText (SourceMatch match, const std::string & fileName, bool hasText,
                                              int depth, bool showingBody, const std::string & bodyName, int bodyLine,
                                              const std::string & invokedByName, int invokedByLine)
{
    std::string  text;
    std::string  invokedBy;



    //  No file is open -- the PC is outside every source line -- so there is
    //  no file to say anything about.
    if (fileName.empty())
    {
        match = SourceMatch::Exact;
    }

    switch (match)
    {
    case SourceMatch::Mismatch:
        text = std::format ("{} is not the file that was assembled, so lines may not match.", fileName);
        break;

    case SourceMatch::Unverified:
        text = std::format ("{} has no recorded hash. It was matched by name and size.", fileName);
        break;

    case SourceMatch::NotFound:
        text = hasText ? std::format ("{} matches no file in the debug file and has no line mapping.", fileName)
                       : std::format ("{} was not found. Drop it on this window or browse for it.", fileName);
        break;

    default:
        break;
    }

    if (depth > 0)
    {
        //  The invocation that produced the shown line: the next level out.
        if (showingBody && invokedByLine > 0)
        {
            invokedBy = invokedByName.empty() ? std::format (", invoked by line {}", invokedByLine)
                                              : std::format (", invoked by {} line {}", invokedByName, invokedByLine);
        }

        text += text.empty() ? "" : "\n";
        text += showingBody ? std::format ("Showing the macro body: {} line {}{}.", bodyName, bodyLine, invokedBy)
                            : std::format ("Stopped inside a macro. Its body line is {} line {}.", bodyName, bodyLine);
    }

    return SourcePathList::Utf8ToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceBannerText::GetFoundElsewhereText
//
//  A file gone from where the debug file records it can still be found by
//  name in another search folder: a copy, from another checkout or an
//  earlier session. Its text is displayed, so the banner has where it is from,
//  and whether its hash is the one in the debug file.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourceBannerText::GetFoundElsewhereText (const std::string & fileName, const std::wstring & debugFilePath,
                                                      const std::wstring & foundPath, bool isVerified)
{
    std::wstring  recorded;



    if (fileName.empty() || foundPath.empty() || debugFilePath.empty())
    {
        return std::wstring();
    }

    recorded = SourceService::Combine (SourceService::GetFolder (debugFilePath), SourcePathList::Utf8ToWide (fileName));

    if (SourcePathList::IsSameFolder (SourceService::GetFolder (recorded), SourceService::GetFolder (foundPath)))
    {
        return std::wstring();
    }

    return std::format (L"Source file {} was not found with the debug file {}. Showing the copy in {}.{}",
                        SourcePathList::Utf8ToWide (fileName), debugFilePath, SourceService::GetFolder (foundPath),
                        isVerified ? L" Its contents are the ones the program was built from." : L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceBannerText::CanShowBody
//
//  A macro's body can be shown unless it is in the very file that could not
//  be found: then there is nothing to show, and neither the button nor the
//  line it would go to means anything.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceBannerText::CanShowBody (bool hasText, int depth, int bodyFileId, int fileId)
{
    return depth > 0 && (hasText || bodyFileId != fileId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceBannerText::GetNextMacroLevel
//
////////////////////////////////////////////////////////////////////////////////

int SourceBannerText::GetNextMacroLevel (int level, int deepest)
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
//  SourceBannerText::GetMacroLevel
//
//  Outside every macro there is only level 0. Without the places between,
//  the body line is level 1.
//
////////////////////////////////////////////////////////////////////////////////

int SourceBannerText::GetMacroLevel (const DebuggerViewSnapshot::SourceState & state, int level)
{
    int  deepest = state.places.empty() ? 1 : (int) state.places.size() - 1;



    if (state.depth == 0)
    {
        return 0;
    }

    return (level < 0 || level > deepest) ? deepest : level;
}
