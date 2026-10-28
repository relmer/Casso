#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerListColumns.h"
#include "CassoExplorer/Window/Managers/ExplorerListMenus.h"
#include "CassoExplorer/Model/RefreshAnchor.h"
#include "CassoExplorer/Window/Panes/ExplorerPane.h"
#include "Seams/Win32ContentDetails.h"
#include "Seams/Win32ShellIcons.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane
//
//  The file list: its rows and cells, each built when it is first drawn,
//  the message an empty folder or image has in its place, the rows cut to
//  the clipboard, a press that drags the selection out, and a row's tip.
//  Its columns and views and its menus are its parts. The window routes
//  pointer and key events over the list here.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerListPane
{
public:
    explicit ExplorerListPane (ExplorerContext & context);

    static constexpr const wchar_t *  kNoSearchMatches = L"No items match your search.";

    //  The list and its message, as children of the window, and their
    //  settings and handlers once the window has its HWND.
    void  Create    ();
    void  Configure ();
    void  Layout    (const RECT & rect);
    RECT  GetListMessageRect () const;

    //  The message aligned for a link's buttons below it, or for none.
    void  AlignMessage (bool aboveActions);

    //  The columns and views, and the menus.
    ExplorerListColumns &  GetColumns () { return m_columns; }
    ExplorerListMenus   &  GetMenus   () { return m_menus; }

    void  FillList          ();
    void  HandleShellListed     (const std::wstring & key);
    bool  SelectRowNamed    (const std::wstring & name);
    void  SizeListIcons     ();
    void  RefreshListIcons  ();
    void  RefreshContentDetails ();

    //  The icons and the Content view's details, read on background threads;
    //  each thread posts to the window when some are in.
    void  StartLoaders            ();
    void  HandleIconsLoaded           ();
    void  HandleContentDetailsArrived ();
    bool  TryApplyContentDetails (int row, DxuiListView::Cell & name);
    const std::vector<DxuiListView::Cell> &  GetListRowCells (int row);

    //  What an empty list says, for the kind of thing being looked at.
    static std::wstring  GetEmptyLocationMessage (Location::Kind kind);

    //  Pointer events: a press the list started is ahead of the panes under
    //  the pointer; a press over the list is after the tree's.
    bool  HandleCapturedMouse (const DxuiMouseEvent & ev);
    bool  HandleMouse         (const DxuiMouseEvent & ev);
    void  UpdateTip       (const DxuiMouseEvent & ev, POINT point);
    void  RefreshTip      ();

    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu     (const DxuiMouseEvent & ev) const;
    bool                            UpdateScrollbarHover (POINT at) { return m_list->SetScrollbarHover (at); }
    LPCWSTR                         GetCursorForPoint    (POINT clientPx) const;

    //  Keys while the list has focus.
    bool            HandleCharKey      (const DxuiKeyEvent & ev);
    bool            HandleShortcutKey  (const DxuiKeyEvent & ev);
    bool            HandleKey          (const DxuiKeyEvent & ev) { return m_list->OnKey (ev); }
    void            HandleFocusChanged (bool focused) { m_list->OnFocusChanged (focused); }
    IDxuiControl *  GetControl     () const { return m_list; }
    DxuiListView *  GetListView    () const { return m_list; }
    RECT            GetBounds      () const { return m_list->GetBounds(); }

    //  Rows cut to the clipboard and not yet pasted, drawn dimmed.
    void  SetCutPaths   (std::vector<std::wstring> paths) { m_cutPaths = std::move (paths); }
    void  ClearCutPaths () { m_cutPaths.clear(); }

    //  Whether the list is mid-press: a row pressed that may become a drag, or
    //  the list handling a drag of its own.
    bool  IsBusy () const { return m_dragArmed || m_list->IsInteracting(); }

    //  Rows, for a drop, a drag, a rename and a re-read.
    bool  IsVisible        () const { return m_list->IsVisible(); }
    bool  HasSelection     () const { return !m_list->GetSelectedRows().empty(); }
    int   GetFocusedRow    () const { return m_list->GetSelectedRow(); }
    int   HitTestRow       (POINT client) const;
    int   GetDropRow       () const { return m_list->GetDropRow(); }
    void  SetDropRow       (int row) { m_list->SetDropRow (row); }
    void  ReleasePress     ();
    bool  TryGetRenameRect (int row, RECT & outRect);
    void  CaptureView      (RefreshAnchor::Before & outBefore) const;
    void  RestoreView      (const RefreshAnchor::After & after);

    //  A refresh blanks the list until it is refilled; the slides of its
    //  scrollbars, header and groups.
    void  SetContentHidden (bool hidden) { m_list->SetContentHidden (hidden); }
    bool  Tick             (int64_t nowMs);

private:

    //  A right press, which opens the menu on its release or drags the
    //  selection when it moves far enough first.
    struct RightPress
    {
        bool   active  = false;
        bool   canDrag = false;   // pressed on a row
        int    group   = -1;      // pressed on a group's header
        POINT  start   = {};
    };

    ExplorerContext  & m_context;

    DxuiListView         * m_list           = nullptr;
    DxuiLabel            * m_listMessage    = nullptr;
    bool                   m_listMessageTop = false;   // a search's message, at the list's top

    Win32ShellIcons      m_listIcons;
    Win32ContentDetails  m_contentDetails;
    Location             m_listLocation;
    size_t               m_listTabCount   = 0;   // the tabs when FillList last saved the session

    //  The list's rows, built as it first asks for each; see GetListRowCells.
    std::vector<std::vector<DxuiListView::Cell>>  m_rowCells;
    std::vector<bool>                             m_rowBuilt;
    Location                                      m_rowLocation;
    bool                                          m_rowDark      = false;

    //  Why each row's image is broken, as its cells are built; empty for the
    //  rest.
    std::vector<std::wstring>  m_rowProblems;
    std::vector<std::wstring>  m_cutPaths;

    bool        m_dragArmed     = false;   // a press on a row, which drags the selection once it moves
    POINT       m_dragStart     = {};
    RightPress  m_rightPress;
    POINT       m_listTipPoint  = {};      // where the pointer last was over the list
    bool        m_listTipActive = false;

    ExplorerListColumns  m_columns { *this, m_context };
    ExplorerListMenus    m_menus   { *this, m_context };
};
