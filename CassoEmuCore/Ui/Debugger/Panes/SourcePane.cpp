#include "Pch.h"

#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/GutterGlyph.h"
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
    m_banner->SetOnAction  ([this] (size_t)
    {
        if (!m_isLoose)
        {
            ToggleBody();
        }
        else if (m_onLoadSymbols)
        {
            m_onLoadSymbols();
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::SetStyle
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::SetStyle (const Style & style)
{
    if (style == m_style)
    {
        return;
    }

    m_style        = style;
    m_isStyleStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::SetAssembler
//
//  The user's choice of whose grammar colors this document, or Any to go by
//  what the file's text shows.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::SetAssembler (SourceSyntax::Assembler assembler)
{
    if (assembler == m_assemblerChoice)
    {
        return;
    }

    m_assemblerChoice = assembler;
    m_isStyleStale    = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetAssembler
//
//  The grammar this document is colored with: the user's choice, or the one
//  its text shows.
//
////////////////////////////////////////////////////////////////////////////////

SourceSyntax::Assembler SourcePane::GetAssembler() const
{
    return (m_assemblerChoice != SourceSyntax::Assembler::Any) ? m_assemblerChoice : m_detected;
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



    //  A loose file keeps its text whatever debug file comes and goes.
    if (m_isLoose)
    {
        m_state = snapshot.source.has_value() ? *snapshot.source : DebuggerViewSnapshot::SourceState();
        Rebuild();
        return;
    }

    if (!snapshot.source.has_value())
    {
        if (m_state.has_value())
        {
            m_state.reset();
            m_lines.clear();
            m_fileId   = -1;
            m_rowsLine = -1;
            m_view->SetRows ({});
            m_rowLines.clear();
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
        m_macroLevel = 0;
        m_rowsFileId = -2;
    }

    m_state  = snapshot.source;
    keepDrop = m_isDropped && m_docFileId == m_droppedAt;

    m_disabledIds.clear();

    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        if (!bp.enabled)
        {
            m_disabledIds.insert (bp.id);
        }
    }

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

    m_docFileId       = fileId;
    m_isDropped       = false;
    m_isLoose         = false;
    m_pendingTopLine  = 0;
    m_assemblerChoice = SourceSyntax::Assembler::Any;

    if (fileId < 0)
    {
        m_lines.clear();
        m_fileId     = -1;
        m_rowsFileId = -2;
        m_view->SetRows ({});
        m_rowLines.clear();
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

    for (int row = 0; row < (int) m_rowLines.size(); row++)
    {
        int  first = m_view->GetFirstLineOfRow (row);

        if (first < 0 || first > top)
        {
            break;
        }

        line = (m_rowLines[(size_t) row] > 0) ? m_rowLines[(size_t) row] : line;
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
    int  first = (line > 0) ? m_view->GetFirstLineOfRow (GetRowOfLine (line)) : -1;



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
    m_foundPath = lookup.path;
    m_lines     = SplitLines (lookup.text);
    m_detected  = SourceSyntax::DetectAssembler (m_lines);
    m_listing   = SourceSyntax::DetectListing (m_lines);
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
    int                                   marked       = (!m_isLoose && GetShownFileId() == m_fileId) ? GetShownLine() : -1;
    std::set<int>                         breakpoints;
    std::set<int>                         disabled;
    bool                                  isRowsStale  = false;
    int                                   top          = m_view->GetTopLine();
    int                                   depth        = 0;
    int                                   level        = GetMacroLevel (*m_state, m_macroLevel);
    bool                                  holdsPc      = false;
    std::pair<int, int>                   shown        = GetPlace (*m_state, (level > 0) ? level : -1);
    std::pair<int, int>                   invokedBy    = (level > 0) ? GetPlace (*m_state, level - 1) : std::pair<int, int> { -1, 0 };
    std::string                           invokedName;
    std::wstring                          bannerText;
    std::wstring                          elsewhere;



    for (const std::tuple<int, int, int> & bp : m_state->breakpointLines)
    {
        if (std::get<0> (bp) == m_fileId)
        {
            breakpoints.insert (std::get<1> (bp));

            if (m_disabledIds.contains (std::get<2> (bp)))
            {
                disabled.insert (std::get<1> (bp));
            }
        }
    }

    //  A line with an enabled breakpoint and a disabled one shows enabled.
    for (const std::tuple<int, int, int> & bp : m_state->breakpointLines)
    {
        if (std::get<0> (bp) == m_fileId && !m_disabledIds.contains (std::get<2> (bp)))
        {
            disabled.erase (std::get<1> (bp));
        }
    }

    isRowsStale = m_rowsFileId != m_fileId || m_rowsLine != marked || m_rowsBreakpoints != breakpoints ||
                  m_rowsDisabled != disabled || m_isStyleStale || m_rowsLineOperands != m_state->lineOperands;

    if (isRowsStale)
    {
        bool  isNewLine = m_rowsLine != marked || m_rowsFileId != m_fileId;

        std::map<int, std::pair<std::wstring, std::wstring>>  lineOperands;

        if (m_state->lineOperands != nullptr)
        {
            for (const auto & [place, operand] : *m_state->lineOperands)
            {
                if (place.first == m_fileId)
                {
                    lineOperands[place.second] = { SourcePathList::Utf8ToWide (operand.first), SourcePathList::Utf8ToWide (operand.second) };
                }
            }
        }

        m_view->SetRows  (BuildRows (m_lines, marked, breakpoints, disabled, m_style, lineOperands, GetAssembler(), m_listing));
        m_rowLines         = GetRowLines (m_view->GetRows());
        m_rowsDisabled     = disabled;
        m_rowsLineOperands = m_state->lineOperands;
        m_isStyleStale = false;
        m_view->SetTopLine (top);

        if (isNewLine && marked > 0)
        {
            ScrollTo (marked);
        }

        m_rowsFileId      = m_fileId;
        m_rowsLine        = marked;
        m_rowsBreakpoints = std::move (breakpoints);
    }

    if (m_isLoose)
    {
        ShowLooseBanner();
        return;
    }

    //  Only the document the PC is in speaks of the macro: the invocation's,
    //  or the shown level's while a body is shown.
    holdsPc = m_fileId >= 0 && (m_fileId == m_state->fileId || (level > 0 && m_fileId == shown.first));
    depth   = (holdsPc && CanShowBody (!m_lines.empty(), m_state->depth, m_state->bodyFileId, m_fileId)) ? m_state->depth : 0;

    //  A level's invocation in the same file goes by its line alone.
    if (invokedBy.first >= 0 && invokedBy.first != shown.first)
    {
        invokedName = GetFileName (invokedBy.first);
    }

    bannerText = GetBannerText (m_match, GetFileName (m_fileId), !m_lines.empty(), depth, level > 0,
                                GetFileName (shown.first), shown.second, invokedName, invokedBy.second);
    elsewhere  = m_isDropped ? std::wstring() : GetFoundElsewhereText (GetFileName (m_fileId), m_state->debugFilePath, m_foundPath);

    if (!elsewhere.empty())
    {
        bannerText = bannerText.empty() ? elsewhere : elsewhere + L"\n" + bannerText;
    }

    m_banner->SetText (bannerText);

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
        m_banner->GetAction (0)->SetLabel (level > 0 ? L"Show invocation" : L"Show body");
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
    int  first = m_view->GetFirstLineOfRow (GetRowOfLine (line));
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



    if (!m_state.has_value() || m_fileId < 0 || at.row < 0 || at.row >= (int) m_rowLines.size() || m_rowLines[(size_t) at.row] <= 0)
    {
        return std::nullopt;
    }

    return m_rowLines[(size_t) at.row];
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
    std::optional<int>  line  = GetLineAt (atDip);
    int                 file  = m_fileId;



    //  The window builds the action at once, in the session's mode.
    if (line.has_value() && GetToggleAction (*m_state, file, *line, CommandMode::AppleWin).has_value())
    {
        m_run ([this, file, at = *line] (CommandMode mode) { return *GetToggleAction (*m_state, file, at, mode); });
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
    m_detected  = SourceSyntax::DetectAssembler (m_lines);
    m_listing   = SourceSyntax::DetectListing (m_lines);
    m_rowsFileId = -2;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::ShowLoose
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::ShowLoose (const std::wstring & path, const std::string & text, bool isSource)
{
    if (!m_state.has_value())
    {
        m_state = DebuggerViewSnapshot::SourceState();
    }

    m_lines         = SplitLines (text);
    m_match         = SourceMatch::NotFound;
    m_isDropped     = false;
    m_isLoose       = true;
    m_isLooseSource = isSource;
    m_fileId        = -1;
    m_foundPath     = path;
    m_detected      = SourceSyntax::DetectAssembler (m_lines);
    m_listing       = SourceSyntax::DetectListing (m_lines);
    m_rowsFileId    = -2;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::ShowLooseBanner
//
//  Relabeled rather than replaced, as the body button is: this can run inside
//  the button's own click.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::ShowLooseBanner()
{
    m_banner->SetText (m_isLooseSource ? kpszNoSymbolsText : L"");

    if (!m_isLooseSource)
    {
        m_banner->SetActions ({});
    }
    else if (m_banner->GetAction (0) == nullptr)
    {
        m_banner->SetActions ({ kpszLoadSymbols });
    }
    else
    {
        m_banner->GetAction (0)->SetLabel (kpszLoadSymbols);
    }
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
    return GetPlace (*m_state, GetMacroLevel (*m_state, m_macroLevel)).first;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetShownLine
//
////////////////////////////////////////////////////////////////////////////////

int SourcePane::GetShownLine() const
{
    return GetPlace (*m_state, GetMacroLevel (*m_state, m_macroLevel)).second;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetPlace
//
//  The file and line at a level, -1 being the body line. A state with no
//  places has only its two ends.
//
////////////////////////////////////////////////////////////////////////////////

std::pair<int, int> SourcePane::GetPlace (const DebuggerViewSnapshot::SourceState & state, int level)
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
//  SourcePane::GetArrowTarget
//
//  The first line in the file that starts at the branch's target. With none,
//  the target lies below when its address is past the marked line's.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::GetArrowTarget (const DebuggerViewSnapshot::SourceState & state, int fileId, int markedLine,
                                 std::optional<int> & outTargetLine, bool & outIsBelow)
{
    std::optional<Word>  markedAt;



    outTargetLine.reset();

    if (!state.pcTarget.has_value() || markedLine <= 0 || state.lineAddresses == nullptr)
    {
        return false;
    }

    for (const auto & [place, address] : *state.lineAddresses)
    {
        if (place.first != fileId)
        {
            continue;
        }

        if (address == *state.pcTarget && !outTargetLine.has_value())
        {
            outTargetLine = place.second;
        }

        if (place.second == markedLine)
        {
            markedAt = address;
        }
    }

    if (outTargetLine.has_value())
    {
        outIsBelow = *outTargetLine > markedLine;
    }
    else
    {
        outIsBelow = !markedAt.has_value() || *state.pcTarget > *markedAt;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetBranchArrow
//
//  The arrow runs along the left of the source text, from the marked line to
//  the target's. A target out of view, or in no line of this file, runs the
//  line to that edge of the lines. The marked line out of view draws none.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::GetBranchArrow (BranchArrow::Input & input, Word & goesTo, bool & isTaken) const
{
    static constexpr int  s_kTextCell   = 2;
    std::optional<int>    targetLine;
    bool                  isBelow       = true;
    float                 x             = 0.0f;
    float                 y             = 0.0f;
    float                 top           = 0.0f;
    float                 bottom        = 0.0f;
    float                 scale         = 1.0f;
    RECT                  bounds        = {};



    if (!IsActive() || !m_view->IsVisible() ||
        !GetArrowTarget (*m_state, m_fileId, m_rowsLine, targetLine, isBelow) ||
        !m_view->GetCellAnchorPx (GetRowOfLine (m_rowsLine), s_kTextCell, x, y))
    {
        return false;
    }

    goesTo  = *m_state->pcTarget;
    isTaken = m_state->isPcTargetTaken;
    bounds  = m_view->GetBounds();
    scale   = m_view->GetPxPerDip();

    m_view->GetLinesSpanPx (top, bottom);

    input.mnemonicX     = x;
    input.sourceY       = y;
    input.isTargetBelow = isBelow;
    input.edgeY         = isBelow ? bottom : top;
    input.sourceEdgeY   = isBelow ? top : bottom;
    input.marginPx     *= scale;
    input.stubPx       *= scale;
    input.radiusPx     *= scale;
    input.headPx       *= scale;

    if (targetLine.has_value() && m_view->GetCellAnchorPx (GetRowOfLine (*targetLine), s_kTextCell, x, y))
    {
        input.targetY = y;
    }

    return input.mnemonicX - input.marginPx - input.stubPx >= (float) bounds.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::IsInstructionLine
//
//  Whether a line's opcode is a 65C02 mnemonic.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::IsInstructionLine (const std::wstring & line, SourceSyntax::Assembler assembler)
{
    for (const SourceSyntax::Run & run : SourceSyntax::GetSourceRuns (line, assembler))
    {
        if (run.token == SourceSyntax::Token::Mnemonic)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::BuildRows
//
//  A marker, the line number and the text. The marked line takes the PC's
//  row fill. With the style's icons, as the disassembly pane draws them, a
//  breakpoint is its icon in the view's glyph margin, and with its PC arrow
//  the marked line's arrow is there too, drawn over the line's breakpoint
//  when it has one, so both sit where Visual Studio puts them. Without the
//  icons a breakpoint is a bullet in the marker, and without the arrow the
//  marked line is a triangle there in the marker color.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTextView::Row> SourcePane::BuildRows (const std::vector<std::wstring> & lines, int markedLine,
                                                      const std::set<int> & breakpointLines,
                                                      const std::set<int> & disabledLines,
                                                      const Style         & style,
                                                      const std::map<int, std::pair<std::wstring, std::wstring>> & lineOperands,
                                                      SourceSyntax::Assembler assembler,
                                                      SourceSyntax::Listing   listing)
{
    std::vector<DxuiTextView::Row>  rows;
    int                             width  = (int) std::to_wstring (lines.size()).size();
    bool                            icons  = style.enabledIcon != nullptr;
    bool                            arrow  = style.pcIcon != nullptr;



    for (size_t i = 0; i < lines.size(); i++)
    {
        int                number   = (int) i + 1;
        bool               isMarked = number == markedLine;
        std::wstring       marker;
        DxuiTextView::Row  row;

        if (!icons)
        {
            marker = breakpointLines.contains (number) ? std::wstring (1, s_kchBullet) : std::wstring (L" ");
        }

        if (!arrow)
        {
            marker += isMarked ? std::wstring (s_kpszTriangleRight) : std::wstring (L" ");
        }

        row.cells = { marker, std::format (L"{:>{}}", number, width), lines[i] };

        if (icons && breakpointLines.contains (number))
        {
            row.icon = disabledLines.contains (number) ? style.disabledIcon : style.enabledIcon;
        }

        if (isMarked)
        {
            row.background = style.pcRowArgb;
            row.icon       = arrow ? GutterGlyph::MakeMarked (row.icon, style.pcIcon) : row.icon;

            if (!arrow && style.pcMarkerArgb != 0)
            {
                row.spans.push_back ({ 0, (int) marker.size() - 1, 1, style.pcMarkerArgb });
            }
        }

        //  What the line's instruction reads, then in the result color what
        //  it leaves, as the disassembly pane's operand column has them.
        //  Only on a line whose opcode is a mnemonic: a directive or a macro
        //  produced its bytes some other way than one instruction.
        if (!lineOperands.empty())
        {
            auto  found  = lineOperands.find (number);
            int   column = (int) row.cells.size();

            row.cells.emplace_back();

            if (found != lineOperands.end() && IsInstructionLine (lines[i], assembler))
            {
                row.cells.back() = found->second.first;

                if (!found->second.second.empty())
                {
                    std::wstring  result = kpszResultPrefix + found->second.second;

                    row.cells.back() += row.cells.back().empty() ? L"" : L"  ";

                    if (style.resultArgb != 0)
                    {
                        row.spans.push_back ({ column, (int) row.cells.back().size(), (int) result.size(), style.resultArgb });
                    }

                    row.cells.back() += result;
                }
            }
        }

        if (style.syntax.mnemonic != 0)
        {
            for (const SourceSyntax::Run & run : SourceSyntax::GetLineRuns (lines[i], assembler, listing))
            {
                row.spans.push_back ({ 2, run.start, run.length, style.syntax.Get (run.token) });
            }
        }

        rows.push_back (std::move (row));
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetRowLines
//
//  A row with a line number is the next source line; one without is a row of
//  the instructions under it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> SourcePane::GetRowLines (const std::vector<DxuiTextView::Row> & rows)
{
    std::vector<int>  lines;
    int               line  = 0;



    for (const DxuiTextView::Row & row : rows)
    {
        bool  isLine = row.cells.size() > 1 && row.cells[1].find_first_not_of (L' ') != std::wstring::npos;

        line += isLine ? 1 : 0;
        lines.push_back (isLine ? line : 0);
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetRowOfLine
//
//  The row a 1-based source line is on, or -1 when no row shows it.
//
////////////////////////////////////////////////////////////////////////////////

int SourcePane::GetRowOfLine (int line) const
{
    auto  found = std::find (m_rowLines.begin(), m_rowLines.end(), line);



    if (line <= 0 || found == m_rowLines.end())
    {
        return -1;
    }

    return (int) (found - m_rowLines.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetToggleLine
//
//  Empty for a file with no record in the debug file, which has no line a
//  breakpoint could be set on.
//
////////////////////////////////////////////////////////////////////////////////

std::string SourcePane::GetToggleLine (const DebuggerViewSnapshot::SourceState & state, int fileId, int line)
{
    std::optional<DebuggerAction>  action = GetToggleAction (state, fileId, line, CommandMode::AppleWin);



    return action.has_value() ? action->echo : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetToggleAction
//
//  Clears the breakpoint on the line by its id, or sets one there. Nothing
//  for a file with no record in the debug file.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerAction> SourcePane::GetToggleAction (const DebuggerViewSnapshot::SourceState & state, int fileId, int line, CommandMode mode)
{
    for (const std::tuple<int, int, int> & bp : state.breakpointLines)
    {
        if (std::get<0> (bp) == fileId && std::get<1> (bp) == line)
        {
            return DebuggerActions::GetClearBreakpoint (std::get<2> (bp), mode);
        }
    }

    for (const DebugSourceFile & record : state.files)
    {
        if (record.id == fileId)
        {
            return DebuggerActions::GetSourceBreakpoint (record.name, line, mode);
        }
    }

    return std::nullopt;
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
                       : std::format ("{} was not found. Drop it on this window to open it.", fileName);
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
//  SourcePane::GetFoundElsewhereText
//
//  A file gone from where the debug file records it can still be found by
//  name in another search folder: a copy, from another checkout or an
//  earlier session. Its text is shown, so the banner says whose it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourcePane::GetFoundElsewhereText (const std::string & fileName, const std::wstring & debugFilePath,
                                                const std::wstring & foundPath)
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

    return std::format (L"{} is not beside the debug file. Showing the copy in {}.",
                        SourcePathList::Utf8ToWide (fileName), SourceService::GetFolder (foundPath));
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





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetNextMacroLevel
//
////////////////////////////////////////////////////////////////////////////////

int SourcePane::GetNextMacroLevel (int level, int deepest)
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
//  SourcePane::GetMacroLevel
//
//  Outside every macro there is only level 0. Without the places between,
//  the body line is level 1.
//
////////////////////////////////////////////////////////////////////////////////

int SourcePane::GetMacroLevel (const DebuggerViewSnapshot::SourceState & state, int level)
{
    int  deepest = state.places.empty() ? 1 : (int) state.places.size() - 1;



    if (state.depth == 0)
    {
        return 0;
    }

    return (level < 0 || level > deepest) ? deepest : level;
}
