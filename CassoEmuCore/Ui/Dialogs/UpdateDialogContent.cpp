#include "Pch.h"

#include "Ui/Dialogs/UpdateDialogContent.h"
#include "Core/TextEncoding.h"
#include "Update/UpdateDialogModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::UpdateDialogContent
//
//  The widgets are members, adopted so the panel walks paint and route
//  input to them. Each tab's notes view is adopted by its own scroll panel,
//  which moves and clips it.
//
////////////////////////////////////////////////////////////////////////////////

UpdateDialogContent::UpdateDialogContent()
{
    constexpr float  kOpenerFontDip = 18.0f;



    Adopt (m_opener);
    Adopt (m_header);
    Adopt (m_tabStrip);

    for (NotesPane & pane : m_panes)
    {
        Adopt (pane.scroll);
        pane.scroll.Adopt (pane.view);

        pane.view.SetOnOpenLink ([this] (const std::string & url)
        {
            if (m_onOpenUrl)
            {
                m_onOpenUrl (TextEncoding::Utf8ToWide (url));
            }
        });
    }

    Adopt (m_status);
    Adopt (m_pageLink);

    m_opener.SetTextRole    (DxuiTextRole::Heading);
    m_opener.SetFontWeight  (DxuiFontWeight::Bold);
    m_opener.SetFontSizeDip (kOpenerFontDip);
    m_opener.SetTextAlign   (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_header.SetTextRole   (DxuiTextRole::Heading);
    m_header.SetFontWeight (DxuiFontWeight::Bold);
    m_header.SetTextAlign  (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_status.SetTextRole  (DxuiTextRole::Body);
    m_status.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_pageLink.SetLabel   (UpdateDialogModel::kpszPageLink);
    m_pageLink.SetVariant (DxuiButton::Variant::Link);
    m_pageLink.SetOnClick ([this] ()
    {
        if (m_onOpenUrl && !m_pageUrl.empty())
        {
            m_onOpenUrl (m_pageUrl);
        }
    });

    m_tabStrip.SetOnChange ([this] (int index)
    {
        if (index >= 0 && index < (int) m_tabs.size())
        {
            SelectTab (m_tabs[(size_t) index]);
        }
    });

    SetTabs ({ NotesTab::Changelog });
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetOpener
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetOpener (const std::wstring & opener)
{
    m_opener.SetText (opener);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetHeader
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetHeader (const std::wstring & header)
{
    m_header.SetText (header);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetNotes
//
//  Each tab gets its own notes; the strip shows only when there is more
//  than one tab, and the first, What's new when there is one, is selected.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetNotes (const ReleaseNotes & notes)
{
    std::vector<FormattedLine>  lines;



    for (NotesTab tab : { NotesTab::WhatsNew, NotesTab::Changelog })
    {
        UpdateDialogModel::FormatNotesTab (notes, tab, lines);
        SetPaneLines (tab, std::move (lines));
        lines.clear();
    }

    SetTabs (UpdateDialogModel::GetNotesTabs (notes));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetPaneLines
//
//  New notes start at the top, with the estimated height until the next
//  paint measures them.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetPaneLines (NotesTab tab, std::vector<FormattedLine> lines)
{
    NotesPane &  pane = GetPane (tab);



    pane.view.SetLines (std::move (lines));
    pane.scroll.SetScrollPosPx (0);
    pane.placedHeightPx = 0;

    LayoutNotes (pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetNotesMessage
//
//  A one-line notice in place of the notes, while they load or when they
//  could not be read. It stands alone, with no tab strip.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetNotesMessage (const std::wstring & message)
{
    std::vector<FormattedLine>  lines (1);
    FormattedRun                run;



    run.text = TextEncoding::WideToUtf8 (message);
    lines[0].runs.push_back (run);

    SetPaneLines (NotesTab::Changelog, std::move (lines));
    SetTabs      ({ NotesTab::Changelog });
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetTabs
//
//  The strip's tabs, in order, with the first selected. The strip's row is
//  reserved only while it shows, so a change in whether it shows moves the
//  layout.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetTabs (std::vector<NotesTab> tabs)
{
    bool                            wasShown = UpdateDialogModel::ShowsTabStrip (m_tabs);
    std::vector<DxuiTabStrip::Tab>  strip;
    DxuiTabStrip::Tab               entry;



    m_tabs = std::move (tabs);

    for (NotesTab tab : m_tabs)
    {
        entry.label = UpdateDialogModel::GetTabLabel (tab);
        strip.push_back (entry);
    }

    m_tabStrip.SetTabs    (std::move (strip));
    m_tabStrip.SetVisible (UpdateDialogModel::ShowsTabStrip (m_tabs));

    SelectTab (m_tabs.front());

    if (m_isLaidOut && wasShown != UpdateDialogModel::ShowsTabStrip (m_tabs))
    {
        Layout (GetBounds(), m_scaler);
    }
    else
    {
        LayoutTabStrip();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SelectTab
//
//  Shows the tab's notes where they were left: each pane keeps its own
//  scroll position.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SelectTab (NotesTab tab)
{
    size_t  i = 0;



    m_selected = tab;

    for (i = 0; i < m_tabs.size(); i++)
    {
        if (m_tabs[i] == tab)
        {
            m_tabStrip.SetSelected ((int) i);
        }
    }

    ShowSelectedPane();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::ShowSelectedPane
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::ShowSelectedPane()
{
    GetPane (NotesTab::WhatsNew).scroll.SetVisible  (m_selected == NotesTab::WhatsNew);
    GetPane (NotesTab::Changelog).scroll.SetVisible (m_selected == NotesTab::Changelog);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetStatus
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetStatus (const std::wstring & status, bool isError)
{
    bool  wasShown = m_hasStatus;



    m_status.SetText     (status);
    m_status.SetTextRole (isError ? DxuiTextRole::Error : DxuiTextRole::Body);
    m_hasStatus = !status.empty();

    // The status rows are reserved only while there is a status, so the
    // notes reclaim them otherwise; a change either way moves the layout.
    if (m_isLaidOut && wasShown != m_hasStatus)
    {
        Layout (GetBounds(), m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetNotesImage
//
//  Either tab's notes can show the image, so both are given it.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetNotesImage (const std::string & src, std::shared_ptr<const NotesImage> image)
{
    for (NotesPane & pane : m_panes)
    {
        pane.view.SetImage (src, image);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::GetImageSources
//
//  Every tab's images, each source once, so all of them are fetched as
//  before whichever tab is showing.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> UpdateDialogContent::GetImageSources() const
{
    std::vector<std::string>  sources;



    for (const NotesPane & pane : m_panes)
    {
        for (const std::string & src : pane.view.GetImageSources())
        {
            if (std::find (sources.begin(), sources.end(), src) == sources.end())
            {
                sources.push_back (src);
            }
        }
    }

    return sources;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SetPageUrl
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::SetPageUrl (const std::wstring & url)
{
    m_pageUrl = url;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::SyncNotesHeight
//
//  Lays a tab's notes out again when painting measured a height other than
//  the one they were placed with. True when it did, so the caller repaints.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateDialogContent::SyncNotesHeight()
{
    int   measured  = 0;
    bool  isChanged = false;



    // The header grew or shrank, say when the age remark arrived: lay the
    // whole content out again, which places every pane as well.
    if (m_isLaidOut && m_measuredHeaderLines != m_headerLines)
    {
        Layout (GetBounds(), m_scaler);
        return true;
    }

    for (NotesPane & pane : m_panes)
    {
        measured = pane.view.GetMeasuredHeightPx();

        if (m_isLaidOut && measured > 0 && measured != pane.placedHeightPx)
        {
            LayoutNotes (pane);
            isChanged = true;
        }
    }

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::LayoutTabStrip
//
//  DxuiTabStrip does not lay out its own tabs, so each gets a fixed width,
//  left to right.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::LayoutTabStrip()
{
    constexpr int  kTabWidthDip = 110;



    std::vector<DxuiTabStrip::Tab>  tabs  = m_tabStrip.GetTabs();
    int                             x     = m_tabStripPx.left;
    int                             width = m_scaler.ToPx (kTabWidthDip);
    int                             index = m_tabStrip.GetSelected();



    if (!m_isLaidOut)
    {
        return;
    }

    for (DxuiTabStrip::Tab & tab : tabs)
    {
        tab.rect  = RECT { x, m_tabStripPx.top, x + width, m_tabStripPx.bottom };
        x        += width;
    }

    m_tabStrip.SetTabs     (std::move (tabs));
    m_tabStrip.SetSelected (index);
    m_tabStrip.Layout      (m_tabStripPx, m_scaler);
    m_tabStrip.SetDpi      (m_scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::LayoutNotes
//
//  The notes view is as tall as its content and as wide as the viewport
//  less the scrollbar strip, which the scroll panel needs left free.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::LayoutNotes (NotesPane & pane)
{
    RECT  notesPx   = m_notesViewportPx;
    int   height    = 0;
    int   oldPos    = 0;
    int   oldHeight = 0;



    if (!m_isLaidOut)
    {
        return;
    }

    oldPos              = pane.scroll.GetScrollPosPx();
    oldHeight           = pane.scroll.GetContentHeightPx();
    height              = pane.view.GetEstimatedHeightPx (m_scaler);
    pane.placedHeightPx = pane.view.GetMeasuredHeightPx();

    notesPx.right  -= m_scaler.ToPx (DxuiScrollPanel::kScrollbarWidthDip);
    notesPx.bottom  = notesPx.top + height;

    pane.scroll.SetLineStepPx (m_scaler.ToPx (DxuiScrollPanel::kScrollbarWidthDip * 2));
    pane.scroll.PlaceChild    (pane.view, notesPx);
    pane.scroll.Layout        (m_notesViewportPx, m_scaler);

    // A resize rewraps the notes to a new height; keep the reader the same
    // fraction of the way down rather than snapping back to the top.
    pane.scroll.SetScrollPosPx (ReleaseNotesLayout::ScaleScrollPos (oldPos, oldHeight, pane.scroll.GetContentHeightPx()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::Layout
//
//  Fixed rows at the top and bottom; the notes take what is left.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    constexpr int  kLineDip       = 20;
    constexpr int  kGapDip        = 8;
    constexpr int  kStatusLines   = 2;
    constexpr int  kOpenerDip     = 30;
    constexpr int  kLinkHeightDip = 22;
    constexpr int  kTabStripDip   = 30;
    constexpr int  kBodyGapDip    = 8;    // above the notes, under the strip or the header



    int  line    = scaler.ToPx (kLineDip);
    int  opener  = scaler.ToPx (kOpenerDip);
    int  gap     = scaler.ToPx (kGapDip);
    int  link    = scaler.ToPx (kLinkHeightDip);
    int  strip   = UpdateDialogModel::ShowsTabStrip (m_tabs) ? scaler.ToPx (kTabStripDip) : 0;
    int  y       = boundsPx.top;
    int  bottom  = boundsPx.bottom;
    int  statusH = 0;



    SetBounds (boundsPx);
    m_scaler    = scaler;
    m_isLaidOut = true;

    m_opener.Layout (RECT { boundsPx.left, y, boundsPx.right, y + opener }, scaler);
    y += opener;

    // As many lines as the header wraps to, measured at the last paint, so
    // a header without an age remark leaves no empty line below it.
    m_headerLines = std::max (1, m_measuredHeaderLines);
    m_header.Layout (RECT { boundsPx.left, y, boundsPx.right, y + line * m_headerLines }, scaler);
    y += line * m_headerLines + gap;

    m_tabStripPx = RECT { boundsPx.left, y, boundsPx.right, y + strip };
    LayoutTabStrip();
    y += strip + scaler.ToPx (kBodyGapDip);

    m_pageLink.Layout (RECT { boundsPx.left, bottom - link, boundsPx.right, bottom });
    m_pageLink.SetDpi (scaler.GetDpi());
    bottom -= link;

    statusH = m_hasStatus ? line * kStatusLines : 0;
    m_status.Layout (RECT { boundsPx.left, bottom - statusH, boundsPx.right, bottom }, scaler);
    m_status.SetVisible (m_hasStatus);
    bottom -= statusH + gap;

    m_notesViewportPx = RECT { boundsPx.left, y, boundsPx.right, std::max (bottom, y) };

    for (NotesPane & pane : m_panes)
    {
        LayoutNotes (pane);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent::Paint
//
//  Counts the lines the header wraps to in the face it is drawn in, word
//  by word, before painting; a count other than the one laid out is picked
//  up by SyncNotesHeight on the next tick.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialogContent::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT   bounds = m_header.GetBounds();
    float  sizePx = m_scaler.ToPxf (theme.BodyFont().sizeDip);



    auto  measure = [&] (const std::wstring & words) -> float
    {
        float    width  = 0.0f;
        float    height = 0.0f;
        HRESULT  hr     = text.MeasureStringWeighted (words.c_str(), sizePx, DxuiTheme::kBodyFace, DxuiFontWeight::Bold, width, height);

        IGNORE_RETURN_VALUE (hr, S_OK);
        return width;
    };

    if (m_isLaidOut)
    {
        m_measuredHeaderLines = UpdateDialogModel::CountWrappedLines (m_header.GetText(), (float) (bounds.right - bounds.left), measure);
    }

    DxuiPanel::Paint (painter, text, theme);
}