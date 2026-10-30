#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerListColumns.h"
#include "CassoExplorer/Window/Managers/ExplorerListMenus.h"
#include "CassoExplorer/Model/RefreshAnchor.h"
#include "CassoExplorer/Window/Panes/ExplorerPane.h"
#include "Seams/Win32ContentDetails.h"
#include "Seams/Win32PropertyColumns.h"
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

    void  Create             ();
    void  Configure          ();
    void  Layout             (const RECT & rect);
    RECT  GetListMessageRect () const;
    void  AlignMessage       (bool aboveActions);

    //  The columns and views, and the menus.
    ExplorerListColumns                    & GetColumns              () { return m_columns; }
    ExplorerListMenus                      & GetMenus                () { return m_menus; }
    void                                     FillList                ();
    bool                                     SelectRowNamed          (const std::wstring & name);

    //  Selects the items with these names in `folder` once a listing has every
    //  one of them, as File Explorer selects what was dropped or pasted. A
    //  copy finishes after the drop returns, so the names are awaited for a
    //  few seconds, and are discarded when another folder is opened.
    void                                     SelectWhenListed        (const std::wstring & folder, std::vector<std::wstring> names);
    void                                     SizeListIcons           ();
    void                                     RefreshListIcons        ();
    void                                     RefreshContentDetails   ();
    void                                     StartLoaders            ();
    bool                                     TryApplyContentDetails  (int row, DxuiListView::Cell & name);
    void                                     ApplyPropertyCells      (int row, std::vector<DxuiListView::Cell> & cells);
    void                                     RefreshPropertyCells    ();
    Win32PropertyColumns                   & GetPropertyValues       () { return m_propertyValues; }

    //  Fits a property column to the values on screen as they are read, until
    //  every one is, or the folder changes.
    void                                     FitColumnOnceRead       (size_t column);
    void                                     FitColumnsRead          ();
    void                                     SelectArrivals          ();

    //  An item's date and size, so its properties are read again when it changes.
    static std::wstring  GetPropertyStamp (const CatalogRow & row) { return std::to_wstring (row.modifiedUnix) + L"." + std::to_wstring (row.sizeBytes); }
    const std::vector<DxuiListView::Cell>  & GetListRowCells         (int row);
    static std::wstring                      GetEmptyLocationMessage (Location::Kind kind);
    void                                     UpdateTip               (const DxuiMouseEvent & ev, POINT point);
    void                                     RefreshTip              ();
    std::vector<DxuiPopupMenuItem>           GetScrollbarMenu        (const DxuiMouseEvent & ev) const;
    bool                                     UpdateScrollbarHover    (POINT at) { return m_list->SetScrollbarHover (at); }
    LPCWSTR                                  GetCursorForPoint       (POINT clientPx) const;
    IDxuiControl                           * GetControl              () const { return m_list; }
    DxuiListView                           * GetListView             () const { return m_list; }
    RECT                                     GetBounds               () const { return m_list->GetBounds(); }

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

    void  HandleShellListed           (const std::wstring & key);
    void  HandleIconsLoaded           ();
    void  HandleContentDetailsArrived ();
    bool  HandleCapturedMouse         (const DxuiMouseEvent & ev);
    bool  HandleMouse                 (const DxuiMouseEvent & ev);
    void  ClearHoverOutside           (const DxuiMouseEvent & ev);
    bool  HandleCharKey               (const DxuiKeyEvent & ev);
    bool  HandleShortcutKey           (const DxuiKeyEvent & ev);
    bool  HandleKey                   (const DxuiKeyEvent & ev) { return m_list->OnKey (ev); }
    void  HandleFocusChanged          (bool focused) { m_list->OnFocusChanged (focused); }

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

    ExplorerContext            & m_context;
    DxuiListView               * m_list            = nullptr;
    DxuiLabel                  * m_listMessage     = nullptr;
    bool                         m_listMessageTop  = false;   // a search's message, at the list's top
    Win32ShellIcons              m_listIcons;
    Win32ContentDetails          m_contentDetails;
    Win32PropertyColumns         m_propertyValues;
    bool                         m_propertiesShown = false;   // a property column is visible for the folder on display
    std::vector<size_t>          m_columnsToFit;   // property columns fitted as their values arrive
    static constexpr uint64_t    s_kArrivalWaitMs  = 10000;

    std::wstring               m_arrivalFolder;   // see SelectWhenListed
    std::vector<std::wstring>  m_arrivalNames;
    uint64_t                   m_arrivalUntilMs = 0;   // GetTickCount64 past which the names are no longer awaited
    Location                   m_listLocation;
    size_t                     m_listTabCount   = 0;   // the tabs when FillList last saved the session

    //  The list's rows, built as it first asks for each; see GetListRowCells.
    std::vector<std::vector<DxuiListView::Cell>>  m_rowCells;
    std::vector<bool>                             m_rowBuilt;
    Location                                      m_rowLocation;
    bool                                          m_rowDark      = false;

    //  Why each row's image is broken, as its cells are built; empty for the
    //  rest.
    std::vector<std::wstring>  m_rowProblems;
    std::vector<std::wstring>  m_cutPaths;
    bool                       m_dragArmed     = false;   // a press on a row, which drags the selection once it moves
    POINT                      m_dragStart     = {};
    RightPress                 m_rightPress;
    POINT                      m_listTipPoint  = {};   // where the pointer last was over the list
    bool                       m_listTipActive = false;
    ExplorerListColumns        m_columns       { *this, m_context };
    ExplorerListMenus          m_menus         { *this, m_context };
};
