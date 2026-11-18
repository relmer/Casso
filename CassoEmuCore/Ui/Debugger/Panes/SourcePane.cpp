#include "Pch.h"

#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/BreakpointActions.h"
#include "Ui/Debugger/CodeMargins.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/Source/SourcePathList.h"
#include "Debugger/SymbolDescriptions.h"
#include "CLexer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::SourcePane
//
////////////////////////////////////////////////////////////////////////////////

SourcePane::SourcePane (DxuiTextView * view, DxuiInfoBanner * banner, FindFn find, RunFn run) :
    m_view   (view),
    m_banner (banner),
    m_find   (std::move (find)),
    m_run    (std::move (run))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::Configure
//
//  The glyph margin and the heat strip after it are the disassembly's
//  (CodeMargins): the same widths, from the view's left edge.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::Configure (HWND hwnd)
{
    m_view->SetOwnerWindow        (hwnd);
    m_view->SetNoWrap             (true);
    m_view->SetColumnCap          (kTextCell, kAnnotationColumn);
    m_view->SetColumnGap          (kNumberCell, kArrowGapCells);
    m_view->SetFirstClippedColumn (kAnnotationCell);
    m_view->SetGutter             (CodeMargins::kGlyphDip, CodeMargins::kIconDip);
    m_view->SetGlyphCenterDip     (CodeMargins::kGlyphCenterDip);
    m_view->SetGutterBand         (CodeMargins::kGlyphDip, CodeMargins::kHeatDip);
    m_view->SetRowFocus           (true);
    m_banner->SetSeverity         (DxuiInfoBanner::Severity::Info);
    m_banner->SetOnAction         ([this] (size_t)
    {
        if (m_isBrowseShown)
        {
            if (m_onBrowse)
            {
                m_onBrowse();
            }
        }
        else if (!m_isLoose)
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
//  SourcePane::SetHeat
//
//  While heat is on, the gutter is wider by the heat strip.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::SetHeat (const std::optional<HeatMapView::Palette> & palette)
{
    if (palette.has_value() != m_heatPalette.has_value())
    {
        m_isStyleStale = true;
    }

    m_heatPalette = palette;
    m_view->SetGutter (CodeMargins::kGlyphDip + (palette.has_value() ? CodeMargins::kHeatDip : 0), CodeMargins::kIconDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::AddHeatStrips
//
//  One fill of the hottest byte's color in the gutter's heat band, as the
//  disassembly fills its heat cell. A line without code, whose bytes nothing
//  touched, or whose bytes in memory are other code's now, has no fill.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::AddHeatStrips (std::vector<DxuiTextView::Row> & rows, int fileId,
                                const std::map<std::pair<int, int>, std::pair<Word, Word>> & lineRanges,
                                const DebuggerViewSnapshot::HeatColumnLevels & levels, const HeatMapView::Palette & palette,
                                const std::set<std::pair<int, int>> * unmatched)
{
    for (size_t i = 0; i < rows.size(); i++)
    {
        auto  found = lineRanges.find ({ fileId, (int) i + 1 });

        if (found != lineRanges.end() && (unmatched == nullptr || !unmatched->contains ({ fileId, (int) i + 1 })))
        {
            rows[i].gutterFill = HeatStrip::Get (levels, found->second.first, (size_t) (found->second.second - found->second.first) + 1, palette, false).hottest;
        }
    }
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



    m_heatLevels = snapshot.heatColumns;

    m_bookmarkAddresses.clear();

    for (const DebuggerViewSnapshot::WatchLine & bookmark : snapshot.bookmarks)
    {
        m_bookmarkAddresses.insert (bookmark.address);
    }

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
//  The line is the view's active row, as a click on it makes it.
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::SetTopSourceLine (int line)
{
    int  first = (line > 0) ? m_view->GetFirstLineOfRow (GetRowOfLine (line)) : -1;



    m_pendingTopLine = (first < 0) ? line : 0;

    if (first >= 0)
    {
        m_view->SetTopLine  (first);
        m_view->SetFocusRow (GetRowOfLine (line));
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
    std::string   name;



    for (const DebugSourceFile & record : m_state->files)
    {
        if (record.id == fileId)
        {
            lookup = m_find (record, m_state->debugFilePath, m_state->programKey);
            name   = record.name;
        }
    }

    m_fileId    = fileId;
    m_match     = lookup.match;
    m_isDropped = false;
    m_foundPath = lookup.path;
    m_lines     = SplitLines (lookup.text);
    m_detected  = SourceSyntax::DetectAssembler (m_lines);
    m_listing   = SourceSyntax::DetectListing (m_lines);
    m_isC       = CLexer::IsCFile (lookup.path.empty() ? SourcePathList::Utf8ToWide (name) : lookup.path);
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
    std::set<int>                         bookmarks;
    bool                                  isRowsStale  = false;
    int                                   top          = m_view->GetTopLine();
    int                                   depth        = 0;
    int                                   level        = SourceBannerText::GetMacroLevel (*m_state, m_macroLevel);
    bool                                  holdsPc      = false;
    bool                                  isBrowse     = false;
    std::pair<int, int>                   shown        = SourceBannerText::GetPlace (*m_state, (level > 0) ? level : -1);
    std::pair<int, int>                   invokedBy    = (level > 0) ? SourceBannerText::GetPlace (*m_state, level - 1) : std::pair<int, int> { -1, 0 };
    std::string                           invokedName;
    std::wstring                          bannerText;
    std::wstring                          elsewhere;
    std::vector<DxuiTextView::Row>        rows;
    auto                                  heat         = (m_heatPalette.has_value() && !m_isLoose && m_state->lineRanges != nullptr) ? m_heatLevels : nullptr;
    std::set<std::pair<int, int>>         unmatched    = (m_state->unmatchedLines != nullptr) ? *m_state->unmatchedLines : std::set<std::pair<int, int>>();



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

    //  A bookmark is on the line whose code starts at its address.
    if (!m_bookmarkAddresses.empty() && !m_isLoose && m_bookmarkMark != nullptr && m_state->lineAddresses != nullptr)
    {
        for (auto each = m_state->lineAddresses->lower_bound ({ m_fileId, 0 }); each != m_state->lineAddresses->end() && each->first.first == m_fileId; ++each)
        {
            if (m_bookmarkAddresses.contains (each->second))
            {
                bookmarks.insert (each->first.second);
            }
        }
    }

    isRowsStale = m_rowsFileId != m_fileId || m_rowsLine != marked || m_rowsBreakpoints != breakpoints || m_rowsBookmarks != bookmarks ||
                  m_rowsDisabled != disabled || m_isStyleStale || m_rowsLineOperands != m_state->lineOperands || m_rowsHeatLevels != heat ||
                  m_rowsHover != m_hoverLine || (heat != nullptr && m_rowsUnmatched != unmatched);

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

        rows = BuildRows (m_lines, marked, breakpoints, disabled, m_style, lineOperands, GetAssembler(), m_listing, m_isC, m_hoverLine);

        //  The bookmark's outline over the dot's place in each such line's
        //  glyph margin; a row is a line, in order.
        for (int line : bookmarks)
        {
            if (line >= 1 && line <= (int) rows.size())
            {
                DxuiTextView::Row  & row = rows[(size_t) line - 1];

                row.icon = (row.icon != nullptr) ? std::shared_ptr<const DxuiIconImage> (GutterGlyph::MakeOver (*row.icon, *m_bookmarkMark)) : m_bookmarkMark;
            }
        }

        if (heat != nullptr)
        {
            AddHeatStrips (rows, m_fileId, *m_state->lineRanges, *heat, *m_heatPalette, m_state->unmatchedLines.get());
        }

        //  For the same file the selection is left in place, so a drag across
        //  it and a copy survive a snapshot that changes only marks or colors.
        if (m_rowsFileId == m_fileId)
        {
            m_view->UpdateRows (std::move (rows));
        }
        else
        {
            m_view->SetRows (std::move (rows));
        }

        m_rowsHeatLevels   = heat;
        m_rowsUnmatched    = std::move (unmatched);
        m_rowLines         = GetRowLines (m_view->GetRows());
        m_rowsDisabled     = disabled;
        m_rowsLineOperands = m_state->lineOperands;
        m_rowsHover        = m_hoverLine;
        m_isStyleStale = false;
        m_view->SetTopLine (top);

        if (isNewLine && marked > 0)
        {
            ScrollTo (marked);
        }

        m_rowsFileId      = m_fileId;
        m_rowsLine        = marked;
        m_rowsBreakpoints = std::move (breakpoints);
        m_rowsBookmarks   = std::move (bookmarks);
    }

    if (m_isLoose)
    {
        ShowLooseBanner();
        return;
    }

    //  Only the document the PC is in speaks of the macro: the invocation's,
    //  or the shown level's while a body is shown.
    holdsPc = m_fileId >= 0 && (m_fileId == m_state->fileId || (level > 0 && m_fileId == shown.first));
    depth   = (holdsPc && SourceBannerText::CanShowBody (!m_lines.empty(), m_state->depth, m_state->bodyFileId, m_fileId)) ? m_state->depth : 0;

    //  A level's invocation in the same file goes by its line alone.
    if (invokedBy.first >= 0 && invokedBy.first != shown.first)
    {
        invokedName = GetFileName (invokedBy.first);
    }

    bannerText = SourceBannerText::GetBannerText (m_match, GetFileName (m_fileId), !m_lines.empty(), depth, level > 0,
                                                  GetFileName (shown.first), shown.second, invokedName, invokedBy.second);
    elsewhere  = m_isDropped ? std::wstring() : SourceBannerText::GetFoundElsewhereText (GetFileName (m_fileId), m_state->debugFilePath, m_foundPath, m_match == SourceMatch::Exact);

    if (!elsewhere.empty())
    {
        bannerText = bannerText.empty() ? elsewhere : elsewhere + L"\n" + bannerText;
    }

    if (m_state->isStale)
    {
        bannerText = bannerText.empty() ? SourceBannerText::GetStaleText (m_state->debugFilePath) : SourceBannerText::GetStaleText (m_state->debugFilePath) + L"\n" + bannerText;
    }

    m_banner->SetText (bannerText);

    //  The banner action: Browse... for a file not found, or for a copy from
    //  another folder whose hash isn't the debug file's, so another copy can
    //  be picked; or the macro's body.
    isBrowse = depth == 0 && m_fileId >= 0 && m_onBrowse &&
               ((m_match == SourceMatch::NotFound && m_lines.empty()) || (!elsewhere.empty() && m_match != SourceMatch::Exact));

    if ((depth > 0 || isBrowse) && m_banner->GetAction (0) == nullptr)
    {
        m_banner->SetActions ({ L"Show body" });
    }
    else if (depth == 0 && !isBrowse && m_banner->GetAction (0) != nullptr)
    {
        m_banner->SetActions ({});
    }

    m_isBrowseShown = isBrowse;

    //  Relabeled rather than replaced: this can run inside the button's own
    //  click.
    if (m_banner->GetAction (0) != nullptr)
    {
        m_banner->GetAction (0)->SetLabel (isBrowse ? L"Browse..." : level > 0 ? L"Show invocation" : L"Show body");
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
    int  row   = -1;
    int  start = 0;



    if (fileId != m_fileId || line <= 0)
    {
        return;
    }

    ScrollTo (line);

    //  The line's text is selected, after the marker and number cells.
    row = GetRowOfLine (line);

    if (row >= 0 && row < (int) m_view->GetRows().size() && m_view->GetRows()[(size_t) row].cells.size() > (size_t) kTextCell)
    {
        const DxuiTextView::Row  & each = m_view->GetRows()[(size_t) row];



        start = (int) (each.cells[0].size() + 1 + each.cells[1].size() + 1);
        m_view->Select ({ row, start }, { row, start + (int) each.cells[(size_t) kTextCell].size() });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::ShowArrowTarget
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::ShowArrowTarget()
{
    std::optional<int>  targetLine;
    bool                isBelow = true;



    if (!m_state.has_value() || !GetArrowTarget (*m_state, m_fileId, m_rowsLine, targetLine, isBelow) || !targetLine.has_value())
    {
        return false;
    }

    ScrollTo (*targetLine);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::ShowMarkedLine
//
////////////////////////////////////////////////////////////////////////////////

void SourcePane::ShowMarkedLine()
{
    if (m_rowsLine > 0)
    {
        ScrollTo (m_rowsLine);
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
//  SourcePane::TryGetHeatTip
//
//  Over the gutter's heat band, on a line whose bytes have heat.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::TryGetHeatTip (POINT atDip, HeatStrip::Tip & tip, RECT & anchor) const
{
    DxuiTextView::Position           at    = m_view->HitTest (atDip);
    const std::pair<Word, Word>    * range = nullptr;
    int                              side  = 0;



    if (m_rowsHeatLevels == nullptr || !m_state.has_value() || m_state->lineRanges == nullptr || !m_view->IsInGutterBand (atDip) ||
        at.row < 0 || at.row >= (int) m_view->GetRows().size() || !m_view->GetRowSide (at.row, side) || side != 0 ||
        !m_state->lineRanges->contains ({ m_fileId, at.row + 1 }) ||
        (m_state->unmatchedLines != nullptr && m_state->unmatchedLines->contains ({ m_fileId, at.row + 1 })))
    {
        return false;
    }

    range  = &m_state->lineRanges->at ({ m_fileId, at.row + 1 });
    tip    = HeatStrip::GetTip (*m_rowsHeatLevels, range->first, (size_t) (range->second - range->first) + 1, false);
    anchor = RECT { atDip.x, atDip.y, atDip.x + 1, atDip.y + 1 };

    return !tip.text.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetAddressAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> SourcePane::GetAddressAt (POINT atDip) const
{
    std::optional<int>  line = GetLineAt (atDip);
    auto                map  = (m_state.has_value()) ? m_state->lineAddresses : nullptr;



    if (!line.has_value() || map == nullptr || !map->contains ({ m_fileId, *line }))
    {
        return std::nullopt;
    }

    return map->at ({ m_fileId, *line });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetFocusAddress
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> SourcePane::GetFocusAddress() const
{
    int   row  = m_view->GetFocusRow();
    auto  map  = (m_state.has_value()) ? m_state->lineAddresses : nullptr;
    int   line = (row >= 0 && row < (int) m_rowLines.size()) ? m_rowLines[(size_t) row] : 0;



    if (m_fileId < 0 || line <= 0 || map == nullptr || !map->contains ({ m_fileId, line }))
    {
        return std::nullopt;
    }

    return map->at ({ m_fileId, line });
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
    if (line.has_value() && HasCode (*line) && GetToggleAction (*m_state, file, *line, CommandMode::AppleWin).has_value())
    {
        m_run ([this, file, at = *line] (CommandMode mode) { return *GetToggleAction (*m_state, file, at, mode); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::HasCode
//
//  Whether the debug file's line table maps a line of this file to an
//  address, which is where a breakpoint can go.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::HasCode (int line) const
{
    return m_state.has_value() && m_state->lineAddresses != nullptr && m_state->lineAddresses->contains ({ m_fileId, line });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::SetGutterHover
//
//  Rows are built again only when the line under the pointer changes.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::SetGutterHover (POINT atDip)
{
    std::optional<int>  line  = m_view->IsInGutter (atDip) ? GetLineAt (atDip) : std::nullopt;
    int                 hover = (line.has_value() && HasCode (*line)) ? *line : -1;



    if (hover == m_hoverLine)
    {
        return false;
    }

    m_hoverLine = hover;

    if (m_state.has_value())
    {
        Rebuild();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::ClickGutter
//
//  A press anywhere on the margin is the margin's, so it doesn't start a
//  selection in the text; on a line with no code it does nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::ClickGutter (POINT atDip)
{
    std::optional<int>  line = GetLineAt (atDip);
    int                 file = m_fileId;



    if (!m_view->IsInGutter (atDip))
    {
        return false;
    }

    if (line.has_value() && HasCode (*line) && GetToggleAction (*m_state, file, *line, CommandMode::AppleWin).has_value())
    {
        m_run ([this, file, at = *line] (CommandMode mode) { return *GetToggleAction (*m_state, file, at, mode); });
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::TryGetLineBreakpoint
//
//  A line with an enabled breakpoint and a disabled one reads as the
//  enabled one, as its glyph does.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::TryGetLineBreakpoint (POINT atDip, int & line, std::optional<LineBreakpoint> & breakpoint) const
{
    std::optional<int>  at = GetLineAt (atDip);



    breakpoint.reset();

    if (!at.has_value() || !HasCode (*at))
    {
        return false;
    }

    line = *at;

    for (const std::tuple<int, int, int> & each : m_state->breakpointLines)
    {
        bool  isEnabled = !m_disabledIds.contains (std::get<2> (each));

        if (std::get<0> (each) == m_fileId && std::get<1> (each) == line && (!breakpoint.has_value() || (isEnabled && !breakpoint->isEnabled)))
        {
            breakpoint = LineBreakpoint { std::get<2> (each), isEnabled };
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetSetBreakpointAction
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerAction> SourcePane::GetSetBreakpointAction (int line, CommandMode mode) const
{
    if (!HasCode (line))
    {
        return std::nullopt;
    }

    for (const DebugSourceFile & record : m_state->files)
    {
        if (record.id == m_fileId)
        {
            return BreakpointActions::GetSourceBreakpoint (record.name, line, mode);
        }
    }

    return std::nullopt;
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
    m_isC       = CLexer::IsCFile (lookup.path);
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
    m_isC           = CLexer::IsCFile (path);
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
    return SourceBannerText::GetPlace (*m_state, SourceBannerText::GetMacroLevel (*m_state, m_macroLevel)).first;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetShownLine
//
////////////////////////////////////////////////////////////////////////////////

int SourcePane::GetShownLine() const
{
    return SourceBannerText::GetPlace (*m_state, SourceBannerText::GetMacroLevel (*m_state, m_macroLevel)).second;
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
//  Lines ending in LF, CR LF or CR, with tabs expanded to every fourth
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
//  line to that edge of the lines. The marked line out of view enters from
//  its edge while the line runs on across the lines toward the target.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::GetBranchArrow (BranchArrow::Input & input, Word & goesTo, bool & isTaken) const
{
    static constexpr int  s_kTextCell   = 2;
    std::optional<int>    targetLine;
    bool                  isBelow       = true;
    int                   row           = GetRowOfLine (m_rowsLine);
    int                   side          = 0;
    int                   targetSide    = 0;
    float                 x             = 0.0f;
    float                 y             = 0.0f;
    float                 top           = 0.0f;
    float                 bottom        = 0.0f;
    float                 scale         = 1.0f;
    RECT                  bounds        = {};



    if (!IsActive() || !m_view->IsVisible() ||
        !GetArrowTarget (*m_state, m_fileId, m_rowsLine, targetLine, isBelow) ||
        !m_view->GetRowSide (row, side) || !m_view->GetCellLeftPx (row, s_kTextCell, x))
    {
        return false;
    }

    //  Out of view, the marked line counts only while the arrow runs on
    //  across the lines: above them toward a target below, or below toward
    //  one above, and not to a target beyond the same edge.
    if (side != 0)
    {
        if ((side < 0) != isBelow ||
            (targetLine.has_value() && m_view->GetRowSide (GetRowOfLine (*targetLine), targetSide) && targetSide == side))
        {
            return false;
        }
    }
    else if (!m_view->GetCellAnchorPx (row, s_kTextCell, x, y))
    {
        return false;
    }

    goesTo  = *m_state->pcTarget;
    isTaken = m_state->isPcTargetTaken;
    bounds  = m_view->GetBounds();
    scale   = m_view->GetPxPerDip();

    m_view->GetLinesSpanPx (top, bottom);

    input.mnemonicX     = x;
    input.sourceY       = (side == 0) ? std::optional<float> (y) : std::nullopt;
    input.isTargetBelow = isBelow;
    input.edgeY         = isBelow ? bottom : top;
    input.sourceEdgeY   = isBelow ? top : bottom;
    input.marginPx      = kArrowMarginDip * scale;
    input.stubPx       *= scale;
    input.radiusPx     *= scale;
    input.headPx       *= scale;
    input.slopPx       *= scale;

    if (targetLine.has_value() && m_view->GetCellAnchorPx (GetRowOfLine (*targetLine), s_kTextCell, x, y))
    {
        input.targetY = y;
    }

    return input.mnemonicX - input.marginPx - input.stubPx >= (float) bounds.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::IsOverArrowLanes
//
//  The lanes span the arrow's distance left of the text, with a second
//  arrow a lane further out.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::IsOverArrowLanes (POINT atDip) const
{
    const float              kLanesDip = kArrowMarginDip + BranchArrow::Input().stubPx + DisassemblyCells::kArrowLaneDip;
    DxuiTextView::Position   at        = m_view->HitTest (atDip);
    int                      side      = 0;
    float                    x         = 0.0f;



    if (!DxuiDockSite::Contains (m_view->GetBounds(), atDip) || at.row < 0 || at.row >= (int) m_view->GetRows().size() ||
        !m_view->GetRowSide (at.row, side) || side != 0 || !m_view->GetCellLeftPx (at.row, kTextCell, x))
    {
        return false;
    }

    return (float) atDip.x >= x - kLanesDip * m_view->GetPxPerDip() && (float) atDip.x < x;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::FindSymbolAt
//
//  The symbol under a point, in the text cell after the marker and number.
//
////////////////////////////////////////////////////////////////////////////////

const SymbolInfo * SourcePane::FindSymbolAt (POINT atDip, const std::vector<SymbolInfo> & symbols) const
{
    DxuiTextView::Position     at    = m_view->HitTest (atDip);
    const DxuiTextView::Row  * row   = nullptr;
    int                        start = 0;
    int                        word  = 0;



    if (at.row < 0 || at.row >= (int) m_view->GetRows().size())
    {
        return nullptr;
    }

    row = &m_view->GetRows()[(size_t) at.row];

    if (row->cells.size() <= (size_t) kTextCell)
    {
        return nullptr;
    }

    start = (int) (row->cells[0].size() + 1 + row->cells[1].size() + 1);

    if (at.offset < start || at.offset > start + (int) row->cells[(size_t) kTextCell].size())
    {
        return nullptr;
    }

    return FindSymbolAt (row->cells[(size_t) kTextCell], at.offset - start, symbols, word);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::GetWordAt
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourcePane::GetWordAt (POINT atDip) const
{
    DxuiTextView::Position     at    = m_view->HitTest (atDip);
    const DxuiTextView::Row  * row   = nullptr;
    int                        start = 0;



    if (at.row < 0 || at.row >= (int) m_view->GetRows().size())
    {
        return {};
    }

    row = &m_view->GetRows()[(size_t) at.row];

    if (row->cells.size() <= (size_t) kTextCell)
    {
        return {};
    }

    start = (int) (row->cells[0].size() + 1 + row->cells[1].size() + 1);

    if (at.offset < start || at.offset > start + (int) row->cells[(size_t) kTextCell].size())
    {
        return {};
    }

    return GetWordAt (row->cells[(size_t) kTextCell], at.offset - start);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::FindMacroLine
//
//  A row's second cell is its line's number.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int> SourcePane::FindMacroLine (const std::wstring & name) const
{
    int  number = 0;



    if (name.empty())
    {
        return std::nullopt;
    }

    for (const DxuiTextView::Row & row : m_view->GetRows())
    {
        if (row.cells.size() <= (size_t) kTextCell || !SourceLineText::IsMacroDefinition (row.cells[(size_t) kTextCell], name))
        {
            continue;
        }

        number = _wtoi (row.cells[1].c_str());

        if (number > 0)
        {
            return number;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::TryGetSymbolTip
//
//  The symbol under the pointer, in the text cell after the marker and number.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::TryGetSymbolTip (POINT atDip, const std::vector<SymbolInfo> & symbols, std::wstring & text, RECT & anchor,
                                  const SymbolInfo ** found) const
{
    DxuiTextView::Position     at    = m_view->HitTest (atDip);
    const DxuiTextView::Row  * row   = nullptr;
    int                        start = 0;



    if (at.row < 0 || at.row >= (int) m_view->GetRows().size())
    {
        return false;
    }

    row = &m_view->GetRows()[(size_t) at.row];

    if (row->cells.size() <= (size_t) kTextCell)
    {
        return false;
    }

    start = (int) (row->cells[0].size() + 1 + row->cells[1].size() + 1);

    if (at.offset < start || at.offset > start + (int) row->cells[(size_t) kTextCell].size() ||
        !SourceLineText::TryGetSymbolText (row->cells[(size_t) kTextCell], at.offset - start, symbols, text, found))
    {
        return false;
    }

    anchor = RECT { atDip.x, atDip.y, atDip.x + 1, atDip.y + 1 };
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePane::IsOperandSymbolAt
//
//  The word under the pointer, in the text cell after the marker and number,
//  against the operand's symbol of the code past the line's label.
//
////////////////////////////////////////////////////////////////////////////////

bool SourcePane::IsOperandSymbolAt (POINT atDip, const std::vector<SymbolInfo> & symbols) const
{
    DxuiTextView::Position     at    = m_view->HitTest (atDip);
    const DxuiTextView::Row  * row   = nullptr;
    int                        start = 0;



    if (at.row < 0 || at.row >= (int) m_view->GetRows().size())
    {
        return false;
    }

    row = &m_view->GetRows()[(size_t) at.row];

    if (row->cells.size() <= (size_t) kTextCell)
    {
        return false;
    }

    start = (int) (row->cells[0].size() + 1 + row->cells[1].size() + 1);

    return at.offset >= start && at.offset <= start + (int) row->cells[(size_t) kTextCell].size() &&
           IsOperandSymbolAt (row->cells[(size_t) kTextCell], SourceLineText::FindCodeStart (row->cells[(size_t) kTextCell]), at.offset - start, symbols);
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
                                                      SourceSyntax::Listing   listing,
                                                      bool                    isC,
                                                      int                     hoverLine)
{
    std::vector<DxuiTextView::Row>  rows;
    int                             width       = (int) std::to_wstring (lines.size()).size();
    bool                            icons       = style.enabledIcon != nullptr;
    bool                            arrow       = style.pcIcon != nullptr;
    bool                            isInComment = false;



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
        else if (number == hoverLine)
        {
            row.icon = style.hoverIcon;
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

        //  What the line's instruction reads in the operand color, then the
        //  result in the result color, as in the disassembly pane's operand
        //  column.
        //  Only on a line whose opcode is a mnemonic: a directive or a macro
        //  produced its bytes some other way than one instruction.
        if (!lineOperands.empty())
        {
            auto  found  = lineOperands.find (number);
            int   column = (int) row.cells.size();

            row.cells.emplace_back();

            if (found != lineOperands.end() && SourceLineText::IsInstructionLine (lines[i], assembler))
            {
                row.cells.back() = found->second.first;

                if (style.operandArgb != 0 && !row.cells.back().empty())
                {
                    row.spans.push_back ({ column, 0, (int) row.cells.back().size(), style.operandArgb });
                }

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

        //  A C block comment can run over lines, so every C line is read in
        //  order, colored or not.
        if (isC)
        {
            std::vector<SourceSyntax::Run>  runs = SourceSyntax::GetCRuns (lines[i], isInComment);

            for (const SourceSyntax::Run & run : runs)
            {
                if (style.syntax.mnemonic != 0)
                {
                    row.spans.push_back ({ 2, run.start, run.length, style.syntax.Get (run.token) });
                }
            }
        }
        else if (style.syntax.mnemonic != 0)
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
            return BreakpointActions::GetClearBreakpoint (std::get<2> (bp), mode);
        }
    }

    for (const DebugSourceFile & record : state.files)
    {
        if (record.id == fileId)
        {
            return BreakpointActions::GetSourceBreakpoint (record.name, line, mode);
        }
    }

    return std::nullopt;
}
