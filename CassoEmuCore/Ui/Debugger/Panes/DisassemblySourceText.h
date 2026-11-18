#pragma once

#include "Pch.h"

class IDebuggerPaneHost;
struct DebuggerViewSnapshot;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySourceText
//
//  The source lines the disassembly views' source rows are drawn from, read
//  once per file while the same debug file and program are loaded.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblySourceText
{
public:
    explicit DisassemblySourceText (IDebuggerPaneHost & host) : m_host (host) {}

    //  A source line's text, as it is drawn in the source rows; false where
    //  the file isn't found.
    bool  TryGet (int fileId, int line, std::wstring & text);

private:
    IDebuggerPaneHost                         & m_host;
    std::wstring                                m_key;        // the debug file's path and program key the text was read for
    std::map<int, std::vector<std::wstring>>    m_text;
    std::set<int>                               m_misses;     // files not found for m_missesFor
    const DebuggerViewSnapshot                * m_missesFor = nullptr;
};
