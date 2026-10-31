#include "Pch.h"
#include "Core/AppTimerIds.h"

#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Model/PreviewText.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"





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

    m_find.Create();
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
    const DxuiDpiScaler  & scaler  = m_context.scaler;
    RECT                   content = rect;
    int                    barPx   = 0;



    m_rect = rect;
    m_toolbar->SetVisible (m_barMode != CassoExplorerCommands::kPreviewBarNone);

    if (m_barMode != CassoExplorerCommands::kPreviewBarNone)
    {
        m_toolbar->PlanForWidth (rect.right - rect.left, scaler);
        barPx = scaler.ToPx (m_toolbar->GetBandDp());

        m_toolbar->SetHostClientRect (m_context.layout.GetClient());
        m_toolbar->Layout (RECT { rect.left, rect.top, rect.right, rect.top + barPx }, scaler);
        content.top += barPx;
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
    m_toolbar->SetVisible (false);
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
    bool                    toggled  = UpdateAutoPreview();
    bool                    visible  = m_context.prefs.previewVisible;
    bool                    picture  = preview.kind == PreviewContent::Kind::Picture && !preview.bgra.empty();
    bool                    hostFile = preview.kind == PreviewContent::Kind::Hex && !preview.hostPath.empty();
    HostCharset             charset  = preview.hostCharset;
    bool                    opened   = m_hex.OpenHostFile (preview, charset);
    bool                    error    = preview.kind == PreviewContent::Kind::Error || (hostFile && !opened);
    bool                    hex      = preview.kind == PreviewContent::Kind::Hex && (!preview.bytes.empty() || opened);
    bool                    disasm   = preview.kind == PreviewContent::Kind::Hex && preview.bytes.empty() && !preview.lines.empty();
    bool                    columns  = preview.kind == PreviewContent::Kind::Catalog && preview.lines.empty();
    bool                    program  = preview.kind == PreviewContent::Kind::Listing && !preview.bytes.empty();
    bool                    forms    = picture && m_context.browser.CanTogglePictureColor();
    bool                    shell    = preview.kind == PreviewContent::Kind::Shell;
    int                     mode     = !visible ? CassoExplorerCommands::kPreviewBarNone
                                     : hex      ? CassoExplorerCommands::kPreviewBarHex
                                     : disasm   ? CassoExplorerCommands::kPreviewBarHex
                                     : program  ? CassoExplorerCommands::kPreviewBarListing
                                     : forms    ? CassoExplorerCommands::kPreviewBarPicture
                                     : picture  ? CassoExplorerCommands::kPreviewBarSave
                                                : CassoExplorerCommands::kPreviewBarNone;
    bool                    blank    = false;



    if (preview.kind != m_lastKind || picture || shell)
    {
        m_pictureZoom = CassoExplorerPrefs::kDefaultPreviewZoom;
        m_picture.SetZoom (1.0f);
    }

    m_lastKind = preview.kind;

    m_shell.Update   (preview, m_content);
    m_picture.Fill   (preview, picture, shell && m_shell.IsPicture());
    m_listing.Fill   (preview, columns);
    m_text.Fill      (preview);

    if (mode != m_barMode)
    {
        m_barMode = mode;

        if (mode != CassoExplorerCommands::kPreviewBarNone)
        {
            m_toolbar->SetEntries (m_context.commands.BuildPreviewToolbarEntries (mode, m_find.GetSearchBox(), m_find.GetGoToBox()));
        }

        //  The boxes go with the hex view's toolbar, and their focus with it.
        if (mode != CassoExplorerCommands::kPreviewBarHex && (m_context.input.GetFocusPane() == ExplorerPane::Search || m_context.input.GetFocusPane() == ExplorerPane::GoTo))
        {
            m_context.input.SetFocusPane (ExplorerPane::Preview);
        }

        m_context.layout.RecomputeLayout();
    }

    m_hex.Fill (preview, hex, opened, charset);

    //  With nothing selected the pane says so, as Explorer's does.
    blank = !m_context.listPane.HasSelection() && !picture && !shell && !error && !hex && !columns && !program;

    m_message->SetText (blank                   ? std::wstring (kSelectToPreview)
                        : !error                ? std::wstring()
                        : (hostFile && !opened) ? PreviewText::FormatPreviewError (L"The file could not be read. " + m_hex.DescribeFileError())
                                                : PreviewText::FormatPreviewError (preview.message));
    m_message->SetVisible (visible && (error || blank));

    //  Explorer's prompt is smaller body text; an error is muted. Both are
    //  centered in the pane.
    ExplorerTextStyles::Apply (*m_message, blank ? ExplorerTextStyles::kPanePrompt : ExplorerTextStyles::kPaneMessage);

    m_picture.SetVisible (visible && (picture || (shell && m_shell.IsPicture())));
    m_listing.SetVisible (visible && columns);
    m_text.SetVisible    (visible && !picture && !shell && !error && !hex && !columns && !blank);
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
//  ExplorerPreviewManager::IsKnownPreview
//
//  A file previewed as more than its bytes: a listing, text, a picture, a
//  catalog or archive, an image's details, or the Windows preview of a PC
//  file.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::IsKnownPreview (const PreviewContent & preview)
{
    using Kind = PreviewContent::Kind;

    return preview.kind == Kind::Listing || preview.kind == Kind::Text    || preview.kind == Kind::Catalog
        || preview.kind == Kind::Details || preview.kind == Kind::Shell
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
//  ExplorerPreviewManager::PaintBackground
//
//  Every preview draws on the content surface, a message included, which
//  has no background of its own.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewManager::PaintBackground (IDxuiPainter & painter, const IDxuiTheme & theme) const
{
    if (!m_context.prefs.previewVisible || m_rect.right <= m_rect.left)
    {
        return;
    }

    painter.FillRect ((float) m_rect.left, (float) m_rect.top,
                      (float) (m_rect.right - m_rect.left), (float) (m_rect.bottom - m_rect.top),
                      GetBackground (theme));
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
//  A preview's message selects with the mouse, as text does, and has the
//  text's menu. The window routes events to the message ahead of its other
//  panes.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::HandleMessageMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;



    if (!m_message->IsVisible() || (!m_message->IsInteracting() && !ExplorerLayout::Contains (m_message->GetBounds(), point)))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
    {
        std::vector<DxuiPopupMenuItem>  items;

        m_context.input.SetFocusPane (ExplorerPane::Preview);
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

    if (!m_message->OnMouse (ev))
    {
        return false;
    }

    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager::HandleMouse
//
//  Events go to a view mid-drag until the button comes up, wherever the
//  pointer is, and otherwise to the view under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewManager::HandleMouse (const DxuiMouseEvent & ev)
{
    if (m_listing.IsArchive() && m_listing.GetArchive().HandleMouse (ev, ev.positionDip))
    {
        m_context.window.Invalidate();
        return true;
    }

    return m_listing.HandleCapturedMouse (ev)
        || m_hex.HandleCapturedMouse     (ev)
        || m_picture.HandleMouse         (ev)
        || m_text.HandleCapturedMouse    (ev)
        || m_hex.HandleMouse             (ev)
        || m_text.HandleMouse            (ev)
        || m_listing.HandleMouse         (ev);
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
    return m_context.prefs.previewVisible && ExplorerLayout::Contains (m_rect, point);
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
    std::vector<int>  ids = CassoExplorerCommands::GetPreviewToolbarCommandIds (true);



    if (m_barMode != CassoExplorerCommands::kPreviewBarHex)
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
