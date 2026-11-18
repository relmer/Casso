#include "Pch.h"

#include "Ui/Debugger/Panes/DisassemblySourceText.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/SourceRows.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblySourceText::TryGet
//
//  A source line's text for the source rows, read through the host as a
//  source document reads its file, once per file while the same debug file
//  and program are loaded. A file not found is looked up again with the next
//  snapshot, since a dropped or located source can be found then.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblySourceText::TryGet (int fileId, int line, std::wstring & text)
{
    const DebuggerViewSnapshot               * snapshot = m_latest.Get();
    IDebuggerWindowHost                      * host     = m_host.GetWindowHost();
    const DebuggerViewSnapshot::SourceState  * source   = nullptr;
    std::wstring                               key;
    SourceLookup                               lookup;



    if (snapshot == nullptr || !snapshot->source.has_value() || host == nullptr || line <= 0)
    {
        return false;
    }

    source = &*snapshot->source;
    key    = source->debugFilePath + TextEncoding::NarrowToWide (source->programKey);

    if (m_key != key)
    {
        m_key = key;
        m_text.clear();
        m_misses.clear();
    }

    if (m_missesFor != snapshot)
    {
        m_missesFor = snapshot;
        m_misses.clear();
    }

    if (!m_text.contains (fileId) && !m_misses.contains (fileId))
    {
        for (const DebugSourceFile & record : source->files)
        {
            if (record.id == fileId)
            {
                lookup = host->FindDebuggerSource (record, source->debugFilePath, source->programKey);
            }
        }

        if (lookup.match != SourceMatch::NotFound && !lookup.text.empty())
        {
            m_text[fileId] = SourceRows::SplitLines (lookup.text);
        }
        else
        {
            m_misses.insert (fileId);
        }
    }

    if (!m_text.contains (fileId) || line > (int) m_text[fileId].size())
    {
        return false;
    }

    text = m_text[fileId][(size_t) (line - 1)];
    return true;
}
