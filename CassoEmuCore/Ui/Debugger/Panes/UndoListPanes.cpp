#include "Pch.h"

#include "Ui/Debugger/Panes/UndoListPanes.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/FlagsDialog.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/TextDialog.h"
#include "Ui/Debugger/UndoBarCommands.h"
#include "Ui/Debugger/UndoText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UndoListPane::UndoListPane
//
////////////////////////////////////////////////////////////////////////////////

UndoListPane::UndoListPane (IDebuggerPaneHost & host, std::wstring id, std::wstring title, ColorLegend::Pane legend) :
    IDebuggerPane (host),
    m_id          (id),
    m_title       (std::move (title)),
    m_legend      (legend),
    m_bar         (host, std::move (id), L"Undo commands")
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoListPane::~UndoListPane
//
////////////////////////////////////////////////////////////////////////////////

UndoListPane::~UndoListPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  UndoListPane::CreateControls
//
////////////////////////////////////////////////////////////////////////////////

void UndoListPane::CreateControls()
{
    m_list = m_host.GetControlParent().CreateChild<DxuiListView>();
    m_bar.Create();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoListPane::Configure
//
//  Undo and Redo, each tip quoting the edit it would act on.
//
////////////////////////////////////////////////////////////////////////////////

void UndoListPane::Configure()
{
    UndoBarCommands::Handlers  handlers;



    handlers.dispatch  = [this] (int id) { Undo (id == UndoBarCommands::kRedo); m_host.Repaint(); };
    handlers.isEnabled = [this] (int id) { return CanUndo (id == UndoBarCommands::kRedo); };
    handlers.getTip    = [this] (int id)
    {
        bool  redo = id == UndoBarCommands::kRedo;

        return CanUndo (redo) ? UndoText::GetLabel (redo, GetUndoText (redo)) : std::wstring();
    };

    m_commands = std::make_unique<UndoBarCommands> (std::move (handlers));

    m_bar.Configure();
    m_bar.GetBar()->SetEntries (m_commands->BuildEntries());

    m_list->SetActivateOnDoubleClick (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoListPane::AddToDock
//
//  The bar, a place PlaceBar fills, over the list.
//
////////////////////////////////////////////////////////////////////////////////

void UndoListPane::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (m_title);
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    AddMiddleParts (*m_frame);
    m_frame->AddPart (m_list);

    dock.AddPane (m_id, m_title, m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoListPane::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * UndoListPane::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoListPane::GetColorKey
//
////////////////////////////////////////////////////////////////////////////////

std::optional<PaneColorKeySpec> UndoListPane::GetColorKey() const
{
    return PaneColorKeySpec { m_bar.GetSlot(), m_legend, nullptr };
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoListPane::AddUndoItems
//
////////////////////////////////////////////////////////////////////////////////

void UndoListPane::AddUndoItems (PaneMenuItems & items)
{
    if (CanUndo (false))
    {
        items.push_back ({ UndoText::GetLabel (false, GetUndoText (false)), [this] { Undo (false); } });
    }

    if (CanUndo (true))
    {
        items.push_back ({ UndoText::GetLabel (true, GetUndoText (true)), [this] { Undo (true); } });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoListPane::IsPressRightOf
//
//  Whether the last press landed at or right of a cell's text, in a row of
//  the list.
//
////////////////////////////////////////////////////////////////////////////////

bool UndoListPane::IsPressRightOf (int row, size_t column) const
{
    RECT  cell = {};
    RECT  list = m_list->GetBounds();



    return m_list->GetCellTextRectPx (row, column, cell) && m_host.GetLastPressPx().x - list.left >= cell.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::ValueEditPane
//
////////////////////////////////////////////////////////////////////////////////

ValueEditPane::ValueEditPane (IDebuggerPaneHost & host, std::wstring id, std::wstring title, ColorLegend::Pane legend) :
    UndoListPane (host, std::move (id), std::move (title), legend)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::~ValueEditPane
//
////////////////////////////////////////////////////////////////////////////////

ValueEditPane::~ValueEditPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::CreateOverlays
//
//  The value box, drawn over its row.
//
////////////////////////////////////////////////////////////////////////////////

void ValueEditPane::CreateOverlays()
{
    m_editor = m_host.GetControlParent().CreateChild<DxuiTextInput>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ValueEditPane::Configure()
{
    UndoListPane::Configure();

    m_editor->SetVisible   (false);
    m_editor->SetOverText  (true);
    m_editor->SetHwnd      (m_host.GetOwnerHwnd());
    m_editor->SetMaxLength (kEditMaxChars);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> ValueEditPane::GetControls() const
{
    return { m_bar.GetSlot(), m_list, m_editor, m_bar.GetBar() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void ValueEditPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    m_isPaused = snapshot.isPaused;
    m_pc       = snapshot.pc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::BeginEdit
//
//  Only a paused machine takes an edit, and only a byte: PC is moved by
//  command. The box is in the list's own face and size, so the text does
//  not jump when it opens over the row.
//
////////////////////////////////////////////////////////////////////////////////

void ValueEditPane::BeginEdit (int row)
{
    RECT                   cell   = {};
    RECT                   bounds = {};
    std::optional<Byte>    value  = (row >= 0) ? GetEditValue (row) : std::nullopt;



    if (!value.has_value())
    {
        return;
    }

    if (!m_isPaused)
    {
        m_host.AppendConsole ({ GetPauseNote() });
        return;
    }

    if (m_editRow >= 0)
    {
        EndEdit (false);
    }

    if (!m_list->GetCellTextRectPx (row, 1, cell))
    {
        return;
    }

    bounds = m_list->GetBounds();
    OffsetRect (&cell, bounds.left, bounds.top);

    m_editRow = row;

    m_editor->SetTextRenderer (m_host.GetPaneTextRenderer());
    m_editor->SetFont         (DxuiTheme::kMonoFace, m_list->GetFontSizeDip());
    m_editor->SetText         (std::format (L"{:02X}", *value));
    m_editor->Layout          (cell, m_host.GetPaneScaler());
    m_editor->SetVisible      (true);
    m_host.FocusControl       (m_editor);
    m_editor->SelectAll();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::EndEdit
//
//  A click anywhere else commits what was typed, as with a watch. What is
//  not a byte in hex is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void ValueEditPane::EndEdit (bool commit)
{
    int   row    = m_editRow;
    Word  typed  = 0;
    bool  isByte = false;



    if (row < 0)
    {
        return;
    }

    isByte = TextDialog::TryParseHexWord (m_editor->GetText(), typed) && typed <= 0xFF;

    m_editRow = -1;
    m_editor->SetVisible (false);
    m_host.FocusControl  (m_list);
    m_host.Repaint();

    if (commit && isByte)
    {
        CommitEdit (row, (Byte) typed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::RouteEditMouse
//
//  The box being edited takes the pointer while it is over the box; a press
//  or a wheel anywhere else commits the edit and passes the event on.
//
////////////////////////////////////////////////////////////////////////////////

bool ValueEditPane::RouteEditMouse (const DxuiMouseEvent & ev)
{
    if (m_editRow < 0)
    {
        return false;
    }

    if (DxuiDockSite::Contains (m_editor->GetBounds(), ev.positionDip))
    {
        m_editor->OnMouse (ev);
        m_host.Repaint();
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel)
    {
        EndEdit (true);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::RouteEditKey
//
//  The box being edited takes every key: Enter commits what was typed, and
//  Escape cancels the edit.
//
////////////////////////////////////////////////////////////////////////////////

bool ValueEditPane::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    if (m_editRow >= 0)
    {
        if (ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_RETURN || ev.vk == VK_ESCAPE))
        {
            EndEdit (ev.vk == VK_RETURN);
        }
        else
        {
            m_editor->OnKey (ev);
        }

        m_host.Repaint();
        return true;
    }

    return focused == m_list && RouteListKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane::RouteListKey
//
////////////////////////////////////////////////////////////////////////////////

bool ValueEditPane::RouteListKey (const DxuiKeyEvent &)
{
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::RegistersPane
//
////////////////////////////////////////////////////////////////////////////////

RegistersPane::RegistersPane (IDebuggerPaneHost & host) :
    ValueEditPane (host, DebuggerLayout::kRegisters, L"Registers", ColorLegend::Pane::Registers)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::~RegistersPane
//
////////////////////////////////////////////////////////////////////////////////

RegistersPane::~RegistersPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::CreateControls
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::CreateControls()
{
    ValueEditPane::CreateControls();

    m_band = m_host.GetControlParent().CreateChild<HistoryBand>();
    m_band->SetOnGoLive ([this] { m_host.GoLive(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::Configure
//
//  Double-clicking PC shows the PC, as Show Next Statement does; on P's
//  flags it opens their editor, and on any other value it edits the value
//  in place, as a single click on it does.
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::Configure()
{
    ValueEditPane::Configure();

    m_list->SetOnActivateRow ([this] (int row) { EditRegister (row, IsPressRightOf (row, 2)); });

    m_list->SetColumns ({ { L"Reg",   0, false, DxuiTextHAlign::Left },
                          { L"Value", 0, false, DxuiTextHAlign::Left },
                          { L"",      0, false, DxuiTextHAlign::Left } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::OnDocked
//
//  The registers' list has no headings, since each row starts with its
//  register. Hidden once the window has made every list dense, which turns
//  them on.
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::OnDocked()
{
    m_list->SetShowHeader (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::AddMiddleParts
//
//  The history band, shown while the machine is taken back in history.
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::AddMiddleParts (DebuggerPaneFrame & frame)
{
    frame.AddPart (m_band, [this] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (HistoryBand::GetHeightDip (m_host.GetPaneRowDip())); },
                   [this] { return HistoryBand::IsShown (m_band->GetStatus()); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::Apply
//
//  THE FLAGS ARE THE P REGISTER written so a person can read it, so they sit
//  on P's row beside the byte they come from, in the same monospace face,
//  where a bit changing moves nothing else. The cycle count's row runs across
//  the pane, its digits grouped, with how far it moved since the stop before
//  after it, signed, so a step back reads as one. Spanning, it sets no
//  column's width, so a long count never pushes the flags along.
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    const StopChanges                             & changes  = m_host.GetStopChanges();
    uint32_t                                        changed  = m_host.GetTextColors().changed;
    std::vector<std::vector<DxuiListView::Cell>>    rows;
    bool                                            wasShown = HistoryBand::IsShown (m_band->GetStatus());



    ValueEditPane::Apply (snapshot);

    m_registers      = snapshot.registers;
    m_lastBranchFrom = snapshot.lastBranchFrom;
    m_lastBranchTo   = snapshot.lastBranchTo;

    for (const DebuggerViewSnapshot::RegisterRow & reg : snapshot.registers)
    {
        bool                isCycles = reg.name == DebuggerViewState::kCyclesRegister;
        bool                isCount  = isCycles || reg.name == DebuggerViewState::kTripRegister;
        bool                isBranch = reg.name == DebuggerViewState::kLastBranchRegister;
        DxuiListView::Cell  value    = { TextEncoding::NarrowToWide (reg.value) };
        DxuiListView::Cell  flags    = { reg.name == "P" ? L"Flags: " + TextEncoding::NarrowToWide (snapshot.flags) : L"" };
        std::wstring        delta;
        uint64_t            count    = 0;

        //  The flags in the fixed-width face, so each letter and dot has a
        //  column of its own in a list otherwise in the proportional face.
        flags.face = DxuiTheme::kMonoFace;

        if (isCount)
        {
            delta = isCycles ? TextEncoding::NarrowToWide (changes.GetDelta ("R:" + reg.name)) : std::wstring();

            if (std::from_chars (reg.value.data(), reg.value.data() + reg.value.size(), count).ec == std::errc())
            {
                value.text = DebuggerStatusText::FormatCount (count, LOCALE_NAME_USER_DEFAULT);
            }

            value.text     += delta.empty() ? std::wstring() : L"  (" + delta + L")";
            value.spansRow  = true;
        }

        //  From and to, as Visual Studio's Registers window has its last
        //  branch; Go to is on the row's menu.
        if (isBranch)
        {
            value.text     = !snapshot.lastBranchFrom.has_value() ? L"None" :
                             std::format (L"${:04X} {} {}", *snapshot.lastBranchFrom, s_kpszRightArrow,
                                          snapshot.lastBranchTo.has_value() ? std::format (L"${:04X}", *snapshot.lastBranchTo) : std::wstring (L"?"));
            value.spansRow = true;
        }

        //  On P's row only the flags that changed are in the changed color.
        if (!isCount && !isBranch && changes.IsChanged ("R:" + reg.name))
        {
            value.argb = changed;
            value.tip  = ColorLegend::GetText (ColorLegend::Meaning::Changed);
            flags.tip  = value.tip;
        }

        if (reg.name == "P")
        {
            AddChangedFlagRanges (changes.GetChangedBits ("R:P"), flags);
        }

        rows.push_back ({ { TextEncoding::NarrowToWide (reg.name) }, value, flags });
    }

    m_list->SetRows (std::move (rows));

    m_history.OnSnapshot (snapshot.isPaused, snapshot.pc);

    //  The band follows the snapshot's place in history, and the pane is
    //  laid out again when the band appears or goes.
    m_band->SetStatus (snapshot.history);

    if (wasShown != HistoryBand::IsShown (snapshot.history) && m_frame != nullptr)
    {
        m_frame->Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::AddChangedFlagRanges
//
//  The changed color on each flag letter at the end of P's flags whose bit
//  is set in changedBits. The flags are the last eight letters of the cell,
//  N first, so bit 7 is the first of them.
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::AddChangedFlagRanges (uint32_t changedBits, DxuiListView::Cell & flags) const
{
    static constexpr int  kFlagCount = 8;
    int                   start      = (int) flags.text.size() - kFlagCount;
    int                   at         = 0;
    uint32_t              changed    = m_host.GetTextColors().changed;



    if (start < 0)
    {
        return;
    }

    for (int bit = kFlagCount - 1; bit >= 0; bit--)
    {
        at = start + (kFlagCount - 1 - bit);

        if (((changedBits >> bit) & 1u) != 0)
        {
            flags.colorRanges.push_back ({ at, at + 1, changed });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::GetRegisterByte
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Byte> RegistersPane::GetRegisterByte (const std::string & name) const
{
    Word  value = 0;



    for (const DebuggerViewSnapshot::RegisterRow & reg : m_registers)
    {
        if (reg.name == name && !DebuggerViewState::IsReadOnlyRegister (name) && TextDialog::TryParseHexWord (TextEncoding::NarrowToWide (reg.value), value) && value <= 0xFF)
        {
            return (Byte) value;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::GetEditValue
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Byte> RegistersPane::GetEditValue (int row) const
{
    return (row < (int) m_registers.size()) ? GetRegisterByte (m_registers[(size_t) row].name) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::CommitEdit
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::CommitEdit (int row, Byte typed)
{
    if (row < (int) m_registers.size())
    {
        CommitRegister (m_registers[(size_t) row].name, typed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::CommitRegister
//
//  Writes a byte register by R, the command a person would type, recording
//  the edit for the pane's undo.
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::CommitRegister (const std::string & name, Byte typed)
{
    std::optional<Byte>  value = GetRegisterByte (name);



    if (!value.has_value())
    {
        return;
    }

    m_history.Record (name, *value, typed, m_pc);
    m_host.RunAction (DebuggerActions::GetSetRegister (name, typed, m_host.GetMode()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::EditRegister
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::EditRegister (int row, bool onFlags)
{
    std::string          name;
    std::optional<Byte>  value;
    Byte                 p     = 0;



    if (row < 0 || row >= (int) m_registers.size())
    {
        return;
    }

    name  = m_registers[(size_t) row].name;
    value = GetRegisterByte (name);

    if (DebuggerViewState::IsReadOnlyRegister (name))
    {
        return;
    }

    if (name == "PC")
    {
        m_host.ShowPcCode();
        return;
    }

    if (name != "P" || !onFlags)
    {
        BeginEdit (row);
        return;
    }

    if (!m_isPaused)
    {
        m_host.AppendConsole ({ GetPauseNote() });
        return;
    }

    if (value.has_value() && FlagsDialog::Ask (m_host.GetOwnerHwnd(), m_host.GetTheme(), *value, p))
    {
        CommitRegister ("P", p);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::OnListReleased
//
//  A click on a register's value edits it in place, as a click on a value
//  does in Visual Studio's registers window. The click has already selected
//  the row it landed on.
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::OnListReleased (DxuiListView * list, POINT clientPx)
{
    RECT  bounds = m_list->GetBounds();
    RECT  value  = {};
    RECT  flags  = {};
    int   row    = m_list->GetSelectedRow();



    if (list != m_list || row < 0 || row >= (int) m_registers.size() || !m_isPaused)
    {
        return;
    }

    if (!m_list->GetCellTextRectPx (row, 1, value) || !m_list->GetCellTextRectPx (row, 2, flags))
    {
        return;
    }

    OffsetRect (&value, bounds.left, bounds.top);
    value.right = bounds.left + flags.left;

    if (DxuiDockSite::Contains (value, clientPx) && GetRegisterByte (m_registers[(size_t) row].name).has_value())
    {
        BeginEdit (row);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::RouteBarMouse
//
//  The bar, then the history band, whose Go live link takes a press.
//
////////////////////////////////////////////////////////////////////////////////

bool RegistersPane::RouteBarMouse (const DxuiMouseEvent & ev)
{
    bool  isOver = m_band->IsVisible() && DxuiDockSite::Contains (m_band->GetBounds(), ev.positionDip);



    if (ValueEditPane::RouteBarMouse (ev))
    {
        return true;
    }

    return isOver && m_host.IsBarRouted (m_id) && m_band->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::TryGetTip
//
//  Over P's flags, a line for each flag, a flag that changed since the last
//  stop in the changed color, as its letter is in the cell.
//
////////////////////////////////////////////////////////////////////////////////

bool RegistersPane::TryGetTip (POINT clientPx, PaneTip & tip)
{
    RECT                 bounds  = m_list->GetBounds();
    RECT                 cell    = {};
    int                  row     = -1;
    std::optional<Byte>  p;
    uint32_t             changed = 0;



    if (!m_host.IsRoutable (m_list) || !m_list->IsVisible() || !DxuiDockSite::Contains (bounds, clientPx))
    {
        return false;
    }

    row = m_list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);

    if (row >= 0 && row < (int) m_registers.size() && m_registers[(size_t) row].name == "P" &&
        m_list->GetCellTextRectPx (row, 2, cell) &&
        clientPx.x - bounds.left >= cell.left && clientPx.x - bounds.left < cell.right)
    {
        p = GetRegisterByte ("P");
    }

    if (!p.has_value())
    {
        return false;
    }

    changed = m_host.GetStopChanges().GetChangedBits ("R:P");

    for (const FlagsDialog::Flag & flag : FlagsDialog::GetFlags())
    {
        tip.lineColors.push_back ((changed & flag.bit) != 0 ? m_host.GetTextColors().changed : 0);
    }

    OffsetRect (&cell, bounds.left, bounds.top);

    tip.kind   = PaneTip::Kind::Control;
    tip.anchor = cell;
    tip.text   = FlagsDialog::Describe (*p);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::AddMenuItems
//
//  The trip count starts again from here, as RCC starts it; the total never
//  does.
//
////////////////////////////////////////////////////////////////////////////////

bool RegistersPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    std::string  name;



    if (list != m_list)
    {
        return false;
    }

    if (row < 0 || row >= (int) m_registers.size())
    {
        return true;
    }

    name = m_registers[(size_t) row].name;

    if (name == "PC")
    {
        items.push_back ({ L"Show in code", [this] { m_host.ShowPcCode(); } });
    }

    if (!DebuggerViewState::IsReadOnlyRegister (name))
    {
        m_host.AddShowInMemoryItems (TextEncoding::NarrowToWide (name), name, items);
    }

    if (name == DebuggerViewState::kCyclesRegister || name == DebuggerViewState::kTripRegister)
    {
        items.push_back ({ L"Reset trip count", [this] { m_host.RunAction (DebuggerActions::GetResetCycles (m_host.GetMode())); } });
    }

    if (name == DebuggerViewState::kLastBranchRegister && m_lastBranchFrom.has_value())
    {
        items.push_back ({ L"Go to branch", [this, at = *m_lastBranchFrom] { m_host.GoToCode (at); } });

        if (m_lastBranchTo.has_value())
        {
            items.push_back ({ L"Go to target", [this, at = *m_lastBranchTo] { m_host.GoToCode (at); } });
        }
    }

    items.push_back ({ L"Copy", [this] { DxuiClipboard::SetText (m_host.GetOwnerHwnd(), m_list->GetSelectionText()); } });
    AddUndoItems (items);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::RouteListKey
//
//  F2 edits the selected value in place, or on P's row opens the flags,
//  which share the row; Ctrl+Z and Ctrl+Y undo and redo the pane's edits.
//
////////////////////////////////////////////////////////////////////////////////

bool RegistersPane::RouteListKey (const DxuiKeyEvent & ev)
{
    if (ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt && ev.vk == VK_F2)
    {
        EditRegister (m_list->GetSelectedRow(), true);
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && (ev.vk == 'Z' || ev.vk == 'Y'))
    {
        Undo (ev.vk == 'Y');
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::CanUndo
//
////////////////////////////////////////////////////////////////////////////////

bool RegistersPane::CanUndo (bool redo) const
{
    return redo ? m_history.CanRedo() : m_history.CanUndo();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::GetUndoText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RegistersPane::GetUndoText (bool redo) const
{
    return redo ? m_history.GetRedoText() : m_history.GetUndoText();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane::Undo
//
//  The pane's last edit taken back, or with `redo` made again, while the
//  machine is at the stop it was made at.
//
////////////////////////////////////////////////////////////////////////////////

void RegistersPane::Undo (bool redo)
{
    std::optional<DebuggerAction>  action = redo ? m_history.TryRedo (m_host.GetMode()) : m_history.TryUndo (m_host.GetMode());



    if (action.has_value())
    {
        m_host.RunAction (*action);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::StackPane
//
////////////////////////////////////////////////////////////////////////////////

StackPane::StackPane (IDebuggerPaneHost & host) :
    ValueEditPane (host, DebuggerLayout::kStack, L"Stack", ColorLegend::Pane::Stack)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::~StackPane
//
////////////////////////////////////////////////////////////////////////////////

StackPane::~StackPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::Configure
//
//  Double-clicking a byte's value edits it in place; double-clicking its
//  address shows it in memory. The pane lists the stack newest first, so its
//  rows run backwards through STACK's.
//
////////////////////////////////////////////////////////////////////////////////

void StackPane::Configure()
{
    ValueEditPane::Configure();

    m_list->SetOnActivateRow ([this] (int row)
    {
        if (row < 0 || row >= (int) m_stack.size())
        {
            return;
        }

        if (IsPressRightOf (row, 1))
        {
            BeginEdit (row);
            return;
        }

        m_host.GoToMemory (m_stack[m_stack.size() - 1 - (size_t) row].address);
    });

    m_list->SetColumns ({ { L"Stack", 0, false, DxuiTextHAlign::Left },
                          { L"Value", 0, false, DxuiTextHAlign::Left } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void StackPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    const StopChanges                             & changes = m_host.GetStopChanges();
    std::vector<std::vector<DxuiListView::Cell>>    rows;



    ValueEditPane::Apply (snapshot);

    m_stack = snapshot.stack;

    for (auto it = snapshot.stack.rbegin(); it != snapshot.stack.rend(); ++it)
    {
        DxuiListView::Cell  value = { std::format (L"{:02X}", it->value) };

        if (changes.IsChanged (std::format ("S:{:04X}", it->address)))
        {
            value.argb = m_host.GetTextColors().changed;
            value.tip  = ColorLegend::GetText (ColorLegend::Meaning::Changed);
        }

        rows.push_back ({ { std::format (L"${:04X}", it->address) }, value });
    }

    m_list->SetRows (std::move (rows));
    m_history.OnSnapshot (snapshot.isPaused, snapshot.pc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::GetEditValue
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Byte> StackPane::GetEditValue (int row) const
{
    return (row < (int) m_stack.size()) ? std::optional<Byte> (m_stack[m_stack.size() - 1 - (size_t) row].value) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::CommitEdit
//
//  Writes a stack byte by the byte-entry command a person would type.
//
////////////////////////////////////////////////////////////////////////////////

void StackPane::CommitEdit (int row, Byte typed)
{
    Word  at    = 0;
    Byte  value = 0;



    if (row < 0 || row >= (int) m_stack.size())
    {
        return;
    }

    at    = m_stack[m_stack.size() - 1 - (size_t) row].address;
    value = m_stack[m_stack.size() - 1 - (size_t) row].value;

    m_history.Record (at, value, typed, m_pc);
    m_host.RunAction (DebuggerActions::GetEnterByte (at, typed, m_host.GetMode()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::PushByte
//
//  PUSH of a byte typed in a dialog, run directly.
//
////////////////////////////////////////////////////////////////////////////////

void StackPane::PushByte()
{
    std::optional<Byte>  value = m_host.RunByteDialog (L"Push byte", L"Byte");



    if (value.has_value())
    {
        m_host.RunAction (DebuggerActions::GetPushByte (*value, m_host.GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::AddMenuItems
//
//  PUSH, POP and PPOP write the machine, so only while it is stopped.
//
////////////////////////////////////////////////////////////////////////////////

bool StackPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    if (list != m_list)
    {
        return false;
    }

    if (row >= 0 && row < (int) m_stack.size())
    {
        Word  at = m_stack[m_stack.size() - 1 - (size_t) row].address;

        m_host.AddShowInMemoryItems (std::format (L"${:04X}", at), std::format ("{:04X}", at), items);
        items.push_back ({ L"Copy", [this] { DxuiClipboard::SetText (m_host.GetOwnerHwnd(), m_list->GetSelectionText()); } });

        if (m_isPaused)
        {
            items.push_back ({ L"Edit value", [this, row] { BeginEdit (row); } });
        }
    }

    if (m_isPaused)
    {
        items.push_back ({ L"Push byte...", [this] { PushByte(); } });
        items.push_back ({ L"Pop byte",     [this] { m_host.RunAction (DebuggerActions::GetPopByte (m_host.GetMode())); } });
        items.push_back ({ L"Pop word",     [this] { m_host.RunAction (DebuggerActions::GetPopWord (m_host.GetMode())); } });
    }

    AddUndoItems (items);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::RouteListKey
//
//  Ctrl+Z and Ctrl+Y undo and redo the pane's edits; F2 edits the selected
//  byte.
//
////////////////////////////////////////////////////////////////////////////////

bool StackPane::RouteListKey (const DxuiKeyEvent & ev)
{
    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && (ev.vk == 'Z' || ev.vk == 'Y'))
    {
        Undo (ev.vk == 'Y');
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt && ev.vk == VK_F2)
    {
        BeginEdit (m_list->GetSelectedRow());
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::CanUndo
//
////////////////////////////////////////////////////////////////////////////////

bool StackPane::CanUndo (bool redo) const
{
    return redo ? m_history.CanRedo() : m_history.CanUndo();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::GetUndoText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StackPane::GetUndoText (bool redo) const
{
    return redo ? m_history.GetRedoText() : m_history.GetUndoText();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane::Undo
//
////////////////////////////////////////////////////////////////////////////////

void StackPane::Undo (bool redo)
{
    std::optional<DebuggerAction>  action = redo ? m_history.TryRedo (m_host.GetMode()) : m_history.TryUndo (m_host.GetMode());



    if (action.has_value())
    {
        m_host.RunAction (*action);
    }
}
