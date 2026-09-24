#include "Pch.h"

#include "Ui/Debugger/Panes/SourcePane.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::SourcePane
//
////////////////////////////////////////////////////////////////////////////////

SourcePane::SourcePane (DxuiTextView * view, DxuiActionBanner * banner, FindFn find, RunFn run, GoToFn goTo) :
    m_view   (view),
    m_banner (banner),
    m_find   (std::move (find)),
    m_run    (std::move (run)),
    m_goTo   (std::move (goTo))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::Configure (HWND hwnd)
{
    m_view->SetOwnerWindow (hwnd);
    m_banner->SetSeverity  (DxuiInfoBanner::Severity::Info);
    m_banner->SetOnAction  ([this] (size_t) { ToggleBody(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::Apply
//
//  A debug file loaded again starts the document over. Its file is found when
//  the window first gives it one; a dropped file stays until it gives another.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::Apply (const DebuggerViewSnapshot & snapshot)
{
    std::wstring  loadedFor;
    bool          keepDrop  = false;



    if (!snapshot.source.has_value())
    {
        if (m_state.has_value())
        {
            m_state.reset();
            m_lines.clear();
            m_fileId   = -1;
            m_rowsLine = -1;
            m_view->SetRows ({});
            m_banner->SetText (L"");
        }

        return;
    }

    loadedFor = snapshot.source->debugFilePath + SourcePathList::Utf8ToWide (snapshot.source->programKey);

    if (loadedFor != m_loadedFor)
    {
        m_loadedFor  = loadedFor;
        m_fileId     = -1;
        m_isDropped  = false;
        m_showBody   = false;
        m_rowsFileId = -2;
    }

    m_state  = snapshot.source;
    keepDrop = m_isDropped && m_docFileId == m_droppedAt;

    if (m_docFileId >= 0 && m_docFileId != m_fileId && !keepDrop)
    {
        LoadFile (m_docFileId);
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::SetFile
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::SetFile (int fileId)
{
    if (fileId == m_docFileId)
    {
        return;
    }

    m_docFileId      = fileId;
    m_isDropped      = false;
    m_pendingTopLine = 0;

    if (fileId < 0)
    {
        m_lines.clear();
        m_fileId     = -1;
        m_rowsFileId = -2;
        m_view->SetRows ({});
        m_banner->SetText (L"");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetTopSourceLine
//
//  The source line whose row holds the view's top line: a long line wraps to
//  several view lines, so the two are not the same count. A view not laid
//  out -- a document behind another tab -- cannot say, and gives 0; so does a
//  line still waiting to be placed, which is given back as it is.
//
////////////////////////////////////////////////////////////////////////////////

int SourcePane::GetTopSourceLine() const
{
    int  top  = m_view->GetTopLine();
    int  line = 0;



    if (m_pendingTopLine > 0)
    {
        return m_pendingTopLine;
    }

    for (int row = 0; row < (int) m_lines.size(); row++)
    {
        int  first = m_view->GetFirstLineOfRow (row);

        if (first < 0 || first > top)
        {
            break;
        }

        line = row + 1;
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::SetTopSourceLine
//
//  Placed once the rows are laid out; until then it waits, as ScrollTo does.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::SetTopSourceLine (int line)
{
    int  first = (line > 0) ? m_view->GetFirstLineOfRow (line - 1) : -1;



    m_pendingTopLine = (first < 0) ? line : 0;

    if (first >= 0)
    {
        m_view->SetTopLine (first);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::LoadFile
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::LoadFile (int fileId)
{
    SourceLookup  lookup;



    for (const DebugSourceFile & record : m_state->files)
    {
        if (record.id == fileId)
        {
            lookup = m_find (record, m_state->debugFilePath, m_state->programKey);
        }
    }

    m_fileId    = fileId;
    m_match     = lookup.match;
    m_isDropped = false;
    m_lines     = SplitLines (lookup.text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::Rebuild
//
//  The rows when anything they show has changed, and the banner.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::Rebuild()
{
    int                                   marked       = (GetShownFileId() == m_fileId) ? GetShownLine() : -1;
    std::set<int>                         breakpoints;
    const DebugSourceFile               * body         = nullptr;
    bool                                  isRowsStale  = false;
    int                                   top          = m_view->GetTopLine();
    int                                   depth        = 0;
    bool                                  holdsPc      = false;



    for (const std::tuple<int, int, int> & bp : m_state->breakpointLines)
    {
        if (std::get<0> (bp) == m_fileId)
        {
            breakpoints.insert (std::get<1> (bp));
        }
    }

    isRowsStale = m_rowsFileId != m_fileId || m_rowsLine != marked || m_rowsBreakpoints != breakpoints;

    if (isRowsStale)
    {
        bool  isNewLine = m_rowsLine != marked || m_rowsFileId != m_fileId;

        m_view->SetRows  (BuildRows (m_lines, marked, breakpoints));
        m_view->SetTopLine (top);

        if (isNewLine && marked > 0)
        {
            ScrollTo (marked);
        }

        m_rowsFileId      = m_fileId;
        m_rowsLine        = marked;
        m_rowsBreakpoints = std::move (breakpoints);
    }

    for (const DebugSourceFile & record : m_state->files)
    {
        body = (record.id == m_state->bodyFileId) ? &record : body;
    }

    //  Only the document the PC is in speaks of the macro: the invocation's,
    //  or the body's while the body is shown.
    holdsPc = m_fileId >= 0 && (m_fileId == m_state->fileId || (m_showBody && m_fileId == m_state->bodyFileId));
    depth   = (holdsPc && CanShowBody (!m_lines.empty(), m_state->depth, m_state->bodyFileId, m_fileId)) ? m_state->depth : 0;

    m_banner->SetText (GetBannerText (m_match, GetFileName (m_fileId), !m_lines.empty(), depth, m_showBody,
                                      body != nullptr ? body->name : std::string(), m_state->bodyLine));

    if (depth > 0 && m_banner->GetAction (0) == nullptr)
    {
        m_banner->SetActions ({ L"Show body" });
    }
    else if (depth == 0 && m_banner->GetAction (0) != nullptr)
    {
        m_banner->SetActions ({});
    }

    //  Relabeled rather than replaced: this can run inside the button's own
    //  click.
    if (m_banner->GetAction (0) != nullptr)
    {
        m_banner->GetAction (0)->SetLabel (m_showBody ? L"Show invocation" : L"Show body");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::ScrollTo
//
//  Leaves the view alone when the line is already on screen; otherwise puts
//  it a third of the way down. A view that has not been painted yet has not
//  measured its text and cannot place the line, so the scroll waits for
//  FollowMarkedLine.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::ScrollTo (int line)
{
    int  first = m_view->GetFirstLineOfRow (line - 1);
    int  top   = m_view->GetTopLine();
    int  cap   = std::max (1, m_view->GetLineCap());



    m_followPending = first < 0;

    if (first >= 0 && (first < top || first >= top + cap))
    {
        m_view->SetTopLine (std::max (0, first - cap / 3));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::FollowMarkedLine
//
//  Once a frame: brings the marked line into view if an earlier scroll could
//  not place it, as happens when the rows arrive before the view is laid out
//  or painted.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::FollowMarkedLine()
{
    if (m_pendingTopLine > 0)
    {
        SetTopSourceLine (m_pendingTopLine);
    }
    else if (m_followPending && m_rowsLine > 0)
    {
        ScrollTo (m_rowsLine);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::ShowLine
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::ShowLine (int fileId, int line)
{
    if (fileId == m_fileId && line > 0)
    {
        ScrollTo (line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetLineAt
//
//  The 1-based line under a point, when a file with a line mapping is shown.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int> SourcePane::GetLineAt (POINT atDip) const
{
    DxuiTextView::Position  at = m_view->HitTest (atDip);



    if (!m_state.has_value() || m_fileId < 0 || at.row < 0 || at.row >= (int) m_lines.size())
    {
        return std::nullopt;
    }

    return at.row + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::OnClick
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::OnClick (POINT atDip)
{
    std::optional<int>  line = GetLineAt (atDip);
    auto                map  = (m_state.has_value()) ? m_state->lineAddresses : nullptr;



    if (!line.has_value() || map == nullptr || !map->contains ({ m_fileId, *line }))
    {
        return;
    }

    m_goTo (map->at ({ m_fileId, *line }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::OnDoubleClick
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::OnDoubleClick (POINT atDip)
{
    std::optional<int>  line = GetLineAt (atDip);



    if (line.has_value())
    {
        m_run (GetToggleLine (*m_state, m_fileId, *line));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::ShowDropped
//
//  A match is that record's text, with the warning its match calls for. No
//  match is plain text with no line mapping, until the PC needs another file.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::ShowDropped (const SourceLookup & lookup, int recordIndex)
{
    if (!m_state.has_value())
    {
        return;
    }

    m_lines     = SplitLines (lookup.text);
    m_match     = lookup.match;
    m_isDropped = true;
    m_droppedAt = m_docFileId;
    m_fileId    = (recordIndex >= 0 && recordIndex < (int) m_state->files.size()) ? m_state->files[(size_t) recordIndex].id : -1;
    m_rowsFileId = -2;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::ToggleBody
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::ToggleBody()
{
    if (!m_state.has_value() || m_state->depth == 0 || !m_onToggleBody)
    {
        return;
    }

    m_onToggleBody();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetShownFileId
//
////////////////////////////////////////////////////////////////////////////////

int SourcePane::GetShownFileId() const
{
    return (m_showBody && m_state->depth > 0) ? m_state->bodyFileId : m_state->fileId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetShownLine
//
////////////////////////////////////////////////////////////////////////////////

int SourcePane::GetShownLine() const
{
    return (m_showBody && m_state->depth > 0) ? m_state->bodyLine : m_state->line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetFileName
//
////////////////////////////////////////////////////////////////////////////////

std::string SourcePane::GetFileName (int fileId) const
{
    for (const DebugSourceFile & record : m_state->files)
    {
        if (record.id == fileId)
        {
            return record.name;
        }
    }

    return m_isDropped ? std::string ("The dropped file") : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::SplitLines
//
//  Lines ending in LF, CR LF or CR, with tabs expanded to every eighth
//  column: the view reads a tab as the break between cells.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> SourcePane::SplitLines (const std::string & text)
{
    std::vector<std::wstring>  lines;
    std::string                line;
    auto                       flush = [&] ()
                                       {
                                           std::wstring  wide = SourcePathList::Utf8ToWide (line);
                                           std::wstring  expanded;

                                           for (wchar_t c : wide)
                                           {
                                               if (c != L'\t')
                                               {
                                                   expanded += c;
                                                   continue;
                                               }

                                               expanded.append ((size_t) (kTabWidth - (int) expanded.size() % kTabWidth), L' ');
                                           }

                                           lines.push_back (std::move (expanded));
                                           line.clear();
                                       };



    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '\r' || text[i] == '\n')
        {
            flush();

            if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
            {
                i++;
            }

            continue;
        }

        line += text[i];
    }

    if (!line.empty())
    {
        flush();
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::BuildRows
//
//  A marker, the line number and the text. The marker is the triangle on the
//  marked line and a bullet on a line with a breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTextView::Row> SourcePane::BuildRows (const std::vector<std::wstring> & lines, int markedLine,
                                                      const std::set<int> & breakpointLines)
{
    std::vector<DxuiTextView::Row>  rows;
    int                             width = (int) std::to_wstring (lines.size()).size();



    for (size_t i = 0; i < lines.size(); i++)
    {
        int           number = (int) i + 1;
        std::wstring  marker = breakpointLines.contains (number) ? std::wstring (1, s_kchBullet) : std::wstring (L" ");

        marker += (number == markedLine) ? std::wstring (s_kpszTriangleRight) : std::wstring (L" ");

        rows.push_back ({ { marker, std::format (L"{:>{}}", number, width), lines[i] } });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetToggleLine
//
////////////////////////////////////////////////////////////////////////////////

std::string SourcePane::GetToggleLine (const DebuggerViewSnapshot::SourceState & state, int fileId, int line)
{
    std::string  name;



    for (const std::tuple<int, int, int> & bp : state.breakpointLines)
    {
        if (std::get<0> (bp) == fileId && std::get<1> (bp) == line)
        {
            return std::format ("BPC {}", std::get<2> (bp));
        }
    }

    for (const DebugSourceFile & record : state.files)
    {
        name = (record.id == fileId) ? record.name : name;
    }

    return std::format ("BP {}:{}", name, line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetBannerText
//
//  What the file needs said, then where a macro is, each on a line of its own
//  so the two do not read as one sentence.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourcePane::GetBannerText (SourceMatch match, const std::string & fileName, bool hasText,
                                        int depth, bool showingBody, const std::string & bodyName, int bodyLine)
{
    std::string  text;



    switch (match)
    {
    case SourceMatch::Mismatch:
        text = std::format ("{} is not the file that was assembled, so lines may not match.", fileName);
        break;

    case SourceMatch::Unverified:
        text = std::format ("{} has no recorded hash; it was matched by name and size.", fileName);
        break;

    case SourceMatch::NotFound:
        text = hasText ? std::format ("{} matches no file in the debug file and has no line mapping.", fileName)
                       : std::format ("{} was not found. Drop it on this window to open it.", fileName);
        break;

    default:
        break;
    }

    if (depth > 0)
    {
        text += text.empty() ? "" : "\n";
        text += showingBody ? std::format ("Showing the macro body: {} line {}.", bodyName, bodyLine)
                            : std::format ("Stopped inside a macro; its body line is {} line {}.", bodyName, bodyLine);
    }

    return SourcePathList::Utf8ToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::CanShowBody
//
//  A macro's body can be shown unless it is in the very file that could not
//  be found: then there is nothing to show, and neither the button nor the
//  line it would go to means anything.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::CanShowBody (bool hasText, int depth, int bodyFileId, int fileId)
{
    return depth > 0 && (hasText || bodyFileId != fileId);
}
