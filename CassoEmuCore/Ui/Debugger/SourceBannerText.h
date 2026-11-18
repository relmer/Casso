#pragma once

#include "Debugger/Source/SourceService.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceBannerText
//
//  The banner over a source document: what it reads for a file that was not
//  found, was found elsewhere or is out of date, and the macro levels its
//  button moves through.
//
////////////////////////////////////////////////////////////////////////////////

class SourceBannerText
{
public:
    static std::wstring  GetBannerText (SourceMatch match, const std::string & fileName, bool hasText,
                                        int depth, bool showingBody, const std::string & bodyName, int bodyLine,
                                        const std::string & invokedByName = std::string(), int invokedByLine = 0);

    //  The banner's first line while the program in memory isn't the debug
    //  file's.
    static std::wstring  GetStaleText (const std::wstring & debugFilePath);

    //  The text for a file found in a folder other than the one the debug
    //  file has it in -- a copy in another search folder -- or empty. A copy
    //  whose hash is the debug file's is the file the program was built from,
    //  and the text has that.
    static std::wstring  GetFoundElsewhereText (const std::string & fileName, const std::wstring & debugFilePath,
                                                const std::wstring & foundPath, bool isVerified = false);

    //  Whether a macro's body can be offered: not when it is in the file that
    //  could not be found.
    static bool          CanShowBody   (bool hasText, int depth, int bodyFileId, int fileId);

    //  The level the banner's button goes to: from outside every macro to the
    //  body line, and from a body back out one invocation at a time.
    static int           GetNextMacroLevel (int level, int deepest);

    //  The level a request resolves to, -1 being the deepest.
    static int           GetMacroLevel (const DebuggerViewSnapshot::SourceState & state, int level);

    //  The file and line at a level, -1 being the body line.
    static std::pair<int, int>  GetPlace (const DebuggerViewSnapshot::SourceState & state, int level);
};
