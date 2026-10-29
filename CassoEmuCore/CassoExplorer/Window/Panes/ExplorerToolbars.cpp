#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Window/Managers/ExplorerTips.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerThemes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::ExplorerToolbars
//
////////////////////////////////////////////////////////////////////////////////

ExplorerToolbars::ExplorerToolbars (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::CreateNavBar
//
//  Each bar is created where it falls in the window's paint order; Create
//  then sets them up.
//
//  The bars, as children of the window, each created where it falls in the
//  window's paint order; their setup; and their settings once the window
//  has its HWND.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::CreateNavBar()
{
    m_toolbar = m_context.window.CreateChild<DxuiToolbar>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::CreateMenuBar
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::CreateMenuBar()
{
    m_menuBar = m_context.window.CreateChild<DxuiMenuBar>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::CreateCommandBar
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::CreateCommandBar()
{
    m_commandBar = m_context.window.CreateChild<DxuiToolbar>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::Create
//
//  Explorer's navigation glyphs are in Windows 11's Segoe Fluent Icons.
//  Without that font, MDL2 is used, which has the same code points; text in
//  a missing font renders as nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::Create()
{
    const wchar_t *  iconFace = DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace) ? DxuiToolbar::kFluentIconFace : DxuiToolbar::kMdl2IconFace;



    m_toolbar->SetTextRenderer (m_context.window.GetTextRenderer());
    m_toolbar->SetPopupHost    (m_context.window.GetPopupHost());
    m_toolbar->SetEntries      (m_context.commands.BuildToolbarEntries());

    m_commandBar->SetTextRenderer (m_context.window.GetTextRenderer());
    m_commandBar->SetPopupHost    (m_context.window.GetPopupHost());

    //  Explorer's flat buttons, on this bar and the others.
    m_toolbar->SetFlatStyle    (true);
    m_commandBar->SetFlatStyle (true);

    m_commandBar->EnableSeeMore      (s_kpszMdl2More, L"See more", &CassoExplorerIcons::s_kMore);
    m_commandBar->SetEntries         (m_context.commands.BuildCommandBarEntries());
    m_commandBar->SetIconFace        (iconFace);
    m_commandBar->SetIconDip         (kCommandBarIconDip);
    m_commandBar->SetChevronOnIcons  (true);
    m_commandBar->SetGroupSeparators (true);
    m_commandBar->SetButtonPadDip    (ExplorerMetrics::kCommandBarPadDip);
    m_commandBar->SetLabeledLeadDip  (kCommandBarLabeledDip);
    m_commandBar->SetContentDropDip  (kCommandBarDropDip);
    m_commandBar->SetChevronGapDip   (ExplorerMetrics::kCommandBarChevronGapDip);
    m_commandBar->SetIconGapDip      (ExplorerMetrics::kCommandBarIconGapDip);
    m_commandBar->SetLabelFontDip    (kCommandBarLabelDip);
    m_commandBar->SetGroupGapDp      (ExplorerMetrics::kCommandBarGroupGapDp);
    m_commandBar->SetBarPadDp        (ExplorerMetrics::kCommandBarPadXDp);

    m_toolbar->SetIconFace      (iconFace);
    m_toolbar->SetIconDip       (kNavIconDip);
    m_toolbar->SetButtonPadDip  (ExplorerMetrics::kNavButtonPadDip);
    m_toolbar->SetBarPadDp      (ExplorerMetrics::kNavBarPadDp);
    m_toolbar->SetGroupGapDp    (ExplorerMetrics::kNavAddressGapDp);

    m_menuBar->SetPopupHost              (m_context.window.GetPopupHost());
    m_menuBar->SetTextRendererForMeasure (m_context.window.GetTextRenderer());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::Configure
//
//  Casso Explorer has no menu bar: Explorer's command bar takes its place.
//  The strip is created empty and hidden, so its Alt handling finds nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::Configure()
{
    m_menuBar->SetItems   ({});
    m_menuBar->SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::ApplyTheme
//
//  Explorer's command bar is the list body's color, not the strip's above
//  it, with a border line above and below it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::ApplyTheme (const DxuiTheme & theme)
{
    if (!IsCreated())
    {
        return;
    }

    m_menuBar->SetStripColors    (theme.navStrip, theme.navHover, theme.navItemText);
    m_menuBar->SetDropdownColors (theme.dropdownBg, theme.dropdownHover, theme.dropdownItemText,
                                  theme.dropdownAccel, theme.panelEdge, theme.buttonBorder);

    m_toolbar->SetStripColors    (theme.navStrip, theme.navItemText);
    m_toolbar->SetEdgeColor      (theme.Border());
    m_commandBar->SetStripColors (theme.ContentBackground(), theme.navItemText);
    m_commandBar->SetEdgeColor   (theme.Border());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::PlanForWidth
//
//  The bars' band thicknesses follow the responsive mode each plans for the
//  width, so they are planned before the bands are docked.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::PlanForWidth (int widthPx)
{
    m_toolbar->PlanForWidth    (widthPx, m_context.scaler);
    m_commandBar->PlanForWidth (widthPx, m_context.scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::Layout (const RECT & commandBand, const RECT & navBand)
{
    m_commandBar->SetHostClientRect (m_context.layout.GetClient());
    m_commandBar->Layout (commandBand, m_context.scaler);

    m_toolbar->SetHostClientRect (m_context.layout.GetClient());
    m_toolbar->Layout (navBand, m_context.scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::CloseMenusOutside
//
//  A command bar menu leaves the mouse with the window, so a press anywhere
//  but the bar that opened it closes it, as a click outside a menu does.
//
//  Pointer events. A press outside a bar with a menu open closes it, ahead
//  of the window's other routing; the menu bar is ahead of the address
//  bar, and the two bars after it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::CloseMenusOutside (const DxuiMouseEvent & ev)
{
    if (ev.kind != DxuiMouseEventKind::Down)
    {
        return;
    }

    for (DxuiToolbar * bar : { m_commandBar, m_toolbar, &m_context.preview.GetToolbar() })
    {
        if (bar->IsMenuOpen() && !ExplorerLayout::Contains (bar->GetBounds(), ev.positionDip))
        {
            bar->CloseMenu();
            m_context.window.Invalidate();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::CloseMenus
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::CloseMenus()
{
    for (DxuiToolbar * bar : { m_commandBar, m_toolbar, &m_context.preview.GetToolbar() })
    {
        bar->CloseMenu();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::HandleMouse
//
//  The strip under the pointer has the event and drives the tooltip; the
//  other has the moves as well, so its hover clears when the pointer
//  crosses.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerToolbars::HandleMouse (const DxuiMouseEvent & ev)
{
    DxuiToolbar &  under = GetToolbarUnder (m_context.layout.GetMenuBand().GetBounds(), ev.positionDip, *m_toolbar, *m_commandBar);
    DxuiToolbar &  other = (&under == m_commandBar) ? *m_toolbar : *m_commandBar;



    if (ev.kind == DxuiMouseEventKind::Move)
    {
        other.OnToolbarMouseMove (ev.positionDip.x, ev.positionDip.y);
    }
    else if (ev.kind == DxuiMouseEventKind::Leave)
    {
        other.OnToolbarMouseLeave();
    }

    if (!RouteToolbarMouse (under, ev) && !(ev.kind == DxuiMouseEventKind::Up && RouteToolbarMouse (other, ev)))
    {
        return false;
    }

    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::TryMenuBarKey
//
//  The menu bar, open or focused, has every key; Alt with a letter is a
//  mnemonic. Returns whether the menu bar took the key; outHandled is its
//  answer.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerToolbars::TryMenuBarKey (const DxuiKeyEvent & ev, bool & outHandled)
{
    if (m_menuBar->IsOpen() || m_menuBar->HasFocus())
    {
        outHandled = m_menuBar->OnKey (ev);
        return true;
    }

    if (ev.alt && ev.vk >= 0x20 && ev.vk <= 0x7E && m_menuBar->HandleAltKey ((wchar_t) ev.vk))
    {
        outHandled = true;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::WantsTick
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerToolbars::WantsTick() const
{
    return m_menuBar->WantsTick() || m_toolbar->WantsTick() || m_commandBar->WantsTick();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::IsMenuOpen
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerToolbars::IsMenuOpen() const
{
    return m_menuBar->IsOpen() || m_toolbar->IsMenuOpen() || m_commandBar->IsMenuOpen();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::TickMenus
//
//  Menus slide open and submenus wait out a delay, both on ticks the host
//  supplies.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::TickMenus (int64_t nowMs)
{
    m_menuBar->TickMenus    (nowMs);
    m_toolbar->TickMenus    (nowMs);
    m_commandBar->TickMenus (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::GetToolbarUnder
//
//  The strip a pointer event goes to: the command bar over its band, the
//  navigation toolbar anywhere else.
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar & ExplorerToolbars::GetToolbarUnder (const RECT & commandBarBand, POINT point, DxuiToolbar & navToolbar, DxuiToolbar & commandBar)
{
    return ExplorerLayout::Contains (commandBarBand, point) ? commandBar : navToolbar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::RouteToolbarMouse
//
//  The toolbar takes its pointer input through its own calls rather than
//  HandleMouse. A move always reaches it, so its hover clears when the pointer
//  leaves; it is consumed only over the strip. The tooltip follows the hover.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerToolbars::RouteToolbarMouse (DxuiToolbar & toolbar, const DxuiMouseEvent & ev)
{
    int              x      = ev.positionDip.x;
    int              y      = ev.positionDip.y;
    bool             took   = false;
    RECT             anchor = {};
    const wchar_t *  tip    = nullptr;
    int64_t          nowMs  = CassoExplorerWindow::GetNowMs();



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Move:
            took = toolbar.OnToolbarMouseMove (x, y);
            tip  = took ? toolbar.GetTooltipAt (x, y, anchor) : nullptr;

            //  A button clicked stays quiet until the pointer has left it, as
            //  Explorer's command bar does, rather than tipping again over the
            //  menu it opened.
            if (m_tipMuted && ExplorerLayout::Contains (m_tipMuteRect, ev.positionDip))
            {
                tip = nullptr;
            }
            else
            {
                m_tipMuted = false;
            }

            if (tip != nullptr && *tip != L'\0')
            {
                m_context.tips.ShowHoverTip (ExplorerTips::TipOwner::Toolbar, anchor, tip);
            }
            else
            {
                m_context.tips.HideHoverTip (ExplorerTips::TipOwner::Toolbar);
            }

            break;

        case DxuiMouseEventKind::Down:
            took = ev.button == DxuiMouseButton::Left && toolbar.OnToolbarLButtonDown (x, y);

            //  A right-click on Back or Forward lists where each would go, as
            //  Explorer's does.
            if (ev.button == DxuiMouseButton::Right && toolbar.TryGetEntryRect (CassoExplorerCommands::kBack, anchor) && ExplorerLayout::Contains (anchor, ev.positionDip))
            {
                m_context.addressBar.ShowHistoryMenu (false, anchor);
                took = true;
            }
            else if (ev.button == DxuiMouseButton::Right && toolbar.TryGetEntryRect (CassoExplorerCommands::kForward, anchor) && ExplorerLayout::Contains (anchor, ev.positionDip))
            {
                m_context.addressBar.ShowHistoryMenu (true, anchor);
                took = true;
            }

            if (took)
            {
                m_context.tips.HideImmediate();
                m_tipMuted = toolbar.GetTooltipAt (x, y, m_tipMuteRect) != nullptr;
            }

            break;

        case DxuiMouseEventKind::Up:
            took = ev.button == DxuiMouseButton::Left && toolbar.OnToolbarLButtonUp (x, y);
            break;

        case DxuiMouseEventKind::Leave:
            toolbar.OnToolbarMouseLeave();
            m_context.tips.HideHoverTip (ExplorerTips::TipOwner::Toolbar);
            break;

        default:
            break;
    }

    return took;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::UpdateCommandBar
//
//  The Recycle Bin, and an archive selected or open, have buttons of their
//  own on the command bar, after View, as Explorer's.
//
//  The location's buttons, the drop-downs and the View button's icon.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::UpdateCommandBar()
{
    using Extra = CassoExplorerCommands::CommandBarExtra;

    Extra  extra = Extra::None;



    if (m_context.browser.GetLocation().kind == Location::Kind::RecycleBin)
    {
        extra = Extra::RecycleBin;
    }
    else if (m_context.browser.IsAppleDriveLocation() || m_context.cassoLinkManager.IsOnLinkedMachine())
    {
        extra = Extra::AppleDrive;
    }
    else if (!m_context.browser.GetExtractTarget().empty())
    {
        extra = Extra::Archive;
    }

    if (extra == m_commandBarExtra)
    {
        return;
    }

    m_commandBarExtra = extra;

    //  Explorer's own icons for the buttons, in the theme's set.
    m_context.commands.SetSvg (CassoExplorerCommands::kEmptyRecycleBin, m_context.menus.GetMenuSvg (L"windows.recyclebin.empty"));
    m_context.commands.SetSvg (CassoExplorerCommands::kRestoreItems,    m_context.menus.GetMenuSvg (L"windows.recyclebin.restoreall"));
    m_context.commands.SetSvg (CassoExplorerCommands::kExtractAll,      m_context.menus.GetMenuSvg (L"windows.compressedfile.extract"));
    m_commandBar->SetEntries (m_context.commands.BuildCommandBarEntries (extra));
    m_commandBar->Layout     (m_context.layout.GetMenuBand().GetBounds(), m_context.scaler);
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::SetCommandBarDropDowns
//
//  The rows of New, Sort, View and Theme. They are commands, so each reads
//  whether it is enabled and checked when the menu draws.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::SetCommandBarDropDowns()
{
    std::vector<DxuiPopupMenuItem>  newItems;
    std::vector<DxuiPopupMenuItem>  sortItems = m_context.listPane.GetMenus().GetSortChoices();
    std::vector<DxuiPopupMenuItem>  viewItems = m_context.listPane.GetMenus().GetViewChoices();
    std::vector<DxuiPopupMenuItem>  themeItems;



    m_context.menus.AddNewChoices (newItems, m_context.browser.GetLocation(), m_context.commands.Find (CassoExplorerCommands::kNewFolder),
                   m_context.commands.Find (CassoExplorerCommands::kNewDisk));

    //  Group by is at the foot of Explorer's Sort menu.
    sortItems.push_back (DxuiPopupMenuItem::ForSeparator());
    sortItems.push_back (DxuiPopupMenuItem::ForSubmenu (m_context.commands.Find (CassoExplorerCommands::kGroupBy), m_context.listPane.GetMenus().GetGroupChoices()));

    for (int id : { (int) CassoExplorerCommands::kThemeSkeuomorphic, (int) CassoExplorerCommands::kThemeRetroTerminal, (int) CassoExplorerCommands::kThemeDarkModern,
                    (int) CassoExplorerCommands::kThemeDark, (int) CassoExplorerCommands::kThemeLight, (int) CassoExplorerCommands::kThemeSystem })
    {
        themeItems.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (id)));
    }

    m_commandBar->SetDropDownItems (CassoExplorerCommands::kNew,   std::move (newItems));
    m_commandBar->SetDropDownItems (CassoExplorerCommands::kSort,  std::move (sortItems));
    m_commandBar->SetDropDownItems (CassoExplorerCommands::kView,  std::move (viewItems));
    m_commandBar->SetDropDownItems (CassoExplorerCommands::kTheme, std::move (themeItems));

    //  Each theme is shown as the pointer passes over its row; leaving the
    //  menu without a choice puts back the one it opened on.
    m_commandBar->SetDropDownSinks (CassoExplorerCommands::kTheme, [this] (int index) { m_context.themes.Preview (index); }, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars::ShowViewOnButton
//
//  The View button shows the current view's icon, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerToolbars::ShowViewOnButton()
{
    if (m_commandBar != nullptr)
    {
        m_commandBar->SetEntryVectorIcon (CassoExplorerCommands::kView, CassoExplorerCommands::GetViewButtonIcon (m_context.listPane.GetColumns().GetView()));
    }
}
