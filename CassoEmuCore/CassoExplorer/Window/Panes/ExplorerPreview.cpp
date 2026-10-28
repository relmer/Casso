#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerPreview.h"
#include "CassoExplorer/Model/PreviewText.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
#include "Config/CrtResolver.h"
#include "Config/CrtPresets.h"
#include "Core/AppTimerIds.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ExplorerPreview
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPreview::ExplorerPreview (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::Create
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::Create()
{
    CassoExplorerNamedControl<DxuiListView>         * list    = m_context.window.CreateChild<CassoExplorerNamedControl<DxuiListView>>();
    CassoExplorerNamedControl<DxuiFramebufferView>  * picture = m_context.window.CreateChild<CassoExplorerNamedControl<DxuiFramebufferView>>();
    CassoExplorerNamedControl<DxuiHexView>          * hexView = m_context.window.CreateChild<CassoExplorerNamedControl<DxuiHexView>>();



    list->SetAccessibleName    (L"Preview");
    picture->SetAccessibleName (L"Picture preview");
    hexView->SetAccessibleName (L"Bytes");

    m_previewList    = list;
    m_picture        = picture;
    m_hexView        = hexView;
    m_textView       = m_context.window.CreateChild<DxuiTextView>();
    m_previewMessage = m_context.window.CreateChild<DxuiSelectableText>();
    m_previewMessage->SetTextRole (DxuiTextRole::Muted);
    m_previewMessage->SetAlign    (DxuiTextHAlign::Center, DxuiTextVAlign::Center);

    m_find.Create();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::CreateToolbar
//
//  The preview's toolbar is the window's last child, so it is drawn over the
//  others.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::CreateToolbar()
{
    m_previewToolbar = m_context.window.CreateChild<DxuiToolbar>();
    m_previewToolbar->SetFlatStyle    (true);
    m_previewToolbar->SetTextRenderer (m_context.window.GetTextRenderer());
    m_previewToolbar->SetPopupHost    (m_context.window.GetPopupHost());
    m_previewToolbar->SetIconDip      (ExplorerToolbars::kNavIconDip);
    m_previewToolbar->SetVisible      (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::Configure()
{
    const CassoExplorerPrefs  & prefs   = m_context.prefs;
    float                       zoom    = (float) prefs.previewZoom / 100.0f;
    bool                        grouped = false;
    HRESULT                     hr      = S_OK;



    m_previewList->SetShowHeader (false);
    m_previewList->SetColumns ({ DxuiListView::Column { L"", 0, true } });

    //  A hex dump and a disassembly are columns of fixed-width text, and read
    //  as such only in a monospace face on rows close to the line height.
    m_previewList->SetMonospace (true);
    m_previewList->SetRowHeightDip (kRowHeightDip);
    m_previewList->SetHorizontalScrollEnabled (true);
    m_previewList->SetPreciseAutoFit (true);
    m_previewList->SetMultiSelect (true);
    m_previewList->SetAlwaysShowSelection (true);
    m_previewList->SetTextSelectionColors (true);
    m_previewList->SetOwnerWindow (m_context.window.GetHwnd());

    //  An archive's files open on a double-click, as the file list's do.
    m_previewList->SetActivateOnDoubleClick (true);
    m_previewList->SetOnActivateRow ([this] (int row)
    {
        if (m_previewArchive)
        {
            m_archive.OpenRow (row);
        }
    });

    m_textView->SetOwnerWindow   (m_context.window.GetHwnd());
    m_textView->SetIconFace      (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                                  ? DxuiToolbar::kFluentIconFace
                                  : DxuiToolbar::kMdl2IconFace);
    m_textView->SetOnContextMenu ([this] (POINT at) { ShowTextContextMenu (at.x, at.y); });

    //  The bytes are Apple text in the column on the right, which is what
    //  the files here hold; the grouping is the user's, kept between runs.
    m_hexView->SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
    m_hexView->SetOwnerWindow  (m_context.window.GetHwnd());
    m_hexView->SetPaddingDip   (6);
    m_hexView->SetColumns      (prefs.hexColumns);
    m_hexView->SetValueFormat  (ParseHexFormat (prefs.hexFormat));
    m_hexView->SetShowValues   (prefs.hexShowValues);
    m_hexView->SetBreakLines   (true);
    m_hexView->SetTextStrength (kTextStrength);
    m_hexView->SetZoom         (zoom);
    m_textView->SetTextStrength (kTextStrength);
    m_textView->SetZoom         (zoom);
    m_picture->SetZoom          (zoom);

    //  A picture fills the pane at any size, as Explorer's preview does, not
    //  only at whole multiples of its own.
    m_picture->SetScaling (false, true);

    m_previewMessage->SetTextRenderer (m_context.window.GetTextRenderer());
    m_previewMessage->SetOwnerWindow  (m_context.window.GetHwnd());

    //  Windows' own previews of host files, hosted over the pane.
    hr = m_shellPreview.Start (m_context.window.GetHwnd(), (UINT) ExplorerMessage::ShellPreview);
    IGNORE_RETURN_VALUE (hr, S_OK);

    grouped = m_hexView->SetGrouping (prefs.hexGrouping);
    IGNORE_RETURN_VALUE (grouped, true);

    m_hexView->SetOnSelectionChanged ([this] () { m_context.statusBar.Fill(); });

    //  The hex view's menu is its own; Go to is the browser's, added to it,
    //  and the view's choices are kept for the next run.
    m_hexView->SetOnBuildContextMenu ([this] (std::vector<DxuiPopupMenuItem> & items)
    {
        std::shared_ptr<const DxuiCommand>  goTo = m_context.commands.Find (CassoExplorerCommands::kGoToOffset);

        if (goTo != nullptr)
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (goTo));
        }
    });

    m_hexView->SetOnSettingsChanged ([this]()
    {
        CassoExplorerPrefs &  settings = m_context.prefs;

        settings.hexShowValues = m_hexView->IsShowingValues();
        settings.hexGrouping   = m_hexView->GetGrouping();
        settings.hexColumns    = m_hexView->GetColumns();
        settings.hexFormat     = (m_hexView->GetValueFormat() == DxuiHexView::ValueFormat::Signed)   ? CassoExplorerPrefs::kHexFormatSigned
                               : (m_hexView->GetValueFormat() == DxuiHexView::ValueFormat::Unsigned) ? CassoExplorerPrefs::kHexFormatUnsigned
                                                                                                      : CassoExplorerPrefs::kHexFormatHex;
        m_context.window.Invalidate();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::Layout (const RECT & rect)
{
    const DxuiDpiScaler  & scaler  = m_context.scaler;
    RECT                   content = rect;
    int                    barPx   = 0;
    int                    insetPx = scaler.ToPx (kPictureInsetDip);



    m_previewRect = rect;
    m_previewToolbar->SetVisible (m_previewBarMode != CassoExplorerCommands::kPreviewBarNone);

    //  The toolbar sits across the pane's top, and the content below it.
    if (m_previewBarMode != CassoExplorerCommands::kPreviewBarNone)
    {
        m_previewToolbar->PlanForWidth (rect.right - rect.left, scaler);
        barPx = scaler.ToPx (m_previewToolbar->GetBandDp());

        m_previewToolbar->SetHostClientRect (m_context.layout.GetClient());
        m_previewToolbar->Layout (RECT { rect.left, rect.top, rect.right, rect.top + barPx }, scaler);
        content.top += barPx;
    }

    m_previewContent = content;
    m_previewList->Layout (content, scaler);

    if (!m_shellPreviewPath.empty())
    {
        m_shellPreview.SetRect (content);
    }

    m_textView->Layout       (content, scaler);
    m_hexView->Layout        (content, scaler);
    m_picture->Layout        (RECT { content.left + insetPx, content.top + insetPx, content.right - insetPx, content.bottom - insetPx }, scaler);
    m_previewMessage->Layout (m_context.layout.GetMessageRect (content), scaler);

    UpdateCrtPicture (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::Hide
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::Hide()
{
    m_previewList->SetVisible    (false);
    m_hexView->SetVisible        (false);
    m_textView->SetVisible       (false);
    m_picture->SetVisible        (false);
    m_previewMessage->SetVisible (false);
    m_previewToolbar->SetVisible (false);

    if (!m_shellPreviewPath.empty())
    {
        m_shellPreview.Unload();
        m_shellPreviewPath.clear();
        m_shellPicture = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::PaintBackground
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::PaintBackground (IDxuiPainter & painter, const IDxuiTheme & theme) const
{
    if (!m_context.prefs.previewVisible || m_previewRect.right <= m_previewRect.left)
    {
        return;
    }

    painter.FillRect ((float) m_previewRect.left, (float) m_previewRect.top,
                      (float) (m_previewRect.right - m_previewRect.left), (float) (m_previewRect.bottom - m_previewRect.top),
                      GetBackground (theme));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetBackground
//
//  Explorer's Preview pane is a shade lighter than its file list in dark
//  mode: #1F1F1F beside #181818, measured. Light mode keeps the list's color.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t ExplorerPreview::GetBackground (const IDxuiTheme & theme)
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
//  ExplorerPreview::HandleMessageMouse
//
//  A preview's message selects with the mouse, as text does, and has the
//  text's own menu.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::HandleMessageMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;



    if (!m_previewMessage->IsVisible() || (!m_previewMessage->IsInteracting() && !ExplorerLayout::Contains (m_previewMessage->GetBounds(), point)))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
    {
        std::vector<DxuiPopupMenuItem>  items;

        m_context.input.SetFocusPane (ExplorerPane::Preview);
        m_context.menus.GetMenuCommands().clear();
        m_context.menus.AddMenuCommand (items, L"&Copy",       [this]() { m_previewMessage->Copy(); });
        m_context.menus.AddMenuCommand (items, L"Select &all", [this]() { m_previewMessage->SelectAll(); m_context.window.Invalidate(); });
        DxuiContextMenu::Show (*m_context.window.GetPopupHost(), point.x, point.y, std::move (items));
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
    }

    if (!m_previewMessage->OnMouse (ev))
    {
        return false;
    }

    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::HandleMouse
//
//  Events go to a widget mid-drag until the button comes up, wherever the
//  pointer is, and otherwise to the widget under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::HandleMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;
    bool   press = ev.kind == DxuiMouseEventKind::Down;



    if (m_previewArchive && m_archive.HandleMouse (ev, point))
    {
        m_context.window.Invalidate();
        return true;
    }

    //  A drag selecting lines keeps the pointer until the button comes up, as
    //  the file list's does.
    if (m_previewList->IsInteracting())
    {
        m_previewList->OnMouse (ExplorerLayout::ToLocal (ev, m_previewList->GetBounds()));
        m_context.window.Invalidate();
        return true;
    }

    if (m_hexView->IsDragging())
    {
        m_hexView->OnMouse (ev);
        m_context.window.Invalidate();
        return true;
    }

    //  A double hi-res picture's own menu switches it between color and its
    //  bits, whichever the file's name or dots chose.
    if (press && ev.button == DxuiMouseButton::Right && m_picture->IsVisible() && ExplorerLayout::Contains (m_picture->GetBounds(), point)
        && m_context.browser.CanTogglePictureColor())
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
        ShowPictureContextMenu (point.x, point.y);
        return true;
    }

    //  A zoomed picture pans by dragging, its scrollbars and the wheel, and
    //  keeps the pointer while a drag is under way.
    if (m_picture->IsInteracting()
        || (m_picture->IsVisible() && ExplorerLayout::Contains (m_picture->GetBounds(), point) && m_picture->OnMouse (ev)))
    {
        if (m_picture->IsInteracting())
        {
            m_picture->OnMouse (ev);
        }

        if (press)
        {
            m_context.input.SetFocusPane (ExplorerPane::Preview);
        }

        m_context.window.Invalidate();
        return true;
    }

    if (m_textView->IsInteracting())
    {
        m_textView->OnMouse (ev);
        m_context.statusBar.Fill();
        m_context.window.Invalidate();
        return true;
    }

    //  DxuiHexView takes points in the same coordinates as its bounds, so the
    //  event is passed unchanged, not converted to widget-local coordinates as
    //  the older list views require.
    if (m_hexView->IsVisible() && ExplorerLayout::Contains (m_hexView->GetBounds(), point))
    {
        if (press)
        {
            m_context.input.SetFocusPane (ExplorerPane::Preview);
        }

        m_hexView->OnMouse (ev);
        m_context.window.Invalidate();
        return true;
    }

    if (m_textView->IsVisible() && ExplorerLayout::Contains (m_textView->GetBounds(), point))
    {
        if (press)
        {
            m_context.input.SetFocusPane (ExplorerPane::Preview);
        }

        m_textView->OnMouse (ev);
        m_context.statusBar.Fill();
        m_context.window.Invalidate();
        return true;
    }

    if (m_previewList->IsVisible() && ExplorerLayout::Contains (m_previewList->GetBounds(), point))
    {
        if (press)
        {
            m_context.input.SetFocusPane (ExplorerPane::Preview);
        }

        m_previewList->OnMouse (ExplorerLayout::ToLocal (ev, m_previewList->GetBounds()));
        m_context.window.Invalidate();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::UpdateScrollbarHover
//
//  Returns whether any of the views' scrollbars changed.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::UpdateScrollbarHover (POINT at)
{
    int  changed = (int) m_hexView->SetScrollbarHover (at)
                 | (int) m_textView->SetScrollbarHover (at)
                 | (int) m_previewList->SetScrollbarHover (at)
                 | (int) m_picture->SetScrollbarHover (at);



    return changed != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::TickScrollbars (int64_t nowMs)
{
    int  moved = (int) m_hexView->TickScrollbars (nowMs)
               | (int) m_textView->TickScrollbars (nowMs)
               | (int) m_previewList->TickScrollbars (nowMs)
               | (int) m_picture->TickScrollbars (nowMs);



    return moved != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::IsOverPane
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::IsOverPane (POINT point) const
{
    return m_context.prefs.previewVisible && ExplorerLayout::Contains (m_previewRect, point);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetCursorForPoint
//
//  The preview's text -- a listing, a catalog, a hex dump -- selects as text
//  does, so the cursor over it is the I-beam, except over its scrollbars.
//  Over an archive's rows it is the arrow, as over the file list's.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ExplorerPreview::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = nullptr;



    for (IDxuiControl * view : { (IDxuiControl *) m_textView, (IDxuiControl *) m_hexView })
    {
        if (cursor == nullptr && view->IsVisible() && ExplorerLayout::Contains (view->GetBounds(), clientPx) && !view->IsOverScrollbar (clientPx))
        {
            cursor = IDC_IBEAM;
        }
    }

    //  The list tests points in its own terms.
    if (cursor == nullptr && m_previewList->IsVisible() && ExplorerLayout::Contains (m_previewList->GetBounds(), clientPx))
    {
        RECT    list  = m_previewList->GetBounds();
        POINT   local = { clientPx.x - list.left, clientPx.y - list.top };

        cursor = m_previewList->GetCursorForPoint (local);
        cursor = (cursor != nullptr || m_previewList->IsOverScrollbar (local)) ? cursor : (m_previewArchive ? IDC_ARROW : IDC_IBEAM);
    }

    if (cursor == nullptr && m_previewMessage->IsVisible())
    {
        cursor = m_previewMessage->GetCursorForPoint (clientPx);
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetPictureCursor
//
//  The picture's cursor. ExplorerInput::GetCursorForPoint calls this after
//  the address bar's and the preview splitter's lookups.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ExplorerPreview::GetPictureCursor (POINT clientPx) const
{
    if (!m_picture->IsVisible() || !ExplorerLayout::Contains (m_picture->GetBounds(), clientPx))
    {
        return nullptr;
    }

    return m_picture->GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetScrollbarMenu
//
//  The menu of a view's scrollbar under the point, or none. The list takes
//  points relative to its own bounds, as its HandleMouse does; the other views
//  take the window's.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerPreview::GetScrollbarMenu (const DxuiMouseEvent & ev) const
{
    POINT  point = ev.positionDip;



    if (m_previewList->IsVisible() && ExplorerLayout::Contains (m_previewList->GetBounds(), point))
    {
        return m_previewList->GetScrollbarMenu (ExplorerLayout::ToLocal (ev, m_previewList->GetBounds()).positionDip);
    }

    if (m_textView->IsVisible() && ExplorerLayout::Contains (m_textView->GetBounds(), point))
    {
        return m_textView->GetScrollbarMenu (point);
    }

    if (m_hexView->IsVisible() && ExplorerLayout::Contains (m_hexView->GetBounds(), point))
    {
        return m_hexView->GetScrollbarMenu (point);
    }

    if (m_picture->IsVisible() && ExplorerLayout::Contains (m_picture->GetBounds(), point))
    {
        return m_picture->GetScrollbarMenu (point);
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::HandleShortcutKey
//
//  An archive's files copy and cut as files do, for the shell to extract on
//  paste, Ctrl+A selects them all, and Delete deletes them for good, as in
//  the file list. A preview that is only a message selects and copies as
//  text does.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::HandleShortcutKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down || ev.alt)
    {
        return false;
    }

    if (m_previewArchive && ev.ctrl && ev.vk == 'A')
    {
        m_previewList->SelectAllRows();
        m_context.window.Invalidate();
        return true;
    }

    if (m_previewArchive && ev.ctrl && (ev.vk == 'C' || ev.vk == 'X'))
    {
        m_archive.CopySelection (ev.vk == 'X');
        return true;
    }

    if (m_previewArchive && !ev.ctrl && ev.vk == VK_DELETE)
    {
        m_archive.DeleteSelection();
        return true;
    }

    if (m_previewMessage->IsVisible() && ev.ctrl && (ev.vk == 'C' || ev.vk == 'A'))
    {
        m_previewMessage->InvokeCommand ((ev.vk == 'C') ? DxuiStandardCommand::Copy : DxuiStandardCommand::SelectAll);
        m_context.window.Invalidate();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::HandleTabKey
//
//  The hex view's two columns are stops of their own inside the pane: Tab
//  moves between them while it has one left, and only then does the walk
//  move on to the next pane.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::HandleTabKey (const DxuiKeyEvent & ev)
{
    return IsHexPreviewShowing() && m_hexView->OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::HandleFocusEntered
//
//  A Tab into the pane starts at the hex view's column the walk's direction
//  calls for.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::HandleFocusEntered (bool forward)
{
    if (IsHexPreviewShowing())
    {
        m_hexView->OnFocusEntered (forward);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::HandleKey
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::HandleKey (const DxuiKeyEvent & ev)
{
    bool  handled = IsHexPreviewShowing()  ? m_hexView->OnKey (ev)
                  : IsTextPreviewShowing() ? m_textView->OnKey (ev)
                                           : m_previewList->OnKey (ev);



    m_context.statusBar.Fill();

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::HandleFocusChanged
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::HandleFocusChanged (ExplorerPane focus)
{
    m_previewList->OnFocusChanged (focus == ExplorerPane::Preview);
    m_hexView->OnFocusChanged     (focus == ExplorerPane::Preview);

    m_previewToolbar->SetFocusIndex (focus == ExplorerPane::PreviewToolbar ? m_previewBarFocus : -1);
    m_find.HandleFocusChanged (focus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetFocusedControl
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * ExplorerPreview::GetFocusedControl() const
{
    return IsHexPreviewShowing()  ? (IDxuiControl *) m_hexView
         : IsTextPreviewShowing() ? (IDxuiControl *) m_textView
                                  : (IDxuiControl *) m_previewList;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ToggleLineAddresses
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::ToggleLineAddresses()
{
    m_context.prefs.lineAddresses = !m_context.prefs.lineAddresses;

    m_textView->SetRows (PreviewText::BuildTextRows (m_context.browser.GetPreview(), m_context.prefs.lineAddresses, PreviewText::GetSourceColors (m_context.theme)));
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::TryGetSelectionDetail
//
//  Bytes selected in the hex view: where they are, and how many. Characters
//  selected in a text preview, the same way.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::TryGetSelectionDetail (std::wstring & outDetail) const
{
    uint64_t  origin = m_hexView->GetOriginAddress();
    size_t    chars  = 0;



    if (IsHexPreviewShowing() && m_hexView->HasSelection())
    {
        outDetail = (m_hexView->GetSelectionCount() == 1)
                  ? std::format (L"${:04X}, 1 byte selected", origin + m_hexView->GetSelectionFirst())
                  : std::format (L"${:04X}{}${:04X}, {} bytes selected",
                                 origin + m_hexView->GetSelectionFirst(),
                                 s_kchEnDash,
                                 origin + m_hexView->GetSelectionLast(),
                                 m_hexView->GetSelectionCount());
        return true;
    }

    if (!IsTextPreviewShowing() || !m_textView->HasSelection())
    {
        return false;
    }

    for (wchar_t ch : m_textView->GetSelectionText())
    {
        chars += (ch != L'\r' && ch != L'\n') ? 1 : 0;
    }

    outDetail = (chars == 1) ? std::wstring (L"1 character selected") : std::format (L"{} characters selected", chars);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetHexEncoding
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexView::TextEncoding ExplorerPreview::GetHexEncoding (bool hostFile, HostCharset charset)
{
    switch (charset)
    {
        case HostCharset::Dos:      return DxuiHexView::TextEncoding::Dos;
        case HostCharset::Windows:  return DxuiHexView::TextEncoding::Windows;
        case HostCharset::Ascii:    return DxuiHexView::TextEncoding::Ascii;
        default:                    break;
    }

    return hostFile ? DxuiHexView::TextEncoding::Ascii : DxuiHexView::TextEncoding::AppleHighBit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetPreviewMonitor
//
//  The monitor chosen in Casso Explorer, or green, the phosphor of Casso's
//  own monitors, when none has been.
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Monitor ExplorerPreview::GetPreviewMonitor() const
{
    for (int index = 0; index < PicturePreview::kMonitorCount; index++)
    {
        if (m_context.prefs.previewMonitor == CassoExplorerPrefs::kMonitorNames[index])
        {
            return (PicturePreview::Monitor) index;
        }
    }

    return PicturePreview::Monitor::Green;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetPreviewCrtDefaults
//
//  The effects of the Casso theme Casso Explorer is in, as Casso extracts it
//  to its own folder; the retro theme's under Light, Dark and Follow Windows.
//  Null, the effects' own defaults, when Casso has not extracted its themes.
//
////////////////////////////////////////////////////////////////////////////////

const ThemeCrtDefaults * ExplorerPreview::GetPreviewCrtDefaults()
{
    bool             casso = ExplorerThemes::IsCassoThemeName (m_context.prefs.theme);
    std::string      name  = casso ? m_context.prefs.theme : std::string (CassoExplorerPrefs::kThemeRetroTerminal);
    Win32FileSystem  fs;
    ThemeLoadError   error;
    std::wstring     dir;
    HRESULT          hr    = S_OK;



    if (name != m_crtThemeName)
    {
        m_crtThemeName  = name;
        m_crtTheme      = LoadedTheme();
        dir             = (PathResolver::GetLocalAppDataDir (L"Casso") / L"Themes" / TextEncoding::Utf8ToWide (name)).wstring();
        hr              = ThemeLoader::Load (fs, dir, m_crtTheme, error);
        m_crtThemeValid = SUCCEEDED (hr);
    }

    return m_crtThemeValid ? &m_crtTheme.crtDefaults : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::UpdateCrtPicture
//
//  The picture preview drawn through Casso's CRT pass on the picture's monitor,
//  at the view's drawn size, again only when that size changed
//  unless `force`. A preview has a single frame, so persistence, which blends a
//  frame into the next, is left out. The picture's glow spills past its
//  edges, into a black margin drawn around it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::UpdateCrtPicture (bool force)
{
    const PreviewContent   & preview  = m_context.browser.GetPreview();
    int                      width    = 0;
    int                      height   = 0;
    int                      margin   = 0;
    CrtResolved              resolved;
    CrtParams                params;
    std::vector<uint32_t>    drawn;
    HRESULT                  hr       = S_OK;
    auto                     found    = m_context.prefs.crtOverrides.end();



    if (preview.kind != PreviewContent::Kind::Picture || preview.bgra.empty() || m_context.window.GetPopupHost() == nullptr)
    {
        return;
    }

    m_picture->GetScaledSize (width, height);

    if (width <= 0 || height <= 0 || (!force && width == m_crtWidth && height == m_crtHeight))
    {
        return;
    }

    found    = m_context.prefs.crtOverrides.find (CassoExplorerPrefs::kMonitorNames[(int) m_context.browser.GetPictureMonitor()]);
    resolved = CrtResolver::Resolve (CrtPresets::GetPreset (PicturePreview::GetPresetIndex (m_context.browser.GetPictureMonitor())), GetPreviewCrtDefaults(),
                                               (found != m_context.prefs.crtOverrides.end()) ? found->second : CrtOverrides());
    params             = MakeCrtParams (resolved, (float) width, (float) height);
    params.persistence = 0.0f;

    hr = m_crtPicture.Render (m_context.window.GetPopupHost()->GetDevice(), m_context.window.GetPopupHost()->GetContext(), preview.bgra, preview.width, preview.height,
                              width, height, params, drawn, margin);

    if (SUCCEEDED (hr))
    {
        m_picture->SetDisplayImage (std::move (drawn), width + 2 * margin, height + 2 * margin, margin);
        m_crtWidth  = width;
        m_crtHeight = height;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::FillPreview
//
//  The preview's kind picks the widget: a picture in the framebuffer view, a
//  catalog as rows under the list's own columns, everything else as lines.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::FillPreview()
{
    const PreviewContent                          & preview  = m_context.browser.GetPreview();
    std::vector<std::vector<DxuiListView::Cell>>    rows;
    bool                                            visible  = m_context.prefs.previewVisible;
    bool                                            picture  = preview.kind == PreviewContent::Kind::Picture && !preview.bgra.empty();
    bool                                            hostFile = preview.kind == PreviewContent::Kind::Hex && !preview.hostPath.empty();
    bool                                            current  = hostFile && m_fileBytes.GetPath() == preview.hostPath && m_fileBytes.IsCurrent();
    bool                                            opened   = hostFile && (current || m_fileBytes.Open (preview.hostPath));
    HostCharset                                     charset  = opened ? m_fileBytes.GetCharset() : preview.hostCharset;
    bool                                            error    = preview.kind == PreviewContent::Kind::Error || (hostFile && !opened);
    bool                                            catalog  = preview.kind == PreviewContent::Kind::Catalog;
    bool                                            hex      = preview.kind == PreviewContent::Kind::Hex && (!preview.bytes.empty() || opened);
    bool                                            disasm   = preview.kind == PreviewContent::Kind::Hex && preview.bytes.empty() && !preview.lines.empty();
    bool                                            columns  = preview.kind == PreviewContent::Kind::Catalog && preview.lines.empty();
    bool                                            program  = preview.kind == PreviewContent::Kind::Listing && !preview.bytes.empty();
    bool                                            forms    = picture && m_context.browser.CanTogglePictureColor();
    bool                                            shell    = preview.kind == PreviewContent::Kind::Shell;
    int                                             mode     = !visible ? CassoExplorerCommands::kPreviewBarNone
                                                             : hex      ? CassoExplorerCommands::kPreviewBarHex
                                                             : disasm   ? CassoExplorerCommands::kPreviewBarHex
                                                             : program  ? CassoExplorerCommands::kPreviewBarListing
                                                             : forms    ? CassoExplorerCommands::kPreviewBarPicture
                                                                        : CassoExplorerCommands::kPreviewBarNone;
    bool                                            blank    = false;



    UpdateShellPreview (preview);

    if (picture)
    {
        m_picture->SetFramebuffer (preview.bgra.data(), preview.width, preview.height);
        UpdateCrtPicture (true);
    }
    else if (!(shell && m_shellPicture))
    {
        m_picture->Clear();
    }

    //  A catalog of any volume but DOS 3.3 or ProDOS arrives as rows under
    //  sortable columns. Everything else shown as text goes to the text view,
    //  where it wraps and selects by character.
    //  An archive's rows look as the file list would show them inside it.
    if (columns)
    {
        m_previewList->SetShowHeader      (true);
        m_previewList->SetMonospace       (!preview.archive);
        m_previewList->SetExplorerDetails (preview.archive);

        //  An archive's rows move and extend by the keys the file list takes.
        m_previewList->SetKeyboardColumnNav (preview.archive);
        m_previewList->SetIconColumn      (preview.archive ? 0 : -1);
        m_previewList->SetFontDip         (preview.archive ? CassoExplorerWindow::kProseFontDip : 0.0f);
        m_previewList->SetRowHeightDip    (kRowHeightDip);
        m_previewList->SetRowHeightPxFn   (preview.archive ? std::function<int (UINT)> (&ExplorerLayout::GetListRowHeightPx) : nullptr);
        m_previewList->SetColumns         (preview.archive ? BrowserPreviewer::GetArchivePreviewColumns() : BrowserCells::GetCatalogPreviewColumns());
        m_previewList->SetTextSelectionColors (!preview.archive);

        for (const CatalogRow & row : preview.rows)
        {
            rows.push_back (preview.archive ? m_context.browser.GetArchivePreviewCells (row) : BrowserCells::ToCatalogPreviewCells (row));
        }
    }

    m_previewArchive = columns && preview.archive;
    m_previewList->SetRows (std::move (rows));
    m_previewList->ResetAutoFit();
    m_previewList->UpdateAutoFitFromRows();
    m_previewList->SetTopRow (0);

    m_textView->SetRows (PreviewText::BuildTextRows (preview, m_context.prefs.lineAddresses, PreviewText::GetSourceColors (m_context.theme)));

    //  A new kind of preview brings its own toolbar, which changes the height
    //  left for the content.
    if (mode != m_previewBarMode)
    {
        m_previewBarMode = mode;

        if (mode != CassoExplorerCommands::kPreviewBarNone)
        {
            m_previewToolbar->SetEntries (m_context.commands.BuildPreviewToolbarEntries (mode, m_find.GetSearchBox(), m_find.GetGoToBox()));
        }

        //  The boxes go with the hex view's toolbar, and their focus with it.
        if (mode != CassoExplorerCommands::kPreviewBarHex && (m_context.input.GetFocusPane() == ExplorerPane::Search || m_context.input.GetFocusPane() == ExplorerPane::GoTo))
        {
            m_context.input.SetFocusPane (ExplorerPane::Preview);
        }

        m_context.layout.RecomputeLayout();
    }

    //  The hex view reads the preview's own bytes where they lie. The source
    //  is re-pointed rather than refilled, so a file of any size costs the
    //  same here as a short one.
    //  A host file is read from the file as the view draws it; an Apple file
    //  is already in memory. Text opens showing only its characters.
    if (!opened)
    {
        m_fileBytes.Close();
    }

    m_previewBytes.SetBytes ((hex && !opened) ? &preview.bytes : nullptr);
    m_hexView->SetSource (!hex ? nullptr : opened ? (const IDxuiHexSource *) &m_fileBytes : (const IDxuiHexSource *) &m_previewBytes);
    m_hexView->SetOriginAddress (preview.origin);
    m_hexView->SetTextEncoding (GetHexEncoding (opened, charset));

    if (hex)
    {
        //  UTF-8 too large to read as text opens on its bytes: the view draws
        //  one character for each byte.
        m_hexView->SetShowValues (!(opened ? charset != HostCharset::None && charset != HostCharset::Utf8 : preview.textFile));
    }

    //  With nothing selected the pane says so, as Explorer's does.
    blank = !m_context.listPane.HasSelection() && !picture && !shell && !error && !hex && !columns && !program;

    m_previewMessage->SetText (blank                   ? std::wstring (kSelectToPreview)
                                        : !error                ? std::wstring()
                                        : (hostFile && !opened) ? PreviewText::FormatPreviewError (L"The file could not be read. " + ChangePrompt::DescribeError (m_fileBytes.GetError()))
                                                       : PreviewText::FormatPreviewError (preview.message));
    m_previewMessage->SetVisible (visible && (error || blank));

    //  Explorer's prompt is smaller body text; an error is muted. Both are
    //  centered in the pane.
    m_previewMessage->SetTextRole (blank ? DxuiTextRole::Body : DxuiTextRole::Muted);
    m_previewMessage->SetFontDip  (blank ? kPromptFontDip : 0.0f);

    m_picture->SetVisible (visible && (picture || (shell && m_shellPicture)));
    m_previewList->SetVisible (visible && columns);
    m_textView->SetVisible (visible && !picture && !shell && !error && !hex && !columns && !blank);
    m_hexView->SetVisible (visible && hex);

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::UpdateShellPreview
//
//  A shell preview is loaded once for its file and reused while the same
//  file is previewed; any other preview, or the pane hidden, unloads it. A
//  load that takes longer than a few seconds is replaced by the bytes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::UpdateShellPreview (const PreviewContent & preview)
{
    bool      shell      = preview.kind == PreviewContent::Kind::Shell && m_context.prefs.previewVisible;
    uint32_t  background = m_context.theme->ContentBackground();
    uint32_t  text       = m_context.theme->Foreground();



    if (!shell)
    {
        if (!m_shellPreviewPath.empty())
        {
            KillTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerShellPreview);
            m_shellPreview.Unload();
            m_shellPreviewPath.clear();
            m_shellPicture = false;
        }

        return;
    }

    if (preview.hostPath == m_shellPreviewPath)
    {
        return;
    }

    m_shellPreviewPath = preview.hostPath;
    m_shellPicture     = false;
    m_picture->Clear();

    m_shellPreview.Load (m_shellPreviewPath, m_previewContent,
                                  RGB ((background >> 16) & 0xFF, (background >> 8) & 0xFF, background & 0xFF),
                                  RGB ((text >> 16) & 0xFF, (text >> 8) & 0xFF, text & 0xFF));

    SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerShellPreview, kShellPreviewTimeoutMs, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::HandleShellPreviewResult
//
//  A handler's window becomes visible over the pane; a thumbnail is drawn in
//  the picture view, fitted; after a failure the bytes are drawn.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::HandleShellPreviewResult()
{
    Win32ShellPreview::Result  result;



    if (m_shellPreviewPath.empty() || !m_shellPreview.TakeResult (result))
    {
        return;
    }

    KillTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerShellPreview);

    switch (result.outcome)
    {
        case Win32ShellPreview::Outcome::Shown:
            m_shellPreview.ShowHost (true);
            break;

        case Win32ShellPreview::Outcome::Picture:
            m_picture->SetFramebuffer (result.bgra.data(), result.width, result.height);
            m_picture->SetVisible (m_context.prefs.previewVisible);
            m_shellPicture = true;
            break;

        case Win32ShellPreview::Outcome::Failed:
        default:
            ShowShellPreviewBytes();
            break;
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ShowShellPreviewBytes
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::ShowShellPreviewBytes()
{
    KillTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerShellPreview);
    m_shellPreview.Unload();
    m_shellPreviewPath.clear();
    m_shellPicture = false;

    m_context.browser.ShowShellPreviewBytes();
    FillPreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::IsHexPreviewShowing
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::IsHexPreviewShowing() const
{
    return m_context.prefs.previewVisible && m_hexView->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::IsTextPreviewShowing
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::IsTextPreviewShowing() const
{
    return m_context.prefs.previewVisible && m_textView->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SetPreviewZoom
//
//  The text and hex previews share one zoom, kept within the range the
//  preferences allow.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SetPreviewZoom (int percent)
{
    int  clamped = (std::max) (CassoExplorerPrefs::kMinPreviewZoom, (std::min) (percent, CassoExplorerPrefs::kMaxPreviewZoom));



    m_context.prefs.previewZoom = clamped;

    m_textView->SetZoom ((float) clamped / 100.0f);
    m_hexView->SetZoom  ((float) clamped / 100.0f);
    m_picture->SetZoom  ((float) clamped / 100.0f);
    UpdateCrtPicture (false);

    m_context.statusBar.Fill();
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SetHexColumns
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SetHexColumns (int columns)
{
    m_hexView->SetColumns (columns);
    m_context.prefs.hexColumns = columns;

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SetHexFormat
//
//  Choosing a format shows the values it applies to.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SetHexFormat (const char * format)
{
    m_context.prefs.hexFormat     = format;
    m_context.prefs.hexShowValues = true;

    m_hexView->SetShowValues  (true);
    m_hexView->SetValueFormat (ParseHexFormat (m_context.prefs.hexFormat));

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SetHexShowValues
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SetHexShowValues (bool show)
{
    m_context.prefs.hexShowValues = show;
    m_hexView->SetShowValues (show);

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ParseHexFormat
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexView::ValueFormat ExplorerPreview::ParseHexFormat (const std::string & name)
{
    if (name == CassoExplorerPrefs::kHexFormatSigned)
    {
        return DxuiHexView::ValueFormat::Signed;
    }

    if (name == CassoExplorerPrefs::kHexFormatUnsigned)
    {
        return DxuiHexView::ValueFormat::Unsigned;
    }

    return DxuiHexView::ValueFormat::Hex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetPreviewStopIndex
//
//  A command's place among the hex view toolbar's stops, or -1 while that
//  toolbar is not showing.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerPreview::GetPreviewStopIndex (int commandId) const
{
    std::vector<int>  ids = CassoExplorerCommands::GetPreviewToolbarCommandIds (true);



    if (m_previewBarMode != CassoExplorerCommands::kPreviewBarHex)
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
//  ExplorerPreview::SetHexGrouping
//
//  A grouping the row cannot divide is refused by the view, and a refused
//  one is not written to the preferences either.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SetHexGrouping (int grouping)
{
    if (!m_hexView->SetGrouping (grouping))
    {
        return;
    }

    m_context.prefs.hexGrouping   = grouping;
    m_context.prefs.hexShowValues = true;

    m_hexView->SetShowValues (true);

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ShowPictureContextMenu
//
//  Color and Monochrome for a double hi-res picture, one of them checked;
//  the choice lasts for this picture.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::ShowPictureContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;
    bool                            color = m_context.browser.GetPreview().pictureMode == PicturePreview::Mode::DoubleHiRes;



    m_context.menus.GetMenuCommands().clear();

    for (bool asColor : { true, false })
    {
        m_context.menus.AddMenuCommand (items, asColor ? L"&Color" : L"&Monochrome", [this, asColor]() { ShowPictureForm (asColor); });

        m_context.menus.GetMenuCommands().back()->radio     = true;
        m_context.menus.GetMenuCommands().back()->isChecked = [color, asColor]() { return color == asColor; };
    }

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ShowPictureForm
//
//  A double hi-res picture drawn in color or as its bits, on the monitor that
//  form is drawn on; the form already drawn is left as it is, so one of the
//  two is always the one drawn.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::ShowPictureForm (bool color)
{
    if ((m_context.browser.GetPreview().pictureMode == PicturePreview::Mode::DoubleHiRes) != color && m_context.browser.TogglePictureColor())
    {
        FillPreview();
        m_context.window.Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ShowTextContextMenu
//
//  Copy and Select all, which reach the text view as the focused control.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::ShowTextContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    for (int id : { (int) CassoExplorerCommands::kCopy, (int) CassoExplorerCommands::kSelectAll })
    {
        std::shared_ptr<const DxuiCommand>  command = m_context.commands.Find (id);

        if (command != nullptr)
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (command));
        }
    }

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::IsArchiveLocation
//
//  Whether the list shows the inside of an archive: a shell folder whose path
//  lies on a lettered drive but is no folder on it.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::IsArchiveLocation() const
{
    const Location &  location = m_context.browser.GetLocation();



    return location.kind == Location::Kind::ShellFolder && location.path.size() > 3 && location.path[1] == L':'
        && m_context.app.fs != nullptr && !m_context.app.fs->DirectoryExists (location.path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SelectMonitor
//
//  The picture preview drawn again on the monitor, in the custom monitor's
//  saved color for that one, which is saved for the next start, apart from
//  Casso's own.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SelectMonitor (PicturePreview::Monitor monitor)
{
    m_context.prefs.previewMonitor = CassoExplorerPrefs::kMonitorNames[(int) monitor];
    m_context.browser.SetPreviewMonitor (monitor, m_context.prefs.previewCustomArgb);
    FillPreview();
    m_context.window.Invalidate();
}
