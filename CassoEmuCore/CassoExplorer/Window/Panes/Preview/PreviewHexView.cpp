#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewHexView.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "Devices/Disk/ChangePrompt.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::PreviewHexView
//
////////////////////////////////////////////////////////////////////////////////

PreviewHexView::PreviewHexView (ExplorerPreviewManager & manager, ExplorerContext & context)
    : PreviewView (manager, context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::Create
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::Create()
{
    CassoExplorerNamedControl<DxuiHexView>  * hex = m_context.window.CreateChild<CassoExplorerNamedControl<DxuiHexView>>();



    hex->SetAccessibleName (L"Bytes");
    m_hex = hex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::Configure
//
//  The bytes are Apple text in the column on the right, which is what the
//  files here have; the grouping is the user's, kept between runs.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::Configure()
{
    const CassoExplorerPrefs  & prefs   = m_context.prefs;
    bool                        grouped = false;



    m_hex->SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
    m_hex->SetOwnerWindow  (m_context.window.GetHwnd());
    m_hex->SetPaddingDip   (ExplorerMetrics::kHexPaddingDip);
    m_hex->SetColumns      (prefs.hexColumns);
    m_hex->SetValueFormat  (ParseHexFormat (prefs.hexFormat));
    m_hex->SetShowValues   (prefs.hexShowValues);
    m_hex->SetBreakLines   (true);
    ExplorerTextStyles::Apply (*m_hex, ExplorerTextStyles::kPreviewText);
    m_hex->SetZoom         ((float) prefs.previewZoom / 100.0f);

    grouped = m_hex->SetGrouping (prefs.hexGrouping);
    IGNORE_RETURN_VALUE (grouped, true);

    m_hex->SetOnSelectionChanged ([this] () { m_context.statusBar.Fill(); });

    //  The hex view's menu is its own; Go to is the browser's, added to it,
    //  and the view's choices are kept for the next run.
    m_hex->SetOnBuildContextMenu ([this] (std::vector<DxuiPopupMenuItem> & items)
    {
        std::shared_ptr<const DxuiCommand>  goTo = m_context.commands.Find (CassoExplorerCommands::kGoToOffset);

        if (goTo != nullptr)
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (goTo));
        }
    });

    m_hex->SetOnSettingsChanged ([this]()
    {
        CassoExplorerPrefs &  settings = m_context.prefs;

        settings.hexShowValues = m_hex->IsShowingValues();
        settings.hexGrouping   = m_hex->GetGrouping();
        settings.hexColumns    = m_hex->GetColumns();
        settings.hexFormat     = (m_hex->GetValueFormat() == DxuiHexView::ValueFormat::Signed)   ? CassoExplorerPrefs::kHexFormatSigned
                               : (m_hex->GetValueFormat() == DxuiHexView::ValueFormat::Unsigned) ? CassoExplorerPrefs::kHexFormatUnsigned
                                                                                                  : CassoExplorerPrefs::kHexFormatHex;
        m_context.window.Invalidate();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::Layout (const RECT & content)
{
    m_hex->Layout (content, m_context.scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::Hide
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::Hide()
{
    m_hex->SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::UpdateScrollbarHover
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewHexView::UpdateScrollbarHover (POINT at)
{
    return m_hex->SetScrollbarHover (at);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewHexView::TickScrollbars (int64_t nowMs)
{
    return m_hex->TickScrollbars (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::OpenHostFile
//
//  A host file's bytes are read from the file; the file stays open while it
//  is previewed. Returns whether the file is open, and its character set,
//  or the preview's for any other bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewHexView::OpenHostFile (const PreviewContent & preview, HostCharset & outCharset)
{
    bool  hostFile = preview.kind == PreviewContent::Kind::Hex && !preview.hostPath.empty();
    bool  current  = hostFile && m_fileBytes.GetPath() == preview.hostPath && m_fileBytes.IsCurrent();
    bool  opened   = hostFile && (current || m_fileBytes.Open (preview.hostPath));



    outCharset = opened ? m_fileBytes.GetCharset() : preview.hostCharset;

    return opened;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::Fill
//
//  The view reads the preview's bytes where they lie. The source is
//  re-pointed rather than refilled, so a file of any size costs the same
//  here as a short one. Text opens showing only its characters.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::Fill (const PreviewContent & preview, bool hex, bool opened, HostCharset charset)
{
    if (!opened)
    {
        m_fileBytes.Close();
    }

    m_previewBytes.SetBytes ((hex && !opened) ? &preview.bytes : nullptr);
    m_hex->SetSource (!hex ? nullptr : opened ? (const IDxuiHexSource *) &m_fileBytes : (const IDxuiHexSource *) &m_previewBytes);
    m_hex->SetOriginAddress (preview.origin);
    m_hex->SetTextEncoding (GetHexEncoding (opened, charset));

    if (hex)
    {
        //  UTF-8 too large to read as text opens on its bytes: the view draws
        //  one character for each byte.
        m_hex->SetShowValues (!(opened ? charset != HostCharset::None && charset != HostCharset::Utf8 : preview.textFile));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::DescribeFileError
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewHexView::DescribeFileError() const
{
    return ChangePrompt::DescribeError (m_fileBytes.GetError());
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::SetVisible
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::SetVisible (bool visible)
{
    m_hex->SetVisible (visible);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::SetZoom
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::SetZoom (float zoom)
{
    m_hex->SetZoom (zoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::IsShowing
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewHexView::IsShowing() const
{
    return m_context.prefs.previewVisible && m_hex->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::HandleCapturedMouse
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewHexView::HandleCapturedMouse (const DxuiMouseEvent & ev)
{
    if (!m_hex->IsDragging())
    {
        return false;
    }

    m_hex->OnMouse (ev);
    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::HandleMouse
//
//  DxuiHexView takes points in the same coordinates as its bounds, so the
//  event is passed unchanged, not converted to widget-local coordinates as
//  the older list views require.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewHexView::HandleMouse (const DxuiMouseEvent & ev)
{
    if (!m_hex->IsVisible() || !ExplorerLayout::Contains (m_hex->GetBounds(), ev.positionDip))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
    }

    m_hex->OnMouse (ev);
    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::HandleTabKey
//
//  The view's two columns are stops of their own inside the pane: Tab moves
//  between them while it has one left, and only then does the walk move on
//  to the next pane.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewHexView::HandleTabKey (const DxuiKeyEvent & ev)
{
    return IsShowing() && m_hex->OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::HandleFocusEntered
//
//  A Tab into the pane starts at the column the walk's direction calls for.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::HandleFocusEntered (bool forward)
{
    if (IsShowing())
    {
        m_hex->OnFocusEntered (forward);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::GetCursorForPoint
//
//  The bytes select as text does, so the cursor over them is the I-beam,
//  except over the view's scrollbars.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR PreviewHexView::GetCursorForPoint (POINT clientPx) const
{
    bool  over = m_hex->IsVisible() && ExplorerLayout::Contains (m_hex->GetBounds(), clientPx) && !m_hex->IsOverScrollbar (clientPx);



    return over ? IDC_IBEAM : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::TryGetScrollbarMenu
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewHexView::TryGetScrollbarMenu (const DxuiMouseEvent & ev, std::vector<DxuiPopupMenuItem> & outItems) const
{
    if (!m_hex->IsVisible() || !ExplorerLayout::Contains (m_hex->GetBounds(), ev.positionDip))
    {
        return false;
    }

    outItems = m_hex->GetScrollbarMenu (ev.positionDip);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::TryGetSelectionDetail
//
//  Bytes selected: where they are, and how many.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewHexView::TryGetSelectionDetail (std::wstring & outDetail) const
{
    uint64_t  origin = m_hex->GetOriginAddress();



    if (!IsShowing() || !m_hex->HasSelection())
    {
        return false;
    }

    outDetail = (m_hex->GetSelectionCount() == 1)
              ? std::format (L"${:04X}, 1 byte selected", origin + m_hex->GetSelectionFirst())
              : std::format (L"${:04X}{}${:04X}, {} bytes selected",
                             origin + m_hex->GetSelectionFirst(),
                             s_kchEnDash,
                             origin + m_hex->GetSelectionLast(),
                             m_hex->GetSelectionCount());
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::SetGrouping
//
//  A grouping the row cannot divide is refused by the view, and a refused
//  one is not written to the preferences either.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::SetGrouping (int grouping)
{
    if (!m_hex->SetGrouping (grouping))
    {
        return;
    }

    m_context.prefs.hexGrouping   = grouping;
    m_context.prefs.hexShowValues = true;

    m_hex->SetShowValues (true);

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::SetColumns
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::SetColumns (int columns)
{
    m_hex->SetColumns (columns);
    m_context.prefs.hexColumns = columns;

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::SetFormat
//
//  Choosing a format shows the values it applies to.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::SetFormat (const char * format)
{
    m_context.prefs.hexFormat     = format;
    m_context.prefs.hexShowValues = true;

    m_hex->SetShowValues  (true);
    m_hex->SetValueFormat (ParseHexFormat (m_context.prefs.hexFormat));

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::SetShowValues
//
////////////////////////////////////////////////////////////////////////////////

void PreviewHexView::SetShowValues (bool show)
{
    m_context.prefs.hexShowValues = show;
    m_hex->SetShowValues (show);

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewHexView::GetHexEncoding
//
//  The character column for a preview's bytes: the host file's character
//  set, or Apple text for a file from an image.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexView::TextEncoding PreviewHexView::GetHexEncoding (bool hostFile, HostCharset charset)
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
//  PreviewHexView::ParseHexFormat
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexView::ValueFormat PreviewHexView::ParseHexFormat (const std::string & name)
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
