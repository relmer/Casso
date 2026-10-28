#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorMenus.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::InspectorMenus
//
////////////////////////////////////////////////////////////////////////////////

InspectorMenus::InspectorMenus (InspectorSession & session, IInspectorComponentHost & host, const InspectorCommands & commands) :
    m_session  (session),
    m_host     (host),
    m_commands (commands)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::Create
//
////////////////////////////////////////////////////////////////////////////////

void InspectorMenus::Create()
{
    m_menuBar = m_host.GetControlParent().CreateChild<DxuiMenuBar>();

    m_menuBar->SetPopupHost (m_host.GetPopupHost());
    m_menuBar->SetTextRendererForMeasure (m_host.GetTextRenderer());
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::Rebuild
//
//  The menus from the commands, in FR-158's order. A command left empty, as
//  Casso's head commands are in a host without a drive, has no row.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorMenus::Rebuild()
{
    const InspectorCommands &     c     = m_commands;
    vector<DxuiMenuBarItem>       menus;
    vector<DxuiPopupMenuItem>     rows;



    auto  add = [&rows] (const InspectorCommands::Command & command)
    {
        if (command != nullptr)
        {
            rows.push_back (DxuiPopupMenuItem::ForCommand (command));
        }
    };

    //  One separator between groups, never two and never at the top.
    auto  separator = [&rows] ()
    {
        if (!rows.empty() && rows.back().kind != DxuiPopupMenuItem::Kind::Separator)
        {
            rows.push_back (DxuiPopupMenuItem::ForSeparator());
        }
    };

    //  The rows so far become a menu, with no separator at its end.
    auto  menu = [&menus, &rows] (LPCWSTR label)
    {
        DxuiMenuBarItem  item;

        while (!rows.empty() && rows.back().kind == DxuiPopupMenuItem::Kind::Separator)
        {
            rows.pop_back();
        }

        item.label   = label;
        item.submenu = std::move (rows);
        menus.push_back (std::move (item));
        rows.clear();
    };
    add (c.exportFile);
    add (c.saveEditedCopy);
    separator();
    add (c.close);
    menu (L"&File");

    add (c.undoApplied);
    add (c.redoApplied);
    separator();
    add (c.copy);
    add (c.copySector);
    add (c.copyMap);
    add (c.selectAll);
    separator();
    add (c.editSector);
    add (c.applyEdits);
    add (c.discardEdits);
    separator();
    add (c.find);
    add (c.findNext);
    add (c.findPrevious);
    menu (L"&Edit");

    add (c.structure);
    add (c.timing);
    add (c.widerRange);
    add (c.narrowerRange);
    separator();
    add (c.zoomIn);
    add (c.zoomOut);
    add (c.fit);
    add (c.flat);
    add (c.tilted);
    separator();

    for (const InspectorCommands::Command & overlay : c.overlays)
    {
        add (overlay);
    }

    separator();

    for (const InspectorCommands::Command & view : c.views)
    {
        add (view);
    }

    separator();
    add (c.legend);
    menu (L"&View");

    add (c.goTo);
    add (c.goToHead);
    add (c.followHead);
    separator();
    add (c.previousSector);
    add (c.nextSector);
    add (c.previousQuarterTrack);
    add (c.nextQuarterTrack);
    add (c.previousTrack);
    add (c.nextTrack);
    separator();
    add (c.previousDifference);
    add (c.nextDifference);
    separator();
    add (c.nextPane);
    menu (L"&Go");

    for (const InspectorCommands::Command & drive : c.drives)
    {
        add (drive);
    }

    separator();
    add (c.compareWith);
    add (c.swapSides);
    add (c.bSettings);
    add (c.endComparison);
    separator();
    add (c.decodeSettings);
    menu (L"&Disk");

    add (c.windowZoomIn);
    add (c.windowZoomOut);
    add (c.actualSize);
    separator();
    add (c.defaultSize);
    menu (L"&Window");

    add (c.walkthrough);
    menu (L"&Help");

    m_menuBar->SetItems (std::move (menus));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::Layout
//
////////////////////////////////////////////////////////////////////////////////

void InspectorMenus::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    RECT  client = m_host.GetControlParent().GetBounds();



    m_menuBar->SetHostClientRect (client);
    m_menuBar->Layout (area, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::GetHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int InspectorMenus::GetHeightPx (const DxuiDpiScaler & scaler) const
{
    return DxuiMenuBar::GetStripHeightPx (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::Tick
//
//  An open menu's animation advances with the window's frames.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorMenus::Tick (int64_t nowMs)
{
    if (m_menuBar->WantsTick())
    {
        m_menuBar->TickMenus (nowMs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::IsActive
//
//  True while a menu is open or a menu title has the keyboard.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorMenus::IsActive() const
{
    return m_menuBar->IsOpen() || m_menuBar->HasFocus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMenus::OnKey
//
//  Alt and a menu's underlined letter open that menu. F10, or Alt pressed
//  and released alone, moves the keyboard to the first menu title, where
//  Left and Right move between titles, Enter, Space and Down open the menu,
//  and Escape or F10 return the keys to the window. An open menu takes
//  every key.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorMenus::OnKey (const DxuiKeyEvent & ev)
{
    bool  isDown    = ev.kind == DxuiKeyEventKind::Down;
    bool  isAlt     = ev.vk == VK_MENU || ev.vk == VK_LMENU || ev.vk == VK_RMENU;
    int   focused   = m_menuBar->GetFocusedMenu();
    int   count     = std::max (m_menuBar->GetMenuCount(), 1);
    bool  isHandled = false;



    if (isAlt)
    {
        //  Alt released with no other key between turns the keys to the
        //  menus.
        isHandled    = !isDown && m_isAltAlone && !m_menuBar->IsOpen();
        m_isAltAlone = isDown && (m_isAltAlone || !ev.ctrl);

        if (isHandled)
        {
            m_menuBar->SetFocusedMenu (0);
        }
    }
    else if (!isDown)
    {
        m_isAltAlone = false;
    }
    else if (m_menuBar->IsOpen())
    {
        m_isAltAlone = false;
        (void) m_menuBar->HandleKey (ev.vk);
        isHandled = true;
    }
    else if (ev.alt && !ev.ctrl && ev.vk >= 0x20 && ev.vk <= 0x7E)
    {
        m_isAltAlone = false;
        isHandled    = m_menuBar->HandleAltKey (static_cast<wchar_t> (ev.vk));
    }
    else if (m_menuBar->HasFocus())
    {
        m_isAltAlone = false;
        isHandled    = true;

        switch (ev.vk)
        {
            case VK_LEFT:   m_menuBar->SetFocusedMenu ((focused + count - 1) % count); break;
            case VK_RIGHT:  m_menuBar->SetFocusedMenu ((focused + 1) % count);         break;
            case VK_RETURN:
            case VK_SPACE:
            case VK_DOWN:   m_menuBar->Open (focused, true);                           break;
            case VK_ESCAPE:
            case VK_F10:    m_menuBar->ClearFocus();                                   break;
            default:                                                                   break;
        }
    }
    else if (ev.vk == VK_F10 && !ev.ctrl && !ev.shift && !ev.alt)
    {
        m_isAltAlone = false;
        m_menuBar->SetFocusedMenu (0);
        isHandled = true;
    }
    else
    {
        m_isAltAlone = false;
    }

    return isHandled;
}