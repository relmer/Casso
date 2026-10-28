#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewArchiveView.h"
#include "CassoExplorer/CassoExplorerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewArchiveView::PreviewArchiveView
//
////////////////////////////////////////////////////////////////////////////////

PreviewArchiveView::PreviewArchiveView (PreviewListingView & listing, ExplorerContext & context)
    : m_listing (listing),
      m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewArchiveView::GetSelectedIds
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> PreviewArchiveView::GetSelectedIds() const
{
    const std::vector<CatalogRow> &  rows = m_context.browser.GetPreview().rows;
    std::vector<std::wstring>        ids;



    for (int row : m_listing.GetList()->GetSelectedRows())
    {
        if (row >= 0 && (size_t) row < rows.size() && !rows[(size_t) row].shellId.empty())
        {
            ids.push_back (rows[(size_t) row].shellId);
        }
    }

    return ids;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewArchiveView::HandleMouse
//
//  An archive's files in the preview act as the file list's do: a press that
//  moves drags them out, for the shell to extract where they land, and a
//  right-click opens the shell's menu for them, Extract All among its verbs.
//  Selection itself is the list's. True when the event is taken here.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewArchiveView::HandleMouse (const DxuiMouseEvent & ev, POINT point)
{
    RECT                       bounds = m_listing.GetList()->GetBounds();
    DxuiMouseEvent             local  = ExplorerLayout::ToLocal (ev, bounds);
    int                        row    = -1;
    HRESULT                    hr     = S_OK;
    POINT                      screen = point;
    std::vector<std::wstring>  ids;



    if (ev.kind == DxuiMouseEventKind::Down && ExplorerLayout::Contains (bounds, point))
    {
        row = m_listing.GetList()->HitTestRow (local.positionDip.x, local.positionDip.y);
        m_context.input.SetFocusPane (ExplorerPane::Preview);

        if (row >= 0 && ev.button == DxuiMouseButton::Right)
        {
            //  A right press on a row outside the selection selects it alone.
            if (!m_listing.GetList()->IsRowSelected (row))
            {
                m_listing.GetList()->ClickRow (row, false, false);
            }

            m_menuArmed = true;
            return true;
        }

        if (row >= 0 && ev.button == DxuiMouseButton::Left)
        {
            m_dragArmed = true;
            m_pressAt   = point;
        }

        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Up && m_menuArmed && ev.button == DxuiMouseButton::Right)
    {
        m_menuArmed = false;
        ids         = GetSelectedIds();

        ClientToScreen (m_context.window.GetHwnd(), &screen);

        if (!ids.empty())
        {
            ShowContextMenu (point.x, point.y);
        }

        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Up)
    {
        m_dragArmed = false;
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Move && m_dragArmed
        && (abs (point.x - m_pressAt.x) > GetSystemMetrics (SM_CXDRAG)
         || abs (point.y - m_pressAt.y) > GetSystemMetrics (SM_CYDRAG)))
    {
        DxuiMouseEvent  release;

        m_dragArmed = false;
        ids         = GetSelectedIds();

        //  The list lets go first, or it would take the move as extending
        //  the selection once the drag is over.
        release.kind   = DxuiMouseEventKind::Up;
        release.button = DxuiMouseButton::Left;
        m_listing.GetList()->OnMouse (release);

        if (!ids.empty())
        {
            hr = m_context.shellVerbs.DragItems (ids, DROPEFFECT_COPY);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewArchiveView::CopySelection
//
//  The shell's data object goes on the clipboard, which keeps the archive's
//  folder open for a paste after the list has moved on.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewArchiveView::CopySelection (bool cut)
{
    std::vector<std::wstring>  ids = GetSelectedIds();
    HRESULT                    hr  = S_OK;



    if (ids.empty())
    {
        return;
    }

    hr = m_context.shellVerbs.PlaceOnClipboard (m_context.window.GetHwnd(), ids, cut);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewArchiveView::DeleteSelection
//
//  An archive has no Recycle Bin, so its files are deleted for good, after
//  Windows asks to confirm, as File Explorer deletes them. The archive is
//  then read again for the preview.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewArchiveView::DeleteSelection()
{
    std::vector<std::wstring>  ids = GetSelectedIds();
    HRESULT                    hr  = S_OK;



    if (ids.empty())
    {
        return;
    }

    hr = m_context.shellVerbs.DeletePermanently (m_context.window.GetHwnd(), ids);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewArchiveView::ShowContextMenu
//
//  The file list's menu for an archive's files: the row of Cut, Copy and
//  Delete, Open, and the shell's full menu under Show more options, Extract
//  All among it.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewArchiveView::ShowContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>                   items;
    std::vector<std::shared_ptr<const DxuiCommand>>  iconCommands;
    POINT                                            screen = { x, y };
    const CassoExplorerActions::Verb                 iconVerbs[] =
    {
        CassoExplorerActions::Verb::Cut,
        CassoExplorerActions::Verb::Copy,
        CassoExplorerActions::Verb::Delete,
    };



    m_context.menus.GetMenuCommands().clear();
    ClientToScreen (m_context.window.GetHwnd(), &screen);

    for (CassoExplorerActions::Verb verb : iconVerbs)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->id       = (int) verb;
        command->label    = ExplorerMenus::GetVerbLabel (verb);
        command->glyph    = ExplorerMenus::GetVerbGlyph (verb);
        command->menuSvg  = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (verb));
        command->dispatch = [this, verb]()
        {
            if (verb == CassoExplorerActions::Verb::Delete)
            {
                DeleteSelection();
            }
            else
            {
                CopySelection (verb == CassoExplorerActions::Verb::Cut);
            }
        };
        iconCommands.push_back (command);
        m_context.menus.GetMenuCommands().push_back (std::move (command));
    }

    items.push_back (DxuiPopupMenuItem::ForIconRow (std::move (iconCommands)));
    items.push_back (DxuiPopupMenuItem::ForSeparator());

    m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::Open), [this]()
    {
        if (!m_listing.GetList()->GetSelectedRows().empty())
        {
            OpenRow (m_listing.GetList()->GetSelectedRows()[0]);
        }
    });

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, screen]()
    {
        HRESULT  hr = m_context.shellVerbs.ShowShellMenu (m_context.window.GetHwnd(), GetSelectedIds(), screen, {});

        IGNORE_RETURN_VALUE (hr, S_OK);
    });
    m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
    m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (CassoExplorerActions::Verb::MoreOptions));

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewArchiveView::OpenRow
//
//  A folder inside the archive opens in the file list; a file opens as the
//  shell opens it.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewArchiveView::OpenRow (int row)
{
    const std::vector<CatalogRow> &  rows = m_context.browser.GetPreview().rows;
    HRESULT                          hr   = S_OK;



    if (row < 0 || (size_t) row >= rows.size() || rows[(size_t) row].shellId.empty())
    {
        return;
    }

    if (rows[(size_t) row].isDirectory)
    {
        m_context.browser.NavigateToLocation (Location::MakeShellFolder (rows[(size_t) row].shellId, rows[(size_t) row].name));
        m_context.listPane.FillList();
        return;
    }

    hr = m_context.shellVerbs.OpenShellItem (m_context.window.GetHwnd(), rows[(size_t) row].shellId);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
