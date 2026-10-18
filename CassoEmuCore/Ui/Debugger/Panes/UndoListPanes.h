#pragma once

#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/RegisterHistory.h"
#include "Ui/Debugger/StackHistory.h"

class HistoryBand;
class UndoBarCommands;





////////////////////////////////////////////////////////////////////////////////
//
//  UndoListPane
//
//  A pane that is a bar of Undo and Redo over its list, each tip quoting
//  the edit it would act on: the registers, the stack and the watches. Each
//  pane's undo is its own, apart from every memory window's.
//
////////////////////////////////////////////////////////////////////////////////

class UndoListPane : public IDebuggerPane
{
public:
    UndoListPane (IDebuggerPaneHost & host, std::wstring id, std::wstring title, ColorLegend::Pane legend);
    ~UndoListPane() override;

    DxuiListView                   * GetList        () const { return m_list; }
    DxuiToolbar                    * GetBar         () const { return m_bar.GetBar(); }

    std::wstring                     GetId          () const override { return m_id; }
    std::wstring                     GetTitle       () const override { return m_title; }
    void                             CreateControls () override;
    void                             Configure      () override;
    void                             AddToDock      (DxuiDockSite & dock) override;
    IDxuiControl                   * GetContent     () const override;
    std::vector<DxuiListView *>      GetLists       () const override { return { m_list }; }
    std::vector<DxuiToolbar *>       GetBars        () const override { return { m_bar.GetBar() }; }
    std::optional<PaneColorKeySpec>  GetColorKey    () const override;
    void                             PlaceBar       () override { m_bar.Place(); }
    bool                             RouteBarMouse  (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    const wchar_t                  * GetBarTip      (POINT clientPx, RECT & anchor) const override { return m_bar.GetTip (clientPx, anchor); }

    //  The pane's undo and redo: whether each can act, what it would act on,
    //  and the step.
    virtual bool                     CanUndo        (bool redo) const = 0;
    virtual std::wstring             GetUndoText    (bool redo) const = 0;
    virtual void                     Undo           (bool redo) = 0;

protected:
    //  Adds the parts of the frame between the bar and the list, as the
    //  registers' history band.
    virtual void                     AddMiddleParts (DebuggerPaneFrame & frame) { (void) frame; }

    //  Undo and Redo rows for a menu, quoting the edits.
    void                             AddUndoItems   (PaneMenuItems & items);

    //  The cell text rectangle a press landed right of, in the list.
    bool                             IsPressRightOf (int row, size_t column) const;

    std::wstring                         m_id;
    std::wstring                         m_title;
    ColorLegend::Pane                    m_legend;
    PaneBarSlot                          m_bar;
    DxuiListView                       * m_list = nullptr;
    std::unique_ptr<DebuggerPaneFrame>   m_frame;
    std::unique_ptr<UndoBarCommands>     m_commands;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ValueEditPane
//
//  An undo list pane whose rows' values are bytes edited in place: a box
//  laid over the value cell, a box of the pane's own so it goes with the
//  pane into a floating window. Only a paused machine takes an edit.
//
////////////////////////////////////////////////////////////////////////////////

class ValueEditPane : public UndoListPane
{
public:
    ValueEditPane (IDebuggerPaneHost & host, std::wstring id, std::wstring title, ColorLegend::Pane legend);
    ~ValueEditPane() override;

    DxuiTextInput                  * GetEditor      () const { return m_editor; }
    bool                             IsEditing      () const { return m_editRow >= 0; }

    //  A box over a row's value, the value in it, all of it selected so
    //  typing replaces it; and its end, Enter committing what was typed and
    //  Escape canceling the edit.
    void                             BeginEdit      (int row);
    void                             EndEdit        (bool commit);

    void                             CreateOverlays () override;
    std::vector<IDxuiControl *>      GetOverlays    () const override { return { m_editor }; }
    void                             Configure      () override;
    std::vector<IDxuiControl *>      GetControls    () const override;
    bool                             RouteEditMouse (const DxuiMouseEvent & ev) override;
    bool                             RouteEditKey   (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    void                             Apply          (const DebuggerViewSnapshot & snapshot) override;

protected:
    //  A row's byte, none for a row with no byte to edit; what goes to the
    //  console when the machine is running; and the edit's commit.
    virtual std::optional<Byte>      GetEditValue   (int row) const = 0;
    virtual const char             * GetPauseNote   () const = 0;
    virtual void                     CommitEdit     (int row, Byte typed) = 0;

    //  The pane's keys on its list with the focus, apart from an edit's.
    virtual bool                     RouteListKey   (const DxuiKeyEvent & ev);

    static constexpr int  kEditMaxChars = 4;

    DxuiTextInput  * m_editor   = nullptr;
    int              m_editRow  = -1;
    bool             m_isPaused = false;
    Word             m_pc       = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RegistersPane
//
//  The registers, each its name and value; P's row has its flags beside
//  the byte. A register that changed since the last stop is in the changed
//  color. The cycle count runs across the pane with how far it moved since
//  the stop before. While the machine is taken back in history, a band
//  over the list has its place and a Go live link.
//
////////////////////////////////////////////////////////////////////////////////

class RegistersPane : public ValueEditPane
{
public:
    explicit RegistersPane (IDebuggerPaneHost & host);
    ~RegistersPane() override;

    RegisterHistory              & GetHistory      () { return m_history; }

    //  PC shows the code at the PC; P's flags open their dialog, a check box
    //  each; every other byte register's value edits in place.
    void                           EditRegister    (int row, bool onFlags);

    void                           CreateControls  () override;
    void                           Configure       () override;
    void                           OnDocked        () override;
    void                           Apply           (const DebuggerViewSnapshot & snapshot) override;
    bool                           RouteBarMouse   (const DxuiMouseEvent & ev) override;
    void                           OnListReleased  (DxuiListView * list, POINT clientPx) override;
    bool                           TryGetTip       (POINT clientPx, PaneTip & tip) override;
    bool                           AddMenuItems    (DxuiListView * list, int row, PaneMenuItems & items) override;
    bool                           CanUndo         (bool redo) const override;
    std::wstring                   GetUndoText     (bool redo) const override;
    void                           Undo            (bool redo) override;

protected:
    void                           AddMiddleParts  (DebuggerPaneFrame & frame) override;
    std::optional<Byte>            GetEditValue    (int row) const override;
    const char                   * GetPauseNote    () const override { return "Pause the machine to edit its registers."; }
    void                           CommitEdit      (int row, Byte typed) override;
    bool                           RouteListKey    (const DxuiKeyEvent & ev) override;

private:
    std::optional<Byte>            GetRegisterByte (const std::string & name) const;
    void                           CommitRegister  (const std::string & name, Byte typed);
    void                           AddChangedFlagRanges (uint32_t changedBits, DxuiListView::Cell & flags) const;

    RegisterHistory                                      m_history;
    HistoryBand                                        * m_band = nullptr;
    std::vector<DebuggerViewSnapshot::RegisterRow>       m_registers;
    std::optional<Word>                                  m_lastBranchFrom;
    std::optional<Word>                                  m_lastBranchTo;
};





////////////////////////////////////////////////////////////////////////////////
//
//  StackPane
//
//  The stack, newest first, as the call stack lists its frames: STACK reads
//  from $01FF down, so the pane turns it over. A byte that changed since the
//  last stop is in the changed color.
//
////////////////////////////////////////////////////////////////////////////////

class StackPane : public ValueEditPane
{
public:
    explicit StackPane (IDebuggerPaneHost & host);
    ~StackPane() override;

    StackHistory                 & GetHistory   () { return m_history; }

    void                           Configure    () override;
    void                           Apply        (const DebuggerViewSnapshot & snapshot) override;
    bool                           AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items) override;
    bool                           CanUndo      (bool redo) const override;
    std::wstring                   GetUndoText  (bool redo) const override;
    void                           Undo         (bool redo) override;

protected:
    std::optional<Byte>            GetEditValue (int row) const override;
    const char                   * GetPauseNote () const override { return "Pause the machine to edit the stack."; }
    void                           CommitEdit   (int row, Byte typed) override;
    bool                           RouteListKey (const DxuiKeyEvent & ev) override;

private:
    void                           PushByte     ();

    StackHistory                                         m_history;
    std::vector<DebuggerViewSnapshot::StackLine>         m_stack;
};
