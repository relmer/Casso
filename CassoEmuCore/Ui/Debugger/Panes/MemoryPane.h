#pragma once

#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Ui/Debugger/Panes/MemoryEditModel.h"
#include "Ui/Debugger/DebuggerActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane
//
//  One memory window (FR-034): a hex view over this window's edit model,
//  editable in place, grouped by one, two or four bytes, with its own undo.
//
//  THE VIEW SCROLLS THE WHOLE 64K; THE SNAPSHOT HOLDS A WINDOW OF IT. When the
//  rows on screen leave the bytes the CPU thread last read for this window,
//  the pane asks for a read around them. Until it arrives, rows outside the
//  read show what an earlier read left there, with its colors, or zero in the
//  muted color where no read has reached.
//
//  The window owns the view as a child control; the pane holds a pointer to
//  it and the model it reads from.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryPane : public IDxuiFindTarget
{
public:
    using  MoveFn   = std::function<void (int id, Word first)>;
    using  RunFn    = std::function<void (const std::string & line)>;
    using  ActionFn = std::function<void (const DebuggerActionBuilder & build)>;

    MemoryPane (int id, DxuiHexView * view, MoveFn move, ActionFn run, RunFn note);

    //  The view holds a pointer to this pane's model, so a pane never moves.
    MemoryPane (const MemoryPane &)             = delete;
    MemoryPane & operator= (const MemoryPane &) = delete;

    int            GetId       () const { return m_id; }
    DxuiHexView  * GetView     () const { return m_view; }
    int            GetGrouping () const { return m_grouping; }
    int            GetColumns  () const { return m_columns; }

    //  The address of the first byte on screen.
    Word           GetTopAddress () const { return m_model.GetAddressOf (m_view->GetTopRow() * (uint64_t) m_view->GetBytesPerRow()); }

    void  Configure (HWND hwnd);

    //  The inks for a byte that changed since the previous snapshot, a ROM
    //  byte and an I/O byte, and the outlines' colors, from the theme's text
    //  colors.
    void  SetTextColors (const DebuggerTextColors::Set & colors);

    //  Heat on the window's bar, and the levels and colors each byte is
    //  tinted by while it is on.
    bool  IsHeatOn  () const   { return m_isHeatOn; }
    void  SetHeatOn (bool isOn) { m_isHeatOn = isOn; }
    void  SetHeat   (std::shared_ptr<const DebuggerViewSnapshot::HeatColumnLevels> levels, const HeatMapView::Palette & palette);

    //  A byte's tint: its heat map color mixed halfway toward the page, or
    //  0 for a byte nothing touched.
    static uint32_t  GetHeatFill (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word address, const HeatMapView::Palette & palette);

    //  While heat is on, a byte's counts and the scale its tint is from, as a
    //  heat cell's tip has them; empty while it is off.
    HeatStrip::Tip  GetHeatTip (uint64_t offset) const;

    //  Whether the machine is stopped, which holds the changed marks.
    void  SetPaused (bool isPaused) { m_model.SetPaused (isPaused); }

    //  The card in each slot, which labels the slot's outlined ranges, and
    //  the model's directory name, which labels the parts of its ROM.
    void  SetSlotCards (const MemoryRegionKeys::SlotCards & cards) { m_model.SetSlotCards (cards); }
    void  SetMachineId (const std::string & machineId)             { m_model.SetMachineId (machineId); }
    void  SetSymbols   (std::shared_ptr<const std::vector<SymbolInfo>> symbols) { m_model.SetSymbols (std::move (symbols)); }

    //  The bytes the snapshot read for this window. The first one places the
    //  view there; later ones leave the view where the user has it.
    void  Apply (const DebuggerViewSnapshot::MemoryWindow & window);

    //  Asks for a read around the rows on screen once they leave the last one.
    void  FollowScroll ();

    //  Scrolls to an address and asks for a read there.
    void  GoTo (Word address);

    //  The grouping and columns the hex view's menu set.
    void  SyncFromView  ();

    //  One, two or four bytes a value.
    void  SetGrouping (int bytesPerValue);

    //  Values across a row, or 0 for as many as the pane's width holds.
    void  SetColumns (int valuesPerRow);

    //  Asks again for the bytes the window last read.
    void  Refresh ();

    //  The 64K the find box searches, from the latest snapshot.
    void  SetMemoryImage (std::shared_ptr<const DebugMemoryImage> image) { m_image = std::move (image); }

    //  Where a find's needle is in memory: bytes, as "A9 00" or "$A9 $00",
    //  or else text, matched by its low seven bits as the screen shows it.
    //  outLength is a hit's length in bytes.
    static std::vector<Word>  FindHits (const DebugMemoryImage & image, const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, size_t & outLength);

    //  The little-endian word a byte and the byte after it make, as a pointer
    //  there reads; none where either byte wasn't read.
    static std::optional<Word>  GetWordAt (const DebugMemoryImage & image, Word address);

    //  The find box's searches, over all of memory.
    IDxuiControl    * GetFindControl          () override;
    RECT              GetFindBounds           () const override;
    bool              IsFindVisible           () const override;
    std::wstring      GetSelectionText        () const override { return std::wstring(); }
    DxuiFindResult    SelectMatch             (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount) override;
    DxuiFindResult    SelectMatchInView       (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount) override;
    void              SetFindScopeToSelection () override {}
    void              ClearFindScope          () override {}
    void              ClearFindHighlights     () override { m_findHits.clear(); m_findCurrent = -1; SetFindMarks(); }
    void              SetFindColors           (uint32_t matchArgb, uint32_t currentArgb) override { m_matchArgb = matchArgb; m_currentArgb = currentArgb; }

    //  The last find's hits, and the one selected; -1 for none.
    const std::vector<Word> &  GetFindHits    () const { return m_findHits; }
    int                        GetFindCurrent () const { return m_findCurrent; }

    //  The address of the byte under a point, if any.
    std::optional<Word>  GetAddressAt (POINT clientDip) const;

    //  Neither writes over a byte that changed since the step; a line with
    //  that byte's address goes to the console instead.
    bool  Undo         () { return UndoOrRedo (false); }
    bool  Redo         () { return UndoOrRedo (true); }
    bool  CanUndo      () const { return m_view->IsEditable() && m_model.CanUndo(); }
    bool  CanRedo      () const { return m_view->IsEditable() && m_model.CanRedo(); }
    void  ClearHistory () { m_model.ClearHistory(); }

    std::string  GetHistoryText () const                       { return m_model.GetHistoryText(); }
    void         SetHistoryText (const std::string & text)     { m_model.SetHistoryText (text); }

    //  Compare with...: the differing bytes marked, the other range's values
    //  drawn in their place or not, and the comparison ended.
    void  SetComparison   (const std::vector<CompareDifference> & differences);
    void  ToggleShowOther ()       { m_model.SetShowOther (!m_model.IsShowingOther()); Refresh(); }
    void  ClearComparison ()       { m_model.ClearComparison(); Refresh(); }
    bool  HasComparison   () const { return m_model.HasComparison(); }
    bool  IsShowingOther  () const { return m_model.IsShowingOther(); }

    //  The bytes from an address, selected and brought into view, as a found
    //  match is.
    void  SelectRange     (Word address, size_t count);

    //  The first and last addresses selected; none with nothing selected.
    std::optional<std::pair<Word, Word>>  GetSelectedRange () const
    {
        if (!m_view->HasSelection())
        {
            return std::nullopt;
        }

        return std::pair<Word, Word> { m_model.GetAddressOf (m_view->GetSelectionFirst()), m_model.GetAddressOf (m_view->GetSelectionLast()) };
    }

    //  What Undo and Redo would take back or make again, as "change 2 bytes
    //  at $0300", or empty when there is nothing.
    std::wstring  GetUndoText () const { return CanUndo() ? m_model.GetUndoText() : std::wstring(); }
    std::wstring  GetRedoText () const { return CanRedo() ? m_model.GetRedoText() : std::wstring(); }

    //  Where a read should start to hold the rows on screen, or nothing when
    //  the last read already holds them.
    static std::optional<Word>  GetReadStartFor (Word readFirst, uint64_t topOffset, int visibleRows, int bytesPerRow = kBytesPerRow);

    //  The columns a window starts with: Auto, as many values as the pane's
    //  width holds, refitted as it resizes.
    static constexpr int  kDefaultColumns = 0;

private:
    void  ShowFindHit  (Word address);
    void  SetFindMarks ();

    static constexpr int  kBytesPerRow = DebuggerViewState::kMemoryRowBytes;

    void  NoteRefusal (uint64_t offset) const;
    bool  UndoOrRedo  (bool redo);

    int                    m_id          = 0;
    DxuiHexView          * m_view        = nullptr;
    MemoryEditModel        m_model;
    MoveFn                 m_move;
    RunFn                  m_note;
    Word                   m_readFirst   = 0;
    bool                   m_placed      = false;
    int                    m_grouping    = 1;
    int                    m_columns     = kDefaultColumns;
    std::optional<Word>    m_requested;
    uint32_t               m_changedArgb = 0xFFFF6B68;
    uint32_t               m_romArgb     = 0xFF7FB2E5;
    uint32_t               m_ioArgb      = 0xFF909090;
    uint32_t               m_unreadArgb  = 0xFF707070;
    uint32_t               m_differsArgb = 0xFFE8A317;
    bool                   m_isHeatOn    = true;

    //  The heat the bytes are tinted by while Heat is on.
    std::shared_ptr<const DebuggerViewSnapshot::HeatColumnLevels>  m_heatLevels;
    HeatMapView::Palette                                           m_heatPalette;
    std::shared_ptr<const DebugMemoryImage>                        m_image;
    std::vector<Word>                                              m_findHits;
    size_t                                                         m_findLength  = 0;
    int                                                            m_findCurrent = -1;
    uint32_t                                                       m_matchArgb   = 0;
    uint32_t                                                       m_currentArgb = 0;
};
