#include "Pch.h"
#include "Core/AppTimerIds.h"

#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/InspectorWindowSet.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/Window/Managers/ExplorerCassoTarget.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Model/PreviewSteps.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewOutput.h"
#include "CassoExplorer/Model/PreviewText.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
#include "CassoExplorer/Model/PreviewText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::ExplorerPreviewManager
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPreviewManager::ExplorerPreviewManager (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::Create
//
//  The views, then the message, which is drawn over them.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::Create()
{
    for (PreviewView * view : m_views)
    {
        view->Create();
    }

    m_message = m_context.window.CreateChild<DxuiSelectableText>();
    ExplorerTextStyles::Apply (*m_message, ExplorerTextStyles::kPaneMessage);
    m_message->SetAlign    (DxuiTextHAlign::Center, DxuiTextVAlign::Center);

    m_context.window.SetMouseHandler (m_message, [this] (const DxuiMouseEvent & ev) { return HandleMessageMouse (ev); });

    m_summary = m_context.window.CreateChild<DxuiTextView>();
    ExplorerTextStyles::Apply (*m_summary, ExplorerTextStyles::kPreviewText);
    m_summary->SetProportional (true);
    m_summary->SetVisible      (false);

    m_find.Create();
    m_thumbnail.Create();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::CreateToolbar
//
//  The preview's toolbar is the window's last child, so it is drawn over the
//  others.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::CreateToolbar()
{
    m_toolbar = m_context.window.CreateChild<DxuiToolbar>();

    m_context.window.SetMouseHandler     (m_toolbar, [this] (const DxuiMouseEvent & ev) { return m_context.toolbars.HandleBarMouse (*m_toolbar, ev); });
    m_context.window.SetCaptureCondition (m_toolbar, [this]() { return m_toolbar->IsMenuOpen(); });

    m_toolbar->SetFlatStyle    (true);
    m_toolbar->SetTextRenderer (m_context.window.GetTextRenderer());
    m_toolbar->SetPopupHost    (m_context.window.GetPopupHost());
    m_toolbar->SetIconDip      (ExplorerToolbars::kNavIconDip);
    m_toolbar->SetVisible      (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::Configure()
{
    for (PreviewView * view : m_views)
    {
        view->Configure();
    }

    m_message->SetTextRenderer (m_context.window.GetTextRenderer());
    m_message->SetOwnerWindow  (m_context.window.GetHwnd());
    m_summary->SetOwnerWindow  (m_context.window.GetHwnd());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::Layout
//
//  The toolbar sits across the pane's top, and the content below it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::Layout (const RECT & rect)
{
    const DxuiDpiScaler  & scaler    = m_context.scaler;
    RECT                   content   = rect;
    int                    barPx     = 0;
    int                    summaryPx = 0;



    m_rect     = rect;
    m_ruleBand = RECT {};
    m_toolbar->SetVisible (true);

    //  Expand is on every file's toolbar.
    {
        m_toolbar->PlanForWidth (rect.right - rect.left, scaler);
        barPx = scaler.ToPx (m_toolbar->GetBandDp());

        m_toolbar->SetHostClientRect (m_context.layout.GetClient());
        m_toolbar->Layout (RECT { rect.left, rect.top, rect.right, rect.top + barPx }, scaler);
        content.top += barPx;
    }

    //  A disk image's small platter and chips, under the toolbar (FR-072).
    if (m_thumbnail.IsShown())
    {
        m_thumbnail.Layout (RECT { content.left, content.top, content.right, content.top + m_thumbnail.GetHeightPx() });
        content.top += m_thumbnail.GetHeightPx();
    }
    else
    {
        m_thumbnail.Hide();
    }

    //  A WOZ image's details, between the toolbar and the catalog, as tall as
    //  their lines at the pane's width, up to a share of the pane.
    if (m_summary->IsVisible())
    {
        m_summary->Layout  (content, scaler);
        m_summary->Measure (*m_context.window.GetTextRenderer(), *m_context.theme);

        summaryPx = (std::min) (m_summary->GetContentHeightPx(), (int) (content.bottom - content.top) / kSummaryShare);

        m_summary->Layout (RECT { content.left, content.top, content.right, content.top + summaryPx }, scaler);

        m_ruleBand  = RECT { content.left, content.top + summaryPx, content.right, content.top + summaryPx + scaler.ToPx (kRuleBandDip) };
        content.top = m_ruleBand.bottom;
    }

    m_content = content;

    for (PreviewView * view : m_views)
    {
        view->Layout (content);
    }

    m_message->Layout (m_context.layout.GetMessageRect (content), scaler);

    m_picture.UpdateCrtPicture (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::Hide
//
//  The pane gone: every view hidden, and a shell preview unloaded.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::Hide()
{
    for (PreviewView * view : m_views)
    {
        view->Hide();
    }

    m_message->SetVisible (false);
    m_summary->SetVisible (false);
    m_toolbar->SetVisible (false);
    m_thumbnail.Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::FillPreview
//
//  The preview's kind picks the view: a picture in the picture view, a
//  catalog as rows under the list's columns, bytes in the hex view, the
//  shell's preview of a host file over the pane, and everything else as
//  text. A new kind brings its toolbar.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::FillPreview()
{
    const PreviewContent  & preview  = m_context.browser.GetPreview();
    bool                    autoOpen = UpdateAutoPreview();
    bool                    toggled  = UpdateShown() || autoOpen;
    bool                    visible  = m_shown;
    bool                    picture  = preview.kind == PreviewContent::Kind::Picture && !preview.bgra.empty();
    bool                    hostFile = preview.kind == PreviewContent::Kind::Hex && !preview.hostPath.empty();
    HostCharset             charset  = preview.hostCharset;
    bool                    opened   = m_hex.OpenHostFile (preview, charset);
    bool                    error    = preview.kind == PreviewContent::Kind::Error || (hostFile && !opened);
    bool                    hex      = preview.kind == PreviewContent::Kind::Hex && (!preview.bytes.empty() || opened);
    bool                    disasm   = preview.kind == PreviewContent::Kind::Hex && preview.disassembly;
    bool                    vars     = preview.kind == PreviewContent::Kind::Variables;
    bool                    columns  = (preview.kind == PreviewContent::Kind::Catalog && preview.lines.empty()) || vars || preview.kind == PreviewContent::Kind::Grid;
    bool                    program  = preview.kind == PreviewContent::Kind::Listing && !preview.bytes.empty();
    bool                    forms    = picture && m_context.browser.CanTogglePictureColor();
    bool                    shell    = preview.kind == PreviewContent::Kind::Shell;
    bool                    woz      = !preview.wozPath.empty();
    bool                    random   = preview.recordLength > 0;
    bool                    sets     = preview.charset != Charset::Default;
    bool                    sheet    = preview.sheetForm != SheetForm::None;
    bool                    details  = visible && woz && preview.kind == PreviewContent::Kind::Catalog && !preview.details.empty();
    bool                    hadThem  = m_summary->IsVisible();
    int                     mode     = !visible ? CassoExplorerCommands::kPreviewBarNone
                                     : hex      ? CassoExplorerCommands::kPreviewBarHex
                                     : disasm   ? CassoExplorerCommands::kPreviewBarHex
                                     : program  ? CassoExplorerCommands::kPreviewBarListing
                                     : forms    ? CassoExplorerCommands::kPreviewBarPicture
                                     : picture  ? CassoExplorerCommands::kPreviewBarSave
                                     : vars     ? CassoExplorerCommands::kPreviewBarVariables
                                     : random   ? CassoExplorerCommands::kPreviewBarRecords
                                     : sets     ? CassoExplorerCommands::kPreviewBarText
                                     : sheet    ? CassoExplorerCommands::kPreviewBarSheet
                                     : woz      ? CassoExplorerCommands::kPreviewBarWoz
                                                : CassoExplorerCommands::kPreviewBarNone;



    if (preview.kind != m_lastKind || picture || shell)
    {
        m_pictureZoom = CassoExplorerPrefs::kDefaultPreviewZoom;
        m_picture.SetZoom (1.0f);
    }

    m_lastKind = preview.kind;

    //  The thumbnail of an image selected alone in a folder, in a format the
    //  track inspector reads; none for one that fails to open (FR-072).
    {
        bool          wasShown = m_thumbnail.IsShown();
        bool          isImage  = visible && !error && m_context.browser.AreSelectedRowsImages() && m_context.browser.GetSelectedRows().size() == 1;
        std::wstring  path     = isImage ? m_context.cassoTarget.GetSelectedImagePath() : std::wstring();

        m_thumbnail.SetPath (InspectorWindowSet::HasTrackFormat (path) ? path : std::wstring());

        if (m_thumbnail.IsShown() != wasShown || m_thumbnail.IsShown())
        {
            m_context.layout.RecomputeLayout();
        }
    }

    m_shell.Update   (preview, m_content);
    m_picture.Fill   (preview, picture, shell && m_shell.IsPicture());
    m_listing.Fill   (preview, columns);
    m_text.Fill      (preview);

    //  A WOZ image's details above its catalog, which moves the catalog down.
    //  An image with no file system has its details in place of a catalog.
    m_summary->SetRows    (details ? PreviewText::BuildDetailRows (preview.details) : std::vector<DxuiTextView::Row>());
    m_summary->SetVisible (details);

    if (details || hadThem)
    {
        m_context.layout.RecomputeLayout();
    }

    if (mode != m_barMode || !m_isBarBuilt || m_isBarExpanded != IsExpanded())
    {
        m_barMode       = mode;
        m_isBarBuilt    = true;
        m_isBarExpanded = IsExpanded();

        m_toolbar->SetEntries (m_context.commands.BuildPreviewToolbarEntries (mode, m_isBarExpanded, m_find.GetSearchBox(), m_find.GetGoToBox()));

        //  The boxes go with the hex view's toolbar, and their focus with it;
        //  the variables' toolbar has the search box alone.
        if (mode != CassoExplorerCommands::kPreviewBarHex && (m_context.input.GetFocusPane() == ExplorerPane::GoTo
                                                           || (m_context.input.GetFocusPane() == ExplorerPane::Search && mode != CassoExplorerCommands::kPreviewBarVariables)))
        {
            m_context.input.SetFocusPane (ExplorerPane::Preview);
        }

        m_context.layout.RecomputeLayout();
    }

    //  View as lists the file's default view and the others that apply.
    {
        std::vector<DxuiPopupMenuItem>  views = { DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kViewAsDefault)) };

        for (ViewChoice view : preview.viewChoices)
        {
            views.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kViewAsDefault + (int) view)));
        }

        m_toolbar->SetDropDownItems (CassoExplorerCommands::kViewAs, std::move (views));
    }

    m_hex.Fill (preview, hex, opened, charset);

    m_message->SetText (!error                  ? std::wstring()
                        : (hostFile && !opened) ? PreviewText::FormatPreviewError (L"The file could not be read. " + m_hex.DescribeFileError())
                                                : PreviewText::FormatPreviewError (preview.message));
    m_message->SetVisible (visible && error);

    //  An error is muted and centered in the pane.
    ExplorerTextStyles::Apply (*m_message, ExplorerTextStyles::kPaneMessage);

    m_picture.SetVisible (visible && (picture || (shell && m_shell.IsPicture())));
    m_listing.SetVisible (visible && columns);
    m_text.SetVisible    (visible && !picture && !shell && !error && !hex && !columns);
    m_hex.SetVisible     (visible && hex);

    //  Opened or closed for the file, so the list is resized around the pane.
    if (toggled)
    {
        m_context.layout.RecomputeLayout();

        //  A view sized to the pane has the pane's size only now.
        FillPreview();
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::TakeThumbnail
//
//  A built thumbnail, on the UI thread; the pane is laid out again around
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::TakeThumbnail()
{
    m_thumbnail.TakeResult();
    m_context.layout.RecomputeLayout();
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::IsKnownPreview
//
//  A file of a type with a preview: a listing, text, a picture, 6502 code in
//  either form, a catalog or archive, an image's details, or the Windows
//  preview of a PC file. A file in the hex view only because no other
//  preview fits it is not one.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::IsKnownPreview (const PreviewContent & preview)
{
    using Kind = PreviewContent::Kind;

    return preview.kind == Kind::Listing || preview.kind == Kind::Text    || preview.kind == Kind::Catalog
        || preview.kind == Kind::Details || preview.kind == Kind::Shell   || preview.kind == Kind::Variables || preview.executable
        || preview.kind == Kind::Document || preview.kind == Kind::Grid
        || (preview.kind == Kind::Picture && !preview.bgra.empty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::SetPinned
//
//  A pane turned off stays off for the selected file, and automatic preview
//  opens it again for the next one.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::SetPinned (bool pinned)
{
    m_pinned  = pinned;
    m_autoKey = pinned ? std::wstring() : GetAutoKey();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::GetAutoKey
//
//  The location and the selected file's name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerPreviewManager::GetAutoKey() const
{
    const std::vector<int>  & selected = m_context.browser.GetSelectedRows();
    std::wstring              key      = m_context.browser.GetLocation().path;



    if (!selected.empty() && selected[0] >= 0 && (size_t) selected[0] < m_context.browser.GetRows().size())
    {
        key += L"|" + m_context.browser.GetRows()[(size_t) selected[0]].name;
    }

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::UpdateAutoPreview
//
//  With automatic preview on, the pane opens for a newly selected file
//  of a known type and closes for any other. A pane turned on manually
//  stays open until it is turned off manually. True when the pane opened
//  or closed.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::UpdateAutoPreview()
{
    const std::vector<int>  & selected = m_context.browser.GetSelectedRows();
    const PreviewContent    & preview  = m_context.browser.GetPreview();
    std::wstring              key      = GetAutoKey();
    bool                      known    = false;
    std::wstring              address  = BrowserModel::FormatAddress (m_context.browser.GetLocation());



    if (!m_context.prefs.autoPreview || m_pinned)
    {
        return false;
    }

    //  A pane automatic preview opened closes on going to another folder.
    if (address != m_autoAddress)
    {
        bool  wasOpen = m_context.prefs.previewVisible && !m_autoAddress.empty();

        m_autoAddress = address;
        m_autoKey     = key;

        if (wasOpen)
        {
            m_context.prefs.previewVisible = false;
            return true;
        }
    }

    //  A linked drive's file is checked once it arrives.
    if (key == m_autoKey || (preview.kind == PreviewContent::Kind::Error && preview.message == CassoExplorerBrowser::s_kWorkingText))
    {
        return false;
    }

    m_autoKey = key;
    known     = !selected.empty() && IsKnownPreview (preview);

    //  Automatic preview only opens the pane, so the list doesn't move under
    //  the pointer as the selection changes.
    if (!known || m_context.prefs.previewVisible)
    {
        return false;
    }

    m_context.prefs.previewVisible = true;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::UpdateShown
//
//  The pane is on screen while it is turned on and a file is selected, so the
//  list isn't narrowed by an empty pane. True when that changed.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::UpdateShown()
{
    bool  shown   = m_context.prefs.previewVisible && m_context.listPane.HasSelection();
    bool  changed = shown != m_shown;



    m_shown = shown;

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::PaintBackground
//
//  Every preview draws on the content surface, a message included, which
//  has no background of its own.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::PaintBackground (IDxuiPainter & painter, const IDxuiTheme & theme) const
{
    int  insetPx     = m_context.scaler.ToPx (kRuleInsetDip);
    int  lineThickPx = (std::max) (1, m_context.scaler.ToPx (1));



    if (!m_shown || m_rect.right <= m_rect.left)
    {
        return;
    }

    painter.FillRect ((float) m_rect.left, (float) m_rect.top,
                      (float) (m_rect.right - m_rect.left), (float) (m_rect.bottom - m_rect.top),
                      GetBackground (theme));

    //  The band between a WOZ image's details and its catalog has their
    //  background, with a rule across its middle.
    if (m_ruleBand.bottom > m_ruleBand.top)
    {
        painter.FillRect ((float) m_ruleBand.left, (float) m_ruleBand.top,
                          (float) (m_ruleBand.right - m_ruleBand.left), (float) (m_ruleBand.bottom - m_ruleBand.top),
                          theme.ContentBackground());
        painter.FillRect ((float) (m_ruleBand.left + insetPx), (float) ((m_ruleBand.top + m_ruleBand.bottom) / 2),
                          (float) (m_ruleBand.right - m_ruleBand.left - insetPx - insetPx), (float) lineThickPx,
                          theme.Divider());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::GetBackground
//
//  Explorer's Preview pane is a shade lighter than its file list in dark
//  mode: #1F1F1F beside #181818, measured. Light mode keeps the list's color.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t ExplorerPreviewManager::GetBackground (const IDxuiTheme & theme)
{
    uint32_t  color = theme.ContentBackground();
    uint32_t  red   = (color >> 16) & 0xFF;
    uint32_t  green = (color >>  8) & 0xFF;
    uint32_t  blue  =  color        & 0xFF;



    if (red + green + blue >= 3 * 128)
    {
        return color;
    }

    red   = (std::min) (red   + kBackgroundLift, 0xFFu);
    green = (std::min) (green + kBackgroundLift, 0xFFu);
    blue  = (std::min) (blue  + kBackgroundLift, 0xFFu);

    return (color & 0xFF000000) | (red << 16) | (green << 8) | blue;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::HandleMessageMouse
//
//  The message's mouse handler. A preview's message selects with the mouse,
//  as text does, and has the text's menu.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::HandleMessageMouse (const DxuiMouseEvent & ev)
{
    POINT  point   = ev.positionDip;
    bool   handled = false;



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
    {
        std::vector<DxuiPopupMenuItem>  items;

        m_context.input.SetFocusPane (ExplorerPane::Preview);
        m_context.input.ReleasePressed();
        m_context.menus.GetMenuCommands().clear();
        m_context.menus.AddMenuCommand (items, L"&Copy",       [this]() { m_message->Copy(); });
        m_context.menus.AddMenuCommand (items, L"Select &all", [this]() { m_message->SelectAll(); m_context.window.Invalidate(); });
        DxuiContextMenu::Show (*m_context.window.GetPopupHost(), point.x, point.y, std::move (items));
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
    }

    handled = m_message->OnMouse (ev);

    if (handled)
    {
        m_context.window.Invalidate();
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::HandleShortcutKey
//
//  An archive's files take the file list's shortcuts. A preview that is only
//  a message selects and copies as text does.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::HandleShortcutKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down || ev.alt)
    {
        return false;
    }

    if (m_listing.HandleShortcutKey (ev))
    {
        return true;
    }

    if (m_message->IsVisible() && ev.ctrl && (ev.vk == 'C' || ev.vk == 'A'))
    {
        m_message->InvokeCommand ((ev.vk == 'C') ? DxuiStandardCommand::Copy : DxuiStandardCommand::SelectAll);
        m_context.window.Invalidate();
        return true;
    }

    //  A text or hex preview copies and selects all, as its menu does.
    if ((m_text.IsShowing() || m_hex.IsShowing()) && ev.ctrl && !ev.shift && (ev.vk == 'C' || ev.vk == 'A'))
    {
        GetFocusedControl()->InvokeCommand ((ev.vk == 'C') ? DxuiStandardCommand::Copy : DxuiStandardCommand::SelectAll);
        m_context.statusBar.Fill();
        m_context.window.Invalidate();
        return true;
    }

    //  Any other preview copies the whole view.
    if (ev.ctrl && !ev.shift && ev.vk == 'C' && ExplorerPreviewOutput::CanCopy (m_context))
    {
        ExplorerPreviewOutput::Copy (m_context);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::HandleKey
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::HandleKey (const DxuiKeyEvent & ev)
{
    bool  handled = m_hex.IsShowing()  ? m_hex.HandleKey (ev)
                  : m_text.IsShowing() ? m_text.HandleKey (ev)
                                       : m_listing.HandleKey (ev);



    m_context.statusBar.Fill();

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::HandleFocusChanged
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::HandleFocusChanged (ExplorerPane focus)
{
    m_listing.HandleFocusChanged (focus == ExplorerPane::Preview);
    m_hex.HandleFocusChanged     (focus == ExplorerPane::Preview);

    m_toolbar->SetFocusIndex (focus == ExplorerPane::PreviewToolbar ? m_barFocus : -1);
    m_find.HandleFocusChanged (focus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::UpdateScrollbarHover
//
//  Returns whether any of the views' scrollbars changed.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::UpdateScrollbarHover (POINT at)
{
    bool  changed = false;



    for (PreviewView * view : m_views)
    {
        changed = view->UpdateScrollbarHover (at) || changed;
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::TickScrollbars (int64_t nowMs)
{
    bool  moved = false;



    for (PreviewView * view : m_views)
    {
        moved = view->TickScrollbars (nowMs) || moved;
    }

    return moved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::IsOverPane
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::IsOverPane (POINT point) const
{
    return m_shown && ExplorerLayout::Contains (m_rect, point);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::GetCursorForPoint
//
//  The preview's text -- a listing, a catalog, a hex dump -- selects as text
//  does, so the cursor over it is the I-beam, except over its scrollbars.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ExplorerPreviewManager::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = m_text.GetCursorForPoint (clientPx);



    cursor = (cursor != nullptr) ? cursor : m_hex.GetCursorForPoint (clientPx);
    cursor = (cursor != nullptr) ? cursor : m_listing.GetCursorForPoint (clientPx);

    if (cursor == nullptr && m_message->IsVisible())
    {
        cursor = m_message->GetCursorForPoint (clientPx);
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::GetScrollbarMenu
//
//  The menu of a view's scrollbar under the point, or none.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerPreviewManager::GetScrollbarMenu (const DxuiMouseEvent & ev) const
{
    std::vector<DxuiPopupMenuItem>  items;



    if (m_listing.TryGetScrollbarMenu (ev, items) || m_text.TryGetScrollbarMenu (ev, items)
        || m_hex.TryGetScrollbarMenu (ev, items) || m_picture.TryGetScrollbarMenu (ev, items))
    {
        return items;
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::GetFocusedControl
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * ExplorerPreviewManager::GetFocusedControl() const
{
    return m_hex.IsShowing()  ? (IDxuiControl *) m_hex.GetView()
         : m_text.IsShowing() ? m_text.GetControl()
                              : (IDxuiControl *) m_listing.GetList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::TryGetSelectionDetail
//
//  Bytes or characters selected, as the status bar's detail.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::TryGetSelectionDetail (std::wstring & outDetail) const
{
    return m_hex.TryGetSelectionDetail (outDetail) || m_text.TryGetSelectionDetail (outDetail);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::SaveTabState
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::SaveTabState()
{
    m_context.browser.SetTabPreview (m_context.prefs.previewVisible);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::RestoreTabState
//
//  An automatic pane is as the tab left it. A pane turned on manually stays
//  open in every tab.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::RestoreTabState()
{
    bool  open = m_context.browser.IsTabPreview();



    if (!m_context.prefs.autoPreview || m_pinned)
    {
        return;
    }

    m_autoAddress = BrowserModel::FormatAddress (m_context.browser.GetLocation());
    m_autoKey     = GetAutoKey();

    if (open != m_context.prefs.previewVisible)
    {
        m_context.prefs.previewVisible = open;
        UpdateShown();
        m_context.layout.RecomputeLayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::HandleAutoPreviewTimer
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::HandleAutoPreviewTimer()
{
    FillPreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::GetShownZoom
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerPreviewManager::GetShownZoom() const
{
    return m_picture.IsVisible() ? m_pictureZoom : m_context.prefs.previewZoom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::SetPreviewZoom
//
//  The text, hex and picture previews share one zoom, kept within the range
//  the preferences allow.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::SetPreviewZoom (int percent)
{
    int    clamped = (std::max) (CassoExplorerPrefs::kMinPreviewZoom, (std::min) (percent, CassoExplorerPrefs::kMaxPreviewZoom));
    float  zoom    = (float) clamped / 100.0f;



    //  A picture's zoom is for that picture; text and hex keep theirs.
    if (m_picture.IsVisible())
    {
        m_pictureZoom = clamped;
        m_picture.SetZoom (zoom);
    }
    else
    {
        m_context.prefs.previewZoom = clamped;

        m_text.SetZoom (zoom);
        m_hex.SetZoom  (zoom);
    }

    m_context.statusBar.Fill();
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::IsArchiveLocation
//
//  Whether the list shows the inside of an archive: a shell folder whose path
//  lies on a lettered drive but is no folder on it.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::IsArchiveLocation() const
{
    const Location &  location = m_context.browser.GetLocation();



    return location.kind == Location::Kind::ShellFolder && location.path.size() > 3 && location.path[1] == L':'
        && m_context.app.fs != nullptr && !m_context.app.fs->DirectoryExists (location.path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::GetPreviewStopIndex
//
//  A command's place among the hex view toolbar's stops, or -1 while that
//  toolbar is not showing.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerPreviewManager::GetPreviewStopIndex (int commandId) const
{
    std::vector<int>  ids = CassoExplorerCommands::GetPreviewToolbarCommandIds (m_barMode, IsExpanded());



    if (m_barMode != CassoExplorerCommands::kPreviewBarHex && m_barMode != CassoExplorerCommands::kPreviewBarVariables)
    {
        return -1;
    }

    for (size_t i = 0; i < ids.size(); i++)
    {
        if (ids[i] == commandId)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::IsExpanded
//
//  The tab's preview over the whole tab, while a file is selected.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::IsExpanded() const
{
    return m_shown && m_context.browser.IsTabPreviewExpanded();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::SetExpanded
//
//  With more than one file selected, the files are the step set and the
//  first of them is selected alone. Collapsing ends the step set and puts
//  the keyboard on the list.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::SetExpanded (bool expanded)
{
    const std::vector<int> &   selected = m_context.browser.GetSelectedRows();
    std::vector<std::wstring>  keys;
    int                        first    = -1;



    m_stepSet.clear();

    if (expanded && selected.size() > 1)
    {
        m_context.browser.GetRowKeys (keys);

        for (int row : selected)
        {
            if (row >= 0 && (size_t) row < keys.size())
            {
                m_stepSet.push_back (keys[(size_t) row]);
            }
        }

        first = PreviewSteps::GetTarget (m_context.browser.GetRows(), keys, m_stepSet, -1, true);

        if (first >= 0)
        {
            SelectStep (first);
        }
    }

    m_context.browser.SetTabPreviewExpanded (expanded);
    FillPreview();
    m_context.layout.RecomputeLayout();
    m_context.input.SetFocusPane (IsExpanded() ? ExplorerPane::Preview : ExplorerPane::List);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::GetStepTarget
//
//  The row previous or next goes to, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerPreviewManager::GetStepTarget (bool forward) const
{
    const std::vector<int> &   selected = m_context.browser.GetSelectedRows();
    std::vector<std::wstring>  keys;
    int                        current  = (selected.size() == 1) ? selected[0] : -1;



    m_context.browser.GetRowKeys (keys);

    return PreviewSteps::GetTarget (m_context.browser.GetRows(), keys, m_stepSet, current, forward);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::CanStep
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::CanStep (bool forward) const
{
    return IsExpanded() && GetStepTarget (forward) >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::Step
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::Step (bool forward)
{
    int  target = IsExpanded() ? GetStepTarget (forward) : -1;



    if (target >= 0)
    {
        SelectStep (target);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::SelectStep
//
//  The row selected alone and scrolled to, as a click would select it,
//  keeping the step set.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::SelectStep (int row)
{
    m_isStepping = true;
    m_context.listPane.SetListSelection ({ row });
    m_context.listPane.GetListView()->EnsureVisible (row);
    m_isStepping = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::HandleSelectionChanged
//
//  A selection made any way but by a step ends the step set.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::HandleSelectionChanged()
{
    if (!m_isStepping)
    {
        m_stepSet.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::HandleEnterKey
//
//  Enter on a file, not a folder, disk image or archive, which open as they
//  always do, expands its preview when Options sets Enter to, turning the
//  pane on first if it's off.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::HandleEnterKey (const DxuiKeyEvent & ev)
{
    const std::vector<int> &  selected = m_context.browser.GetSelectedRows();
    bool                      plain    = ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_RETURN && !ev.ctrl && !ev.alt && !ev.shift;
    bool                      isFile   = selected.size() == 1 && selected[0] >= 0 && (size_t) selected[0] < m_context.browser.GetRows().size();



    if (!plain || !m_context.prefs.enterExpandsPreview || !isFile)
    {
        return false;
    }

    {
        const CatalogRow &  row = m_context.browser.GetRows()[(size_t) selected[0]];

        if (row.isDirectory || row.isDiskImage || row.isArchive)
        {
            return false;
        }
    }

    if (!m_context.prefs.previewVisible)
    {
        m_context.prefs.previewVisible = true;
        SetPinned (true);
        FillPreview();
    }

    SetExpanded (true);

    return IsExpanded();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::HandleExpandedKey
//
//  Esc collapses the preview. Left and Right step while the keyboard is on
//  the preview, since the toolbar takes them to move between its buttons.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::HandleExpandedKey (const DxuiKeyEvent & ev, ExplorerPane focus)
{
    bool  plain = ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt && !ev.shift;



    if (!IsExpanded() || !plain)
    {
        return false;
    }

    if (ev.vk == VK_ESCAPE)
    {
        SetExpanded (false);
        return true;
    }

    if ((ev.vk == VK_LEFT || ev.vk == VK_RIGHT) && focus == ExplorerPane::Preview)
    {
        Step (ev.vk == VK_RIGHT);
        m_context.window.Invalidate();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::ChooseRecordLength
//
//  Opens the prompt for the record length of the previewed random-access
//  text file, prefilled with the current one, and opens it again after text
//  that isn't a length. An empty answer or 0 is the length found in the file.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::ChooseRecordLength()
{
    std::wstring  text   = std::to_wstring (m_context.browser.GetPreview().recordLength);
    size_t        length = 0;
    bool          parsed = false;



    while (!parsed && CassoExplorerPromptDialog::Ask (m_context.window.GetHwnd(), m_context.theme, L"Record length",
                                                       L"Bytes in each record, or 0 for the length found in the file:", text, kRecordLengthDigits, text))
    {
        parsed = TryParseRecordLength (text, length);

        if (!parsed)
        {
            m_context.window.ShowMessage (L"Type a length from 1 to 65535, or 0.", MB_ICONWARNING);
        }
    }

    if (parsed)
    {
        m_context.browser.SetPreviewRecordLength (length);
        FillPreview();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::TryParseRecordLength
//
//  A record length in decimal digits, from 0 to 65535. Empty text is 0.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::TryParseRecordLength (const std::wstring & text, size_t & outLength)
{
    size_t  value = 0;
    bool    valid = text.size() <= kRecordLengthDigits;



    for (wchar_t ch : text)
    {
        valid = valid && ch >= L'0' && ch <= L'9';
        value = value * 10 + (size_t) (ch - L'0');
    }

    valid = valid && value <= kRecordLengthMax;

    if (valid)
    {
        outLength = value;
    }

    return valid;
}
